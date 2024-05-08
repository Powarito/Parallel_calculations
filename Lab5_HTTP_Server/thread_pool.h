#pragma once

#include <vector>
#include <functional>

#include "concurrent_queue.h"

class thread_pool {
public:
	inline thread_pool() = default;
	inline ~thread_pool() { terminate(); }

public:
	inline void initialize(const std::size_t worker_count);
	inline void terminate(const bool immediately = false);

	inline void set_paused(const bool paused);
	inline bool is_paused() const;

	inline bool working()			const;
	inline bool working_unsafe()	const;

	template <typename task_t, typename... arguments>
	inline void add_task(task_t&& task, arguments&&... parameters);

public:
	inline thread_pool(const thread_pool& other) = delete;
	inline thread_pool(thread_pool&& other) = delete;
	inline thread_pool& operator=(const thread_pool& rhs) = delete;
	inline thread_pool& operator=(thread_pool&& rhs) = delete;

private:
	inline void routine();

	mutable read_write_lock					m_rw_lock;
	mutable std::condition_variable_any		m_task_waiter;
	std::vector<std::thread>				m_workers;

	concurrent_queue<std::function<void()>>	m_tasks;

	bool m_initialized = false;
	bool m_terminated = false;
	bool m_paused = false;
};


inline void thread_pool::initialize(const std::size_t worker_count) {
	write_lock w_lock(m_rw_lock);

	if (m_initialized || m_terminated) {
		return;
	}

	m_workers.reserve(worker_count);
	for (size_t id = 0; id < worker_count; ++id) {
		m_workers.emplace_back(&thread_pool::routine, this);
	}

	m_initialized = !m_workers.empty();
}

inline void thread_pool::terminate(const bool immediately) {
	{
		write_lock w_lock(m_rw_lock);

		if (working_unsafe()) {
			m_terminated = true;
			m_paused = false;

			if (immediately) {
				m_tasks.clear();
			}
		}
		else {
			return;
		}
	}

	m_task_waiter.notify_all();

	for (std::thread& worker : m_workers) {
		worker.join();
	}

	write_lock w_lock(m_rw_lock);

	m_workers.clear();
	m_terminated = false;
	m_initialized = false;
	m_paused = false;
}

inline void thread_pool::routine() {
	while (true) {
		bool task_accquiered = false;
		std::function<void()> task;

		std::chrono::milliseconds waiting_time;

		{
			write_lock w_lock(m_rw_lock);

			auto wait_condition = [this, &task_accquiered, &task] {
				if (m_paused) {
					return false;
				}

				task_accquiered = m_tasks.pop(task);
				return m_terminated || task_accquiered;
			};

			m_task_waiter.wait(w_lock, wait_condition);

			if (m_terminated && !task_accquiered) {
				return;
			}
		}

		task();
	}
}

inline void thread_pool::set_paused(const bool paused) {
	write_lock w_lock(m_rw_lock);

	if (working_unsafe()) {
		m_paused = paused;
		
		if (!m_paused) {
			m_task_waiter.notify_all();
		}
	}
}

inline bool thread_pool::is_paused() const {
	read_lock r_lock(m_rw_lock);
	return m_paused;
}

inline bool thread_pool::working() const {
	read_lock r_lock(m_rw_lock);
	return working_unsafe();
}

inline bool thread_pool::working_unsafe() const {
	return m_initialized && !m_terminated;
}

template <typename task_t, typename... arguments>
inline void thread_pool::add_task(task_t&& task, arguments&&... parameters) {
	{
		read_lock r_lock(m_rw_lock);

		if (!working_unsafe()) {
			return;
		}
	}

	auto bind = std::bind(std::forward<task_t>(task), std::forward<arguments>(parameters)...);

	m_tasks.emplace(bind);
	m_task_waiter.notify_one();
}