#pragma once

#include <thread>
#include <vector>
#include <queue>
#include <atomic>
#include <mutex>

// ===== Atomic algorithm =====

template <typename T>
inline void resursive_change_max_atomic(const T& desired, const std::size_t start_from_index, const std::size_t elem_num, std::vector<std::atomic<T>>& vec_of_max_atomics) {
	for (std::size_t i = start_from_index; i < elem_num; ++i) {
		T contained = vec_of_max_atomics[i].load();
		bool changed = true;

		if (desired > contained) {
			while (!vec_of_max_atomics[i].compare_exchange_weak(contained, desired)) {
				if (desired <= contained) {
					changed = false;
					break;
				}
			}

			resursive_change_max_atomic((changed ? contained : desired), i + 1, elem_num, vec_of_max_atomics);
			break;
		}
	}
}

// Find elem_num max elements in the given vector range and store them in the given vector of atomics (vec_of_max_atomics).
// The size of vec_of_max_atomics should not be less than elem_num (ideally, it should be equal according to the logic).
// The number of elements in the given vector range should not be less than elem_num. Otherwise, it is undefined behavior.
template <typename T>
inline void find_n_max_elem_in_vector_range_atomic(const typename std::vector<T>::const_iterator it_begin, const typename std::vector<T>::const_iterator it_end, const std::size_t elem_num, std::vector<std::atomic<T>>& vec_of_max_atomics) {
	// Build a priority queue from the given vector iterators using a special constructor for this. The time complexity of this method is effective - O(N).
	std::priority_queue<T> max_heap(it_begin, it_end);

	for (std::size_t i = 0; i < elem_num; ++i) {
		T max_heap_top = max_heap.top();
		max_heap.pop();

		// A little optimization: since we pop values from a max heap, it means that the next values would not be greater then the previous ones.
		// And because of that, no matter what result would be with vec_of_max_atomics[i] for the previous value, 
		// for the next value it always would fail with the same vec_of_max_atomics[i] (contained value in the vec_of_max_atomics[i] is never changed).
		// So we don't need to iterate through all the vec_of_max_atomics vector from the very start again and again, but start from vec_of_max_atomics[i].
		resursive_change_max_atomic(max_heap_top, i, elem_num, vec_of_max_atomics);
	}
}

// Find elem_num max elements in the given vector and store them in the given vector of atomics (vec_of_max_atomics).
// The size of vec_of_max_atomics should not be less than elem_num (ideally, it should be equal according to the logic).
// The number of elements in the given vector range should not be less than elem_num. Otherwise, it is undefined behavior.
// The algorithm is performed in thread_count threads.
template <typename T>
inline void find_n_max_elem_in_vector_atomic(const std::vector<T>& vec, const std::size_t elem_num, const std::size_t thread_count, std::vector<std::atomic<T>>& vec_of_max_atomics) {
	const std::size_t vec_size = vec.size();

	const std::size_t step = vec_size / thread_count;
	const std::size_t remainder = (vec_size - step * thread_count == 0 ? 0 : vec_size - step * (thread_count - 1));

	std::vector<std::thread> worker_threads(thread_count);

	for (std::size_t i = 0; i < thread_count - 1; ++i) {
		worker_threads[i] = std::thread(find_n_max_elem_in_vector_range_atomic<T>, vec.cbegin() + i * step, vec.cbegin() + (i + 1) * step, elem_num, std::ref(vec_of_max_atomics));
	}
	worker_threads[thread_count - 1] = std::thread(find_n_max_elem_in_vector_range_atomic<T>, vec.cend() - (remainder == 0 ? step : remainder), vec.cend(), elem_num, std::ref(vec_of_max_atomics));

	for (auto& worker_thread : worker_threads) {
		worker_thread.join();
	}
}

// ===== Mutex algorithm =====

template <typename T>
inline void resursive_change_max_mutex(const T& desired, const std::size_t start_from_index, const std::size_t elem_num, std::vector<T>& vec_of_max_values, std::mutex& max_values_mutex) {
	max_values_mutex.lock();

	for (std::size_t i = start_from_index; i < elem_num; ++i) {
		T max_value = vec_of_max_values[i];

		if (desired > max_value) {
			vec_of_max_values[i] = desired;
		
			max_values_mutex.unlock();
			resursive_change_max_mutex(max_value, i + 1, elem_num, vec_of_max_values, max_values_mutex);
	
			return;
		}
	}

	max_values_mutex.unlock();
}

// Find elem_num max elements in the given vector range and store them in the given vector of max values (vec_of_max_values).
// The size of vec_of_max_values should not be less than elem_num (ideally, it should be equal according to the logic).
// The number of elements in the given vector range should not be less than elem_num. Otherwise, it is undefined behavior.
template <typename T>
inline void find_n_max_elem_in_vector_range_mutex(const typename std::vector<T>::const_iterator it_begin, const typename std::vector<T>::const_iterator it_end, const std::size_t elem_num, std::vector<T>& vec_of_max_values, std::mutex& max_values_mutex) {
	// Build a priority queue from the given vector iterators using a special constructor for this. The time complexity of this method is effective - O(N).
	std::priority_queue<T> max_heap(it_begin, it_end);

	for (std::size_t i = 0; i < elem_num; ++i) {
		T max_heap_top = max_heap.top();
		max_heap.pop();

		// A little optimization: since we pop values from a max heap, it means that the next values would not be greater then the previous ones.
		// And because of that, no matter what result would be with vec_of_max_values[i] for the previous value, 
		// for the next value it always would fail with the same vec_of_max_values[i] (contained value in the vec_of_max_values[i] is never changed).
		// So we don't need to iterate through all the vec_of_max_values vector from the very start again and again, but start from vec_of_max_values[i].
		resursive_change_max_mutex(max_heap_top, i, elem_num, vec_of_max_values, max_values_mutex);
	}
}

// Find elem_num max elements in the given vector and store them in the given vector of max values (vec_of_max_values).
// The size of vec_of_max_values should not be less than elem_num (ideally, it should be equal according to the logic).
// The number of elements in the given vector range should not be less than elem_num. Otherwise, it is undefined behavior.
// The algorithm is performed in thread_count threads.
template <typename T>
inline void find_n_max_elem_in_vector_mutex(const std::vector<T>& vec, const std::size_t elem_num, const std::size_t thread_count, std::vector<T>& vec_of_max_values, std::mutex& max_values_mutex) {
	const std::size_t vec_size = vec.size();

	const std::size_t step = vec_size / thread_count;
	const std::size_t remainder = (vec_size - step * thread_count == 0 ? 0 : vec_size - step * (thread_count - 1));

	std::vector<std::thread> worker_threads(thread_count);

	for (std::size_t i = 0; i < thread_count - 1; ++i) {
		worker_threads[i] = std::thread(find_n_max_elem_in_vector_range_mutex<T>, vec.cbegin() + i * step, vec.cbegin() + (i + 1) * step, elem_num, std::ref(vec_of_max_values), std::ref(max_values_mutex));
	}
	worker_threads[thread_count - 1] = std::thread(find_n_max_elem_in_vector_range_mutex<T>, vec.cend() - (remainder == 0 ? step : remainder), vec.cend(), elem_num, std::ref(vec_of_max_values), std::ref(max_values_mutex));

	for (auto& worker_thread : worker_threads) {
		worker_thread.join();
	}
}

// ===== Singlethreaded algorithm =====

// Find elem_num max elements in the given vector and store them in the given vector of max values (vec_of_max_values).
// The size of vec_of_max_values should not be less than elem_num (ideally, it should be equal according to the logic).
// The number of elements in the given vector should not be less than elem_num. Otherwise, it is undefined behavior.
// The algorithm is performed in a single thread.
template <typename T>
inline void find_n_max_elem_in_vector(const std::vector<T>& vec, const std::size_t elem_num, std::vector<T>& vec_of_max_values) {
	// Build a priority queue from the given vector using a special constructor for this. The time complexity of this method is effective - O(N).
	std::priority_queue<T> max_heap(vec.begin(), vec.end());

	for (std::size_t i = 0; i < elem_num; ++i) {
		T max_heap_top = max_heap.top();
		max_heap.pop();
		vec_of_max_values[i] = max_heap_top;
	}
}