#pragma once

#include <thread>
#include <vector>
#include <queue>
#include <atomic>
#include <mutex>

// ===== Atomic algorithm =====

// Find elem_num max elements in the given vector range and store them in the given vector of atomics (vec_of_max_atomics).
// The size of vec_of_max_atomics should not be less than elem_num (ideally, it should be equal according to the logic).
// The number of elements in the given vector range should not be less than elem_num. Otherwise, it is undefined behavior.
template <typename T>
inline void find_n_max_elem_in_vector_range_atomic(const typename std::vector<T>::const_iterator it_begin, const typename std::vector<T>::const_iterator it_end, const std::size_t elem_num, std::vector<std::atomic<T>>& vec_of_max_atomics) {
	// Build a priority queue from the given vector iterators using a special constructor for this. The time complexity of this method is effective - O(N).
	std::priority_queue<T> max_heap(it_begin, it_end);

	for (std::size_t i = 0; i < elem_num; ++i) {
		T max_heap_top = max_heap.top();

		// A little optimization: since we pop values from a max heap, it means that the next values would not be greater then the previous ones.
		// And because of that, no matter what result would be with some atomic for the previous value, 
		// for the next value it always would fail with the same atomic (contained value in the atomic is never changed).
		// So we don't need to iterate through all the atomics from the very start again and again.
		for (std::size_t k = i; k < elem_num; ++k) {
			T contained = vec_of_max_atomics[k].load();
			bool changed = true;

			if (max_heap_top > contained) {
				while (!vec_of_max_atomics[k].compare_exchange_weak(contained, max_heap_top)) {
					if (max_heap_top <= contained) {
						changed = false;
						break;
					}
				}

				if (changed) {
					break;
				}
			}
		}

		max_heap.pop();
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

// Find elem_num max elements in the given vector range and store them in the given vector of max values (vec_of_max_values).
// The size of vec_of_max_values should not be less than elem_num (ideally, it should be equal according to the logic).
// The number of elements in the given vector range should not be less than elem_num. Otherwise, it is undefined behavior.
template <typename T>
inline void find_n_max_elem_in_vector_range_mutex(const typename std::vector<T>::const_iterator it_begin, const typename std::vector<T>::const_iterator it_end, const std::size_t elem_num, std::vector<T>& vec_of_max_values, std::mutex& max_values_mutex) {
	// Build a priority queue from the given vector iterators using a special constructor for this. The time complexity of this method is effective - O(N).
	std::priority_queue<T> max_heap(it_begin, it_end);

	for (std::size_t i = 0; i < elem_num; ++i) {
		T max_heap_top = max_heap.top();

		// A little optimization: since we pop values from a max heap, it means that the next values would not be greater then the previous ones.
		// And because of that, no matter what result would be with some vec_of_max_values[k] for the previous value, 
		// for the next value it always would fail with the same vec_of_max_values[k] (contained value in the vec_of_max_values[k] is never changed).
		// So we don't need to iterate through all the vec_of_max_values vector from the very start again and again.
		max_values_mutex.lock();

		for (std::size_t k = i; k < elem_num; ++k) {
			if (max_heap_top > vec_of_max_values[k]) {
				vec_of_max_values[k] = max_heap_top;
				break;
			}
		}

		max_values_mutex.unlock();

		max_heap.pop();
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