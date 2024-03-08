#pragma once

// <iostream> and <sstream> should be included also, because debug version on thread_pool uses them.
// They can be included before #include "thread_pool".
#include <iostream>
#include <sstream>

#include "concurrent_queue.h"
#include <vector>
#include <functional>
#include <chrono>

template <bool debug = false>
class thread_pool {
public:
	inline thread_pool(const std::size_t interval_seconds = 30);
	inline ~thread_pool() { terminate(); }

public:
	inline void initialize(const std::size_t worker_count);
	inline void terminate(const bool immediately = false);

	inline void set_paused(const bool paused);
	inline bool is_paused() const;

	inline bool accepting()			const;
	inline bool accepting_unsafe()	const;
	inline bool working()			const;
	inline bool working_unsafe()	const;

	template <typename task_t, typename... arguments>
	inline void add_task(task_t&& task, arguments&&... parameters);

public:
	inline thread_pool(const thread_pool& other)			= delete;
	inline thread_pool(thread_pool&& other)					= delete;
	inline thread_pool& operator=(const thread_pool& rhs)	= delete;
	inline thread_pool& operator=(thread_pool&& rhs)		= delete;

private:
	inline void routine();

	mutable read_write_lock					m_rw_lock;
	mutable std::condition_variable_any		m_task_waiter;
	std::vector<std::thread>				m_workers;

	concurrent_queue<std::function<void()>>	m_tasks;

	bool m_initialized	= false;
	bool m_terminated	= false;
	bool m_paused		= false;
	
private:
	// IMPORTANT NOTE: this member field represents current readiness state of a thread pool to accept new tasks from the user code and to add them to the internal task queue.
	// =======================================================================================================================================================================
	// When m_accepting_new_tasks is true, thread pool accepts new tasks from the user code, 
	// BUT the thread pool itself does NOT start executing tasks from the internal task queue (worker threads stop).
	// -----------------------------------------------------------------------------------------------------------------------------------------------------------------------
	// When m_accepting_new_tasks is false, thread pool rejects new tasks from the user code, 
	// BUT the thread pool itself IS working internally by STARTING executing tasks from the internal task queue (worker threads execute tasks).
	bool						m_accepting_new_tasks	= false;
	std::size_t					m_active_tasks_counter	= 0;
	
private:
	std::thread					m_timer_thread;
	std::condition_variable_any	m_timer_waiter;
	const std::size_t			m_interval_seconds;

	inline void timer_function();

private:
	std::chrono::milliseconds	m_total_waiting_time	= std::chrono::milliseconds::zero();
	std::chrono::milliseconds	m_total_completing_time	= std::chrono::milliseconds::zero();
	std::size_t					m_completed_tasks		= 0;

	std::size_t					m_sum_of_queue_lengths	= 0;
	std::size_t					m_queue_updates_amount	= 0;
};


template<bool debug>
inline thread_pool<debug>::thread_pool(const std::size_t interval_seconds) : m_interval_seconds(interval_seconds) {}

template <bool debug>
inline void thread_pool<debug>::initialize(const std::size_t worker_count) {
	write_lock w_lock(m_rw_lock);

	if (m_initialized || m_terminated) {
		return;
	}

	if constexpr (debug) {
		std::ostringstream ss; ss << "TP " << this << ": INITIALIZING.\n";
		std::clog << ss.str();
	}

	m_workers.reserve(worker_count);
	for (size_t id = 0; id < worker_count; ++id) {
		m_workers.emplace_back(&thread_pool::routine, this);
	}
	
	bool workers_not_empty = !m_workers.empty();
	m_initialized = workers_not_empty;
	m_accepting_new_tasks = workers_not_empty;

	if (workers_not_empty) {
		m_timer_thread = std::thread(&thread_pool::timer_function, this);

		if constexpr (debug) {
			std::ostringstream ss; ss << "TP " << this << (m_initialized ? ": INITIALIZED." : ": NOT INITIALIZED.") << (m_accepting_new_tasks ? " ACCEPTING" : " REJECTING") << " new tasks.\n";
			std::clog << ss.str();
		}
	}
	else if constexpr (debug) {
		std::ostringstream ss; ss << "TP " << this << ": FAILED TO INITIALIZE: incorrect amount of worker threads.\n";
		std::clog << ss.str();
	}
}

template <bool debug>
inline void thread_pool<debug>::terminate(const bool immediately) {
	{
		write_lock w_lock(m_rw_lock);

		if (working_unsafe()) {
			m_terminated = true;
			m_accepting_new_tasks = false;
			m_paused = false;

			if (immediately) {
				m_tasks.clear();
			}

			if constexpr (debug) {
				if (immediately) {
					std::ostringstream ss; ss << "TP " << this << ": TERMINATING immediately - ending current active tasks and deleting existing tasks from the internal queue. Rejecting any new tasks.\n";
					std::clog << ss.str();
				}
				else {
					std::ostringstream ss; ss << "TP " << this << ": TERMINATING - forcing worker threads to start executing existing tasks. Rejecting any new tasks.\n";
					std::clog << ss.str();
				}
			}
		}
		else {
			return;
		}
	}

	m_task_waiter.notify_all();

	m_timer_thread.join();

	for (std::thread& worker : m_workers) {
		worker.join();
	}

	write_lock w_lock(m_rw_lock);

	const std::size_t workers_amount = m_workers.size();
	m_workers.clear();
	m_terminated = false;
	m_initialized = false;
	m_accepting_new_tasks = false;
	m_paused = false;

	if constexpr (debug) {
		std::ostringstream ss; ss << "TP " << this << ": TERMINATED.\n";

		if (workers_amount > 0 && m_completed_tasks > 0 && m_queue_updates_amount > 0) {
			ss << "TP " << this << ": STATISTICS:\n"
				<< "\tAVERAGE WAITING TIME:    " << m_total_waiting_time.count() / m_completed_tasks / workers_amount << " ms.\n"
				<< "\tAVERAGE COMPLETING TIME: " << m_total_completing_time.count() / m_completed_tasks << " ms.\n"
				<< "\tAVERAGE QUEUE LENGTH:    " << m_sum_of_queue_lengths / m_queue_updates_amount << ".\n";
		}

		std::clog << ss.str();
	}
}

template <bool debug>
inline void thread_pool<debug>::routine() {
	while (true) {
		bool task_accquiered = false;
		std::function<void()> task;
		
		std::chrono::milliseconds waiting_time;

		{
			write_lock w_lock(m_rw_lock);

			auto wait_condition = [this, &task_accquiered, &task] {
				if (m_accepting_new_tasks || m_paused) {
					return false;
				}

				task_accquiered = m_tasks.pop(task);
				return m_terminated || task_accquiered;
			};

			auto before_waiting = std::chrono::high_resolution_clock::now();

			m_task_waiter.wait(w_lock, wait_condition);
			
			auto after_waiting  = std::chrono::high_resolution_clock::now();
			waiting_time = std::chrono::duration_cast<std::chrono::milliseconds>(after_waiting - before_waiting);

			if (m_terminated && !task_accquiered) {
				return;
			}

			++m_active_tasks_counter;

			m_sum_of_queue_lengths += m_tasks.size();
			++m_queue_updates_amount;
		}

		auto before_completing = std::chrono::high_resolution_clock::now();

		task();

		auto after_completing = std::chrono::high_resolution_clock::now();
		auto completing_time  = std::chrono::duration_cast<std::chrono::milliseconds>(after_completing - before_completing);

		{
			write_lock w_lock(m_rw_lock);
			--m_active_tasks_counter;

			m_total_waiting_time	+= waiting_time;
			m_total_completing_time	+= completing_time;
			++m_completed_tasks;
		}

		m_timer_waiter.notify_one();
	}
}

template<bool debug>
inline void thread_pool<debug>::set_paused(const bool paused) {
	write_lock w_lock(m_rw_lock);
	
	if (working_unsafe()) {
		if constexpr (debug) {
			std::ostringstream ss; ss << "TP " << this << ": SET PAUSED: " << (paused ? "TRUE" : "FALSE") << ". Previous value: " << (m_paused ? "TRUE" : "FALSE") << ".\n";
			std::clog << ss.str();
		}

		m_paused = paused;
	}
}

template<bool debug>
inline bool thread_pool<debug>::is_paused() const {
	read_lock r_lock(m_rw_lock);
	return m_paused;
}

template <bool debug>
inline bool thread_pool<debug>::accepting() const {
	read_lock r_lock(m_rw_lock);
	return accepting_unsafe();
}

template <bool debug>
inline bool thread_pool<debug>::accepting_unsafe() const {
	return m_accepting_new_tasks;
}

template <bool debug>
inline bool thread_pool<debug>::working() const {
	read_lock r_lock(m_rw_lock);
	return working_unsafe();
}

template <bool debug>
inline bool thread_pool<debug>::working_unsafe() const {
	return m_initialized && !m_terminated;
}

template <bool debug>
template <typename task_t, typename... arguments>
inline void thread_pool<debug>::add_task(task_t&& task, arguments&&... parameters) {
	{
		read_lock r_lock(m_rw_lock);

		if ((!accepting_unsafe() || (m_active_tasks_counter > 0)) && !m_paused || !working_unsafe()) {
			if constexpr (debug) {
				std::ostringstream ss; ss << "TP " << this << ": REJECTING new task - " << typeid(task).name() << ".\n";
				std::clog << ss.str();
			}
			return;
		}

		if constexpr (debug) {
			std::ostringstream ss; ss << "TP " << this << ": ACCEPTING new task - " << typeid(task).name() << ".\n";
			std::clog << ss.str();
		}

		m_sum_of_queue_lengths += m_tasks.size() + 1;
		++m_queue_updates_amount;
	}
	
	auto bind = std::bind(std::forward<task_t>(task), std::forward<arguments>(parameters)...);

	m_tasks.emplace(bind);
	m_task_waiter.notify_one();
}

template<bool debug>
inline void thread_pool<debug>::timer_function() {
	while (true) {
		std::this_thread::sleep_for(std::chrono::seconds(m_interval_seconds));

		write_lock w_lock(m_rw_lock);

		if (m_terminated) {
			m_accepting_new_tasks = false;
			return;
		}

		bool prev_state = m_accepting_new_tasks;
		m_accepting_new_tasks = !m_accepting_new_tasks;

		if (m_paused) {
			m_accepting_new_tasks = true;
		}

		if constexpr (debug) {
			if (m_accepting_new_tasks != prev_state) {
				std::ostringstream ss; ss << "TP " << this << ": " << (m_accepting_new_tasks ? "WANTS to START accepting new tasks - TP is NOT starting to execute existing tasks." : "is NOT ACCEPTING new tasks.") << "\n";
				std::clog << ss.str();
			}
		}

		if (!m_accepting_new_tasks) {
			w_lock.unlock();
			m_task_waiter.notify_all();
		}
		else {
			m_timer_waiter.wait(w_lock, [this] {
				return m_active_tasks_counter == 0;
			});

			if constexpr (debug) {
				if (m_accepting_new_tasks != prev_state) {
					std::ostringstream ss; ss << "TP " << this << ": is ACTUALLY ACCEPTING new tasks.\n";
					std::clog << ss.str();
				}
			}
		}
	}
}