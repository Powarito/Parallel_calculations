#pragma once

#include <queue>
#include <thread>
#include <shared_mutex>

using read_write_lock = std::shared_mutex;
using read_lock = std::shared_lock<read_write_lock>;
using write_lock = std::unique_lock<read_write_lock>;

template <typename type>
class concurrent_queue {
	using concurrent_queue_implementation = std::queue<type>;

public:
	inline concurrent_queue() = default;
	inline ~concurrent_queue() { clear(); }

public:
	inline bool empty() const;
	inline std::size_t size() const;

	inline void clear();
	inline bool pop(type& value);
	inline bool pop();

	template <typename... arguments>
	inline void emplace(arguments&&... parameters);

public:
	inline concurrent_queue(const concurrent_queue& other) = delete;
	inline concurrent_queue(concurrent_queue&& other) = delete;
	inline concurrent_queue& operator=(const concurrent_queue& rhs) = delete;
	inline concurrent_queue& operator=(concurrent_queue&& rhs) = delete;

private:
	mutable read_write_lock m_rw_lock;
	concurrent_queue_implementation m_queue;
};


template <typename type>
bool concurrent_queue<type>::empty() const  {
	read_lock r_lock(m_rw_lock);
	return m_queue.empty();
}

template <typename type>
std::size_t concurrent_queue<type>::size() const {
	read_lock r_lock(m_rw_lock);
	return m_queue.size();
}

template <typename type>
void concurrent_queue<type>::clear() {
	write_lock w_lock(m_rw_lock);

	while (!m_queue.empty()) {
		m_queue.pop();
	}
}

template <typename type>
bool concurrent_queue<type>::pop(type& value) {
	write_lock w_lock(m_rw_lock);

	if (m_queue.empty()) {
		return false;
	}
	else {
		value = std::move(m_queue.front());
		m_queue.pop();
		return true;
	}
}

template<typename type>
bool concurrent_queue<type>::pop() {
	write_lock w_lock(m_rw_lock);

	if (m_queue.empty()) {
		return false;
	}
	else {
		m_queue.pop();
		return true;
	}
}

template <typename type>
template <typename... arguments>
void concurrent_queue<type>::emplace(arguments&&... parameters) {
	write_lock w_lock(m_rw_lock);
	m_queue.emplace(std::forward<arguments>(parameters)...);
}