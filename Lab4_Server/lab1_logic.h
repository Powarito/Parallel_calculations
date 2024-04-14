#pragma once

#include <vector>
#include <atomic>

// Find minimal element in the given range and assign it to vec[index]. Part of the algorithm.
template <typename T>
inline void replace_with_min(std::vector<T>& vec, const typename std::vector<T>::iterator it_begin, const typename std::vector<T>::iterator it_end, const std::size_t index) {
	typename std::vector<T>::iterator it = std::min_element(it_begin, it_end);
	if (it != it_end) {
		vec[index] = *it;
	}
}

enum class status { not_processed, in_progress, processed };

// Algorithm function called in threads.
template <typename T>
inline void parse_matrix_rows(std::vector<T>& vec, const typename std::vector<T>::iterator it_begin, const std::size_t dimension, const std::size_t rows, const std::size_t init_index, std::atomic<int>& progress_threads_done, const std::size_t thread_count, std::atomic<status>& current_status) {
	for (std::size_t i = 0; i < rows; ++i) {
		replace_with_min(vec, it_begin + i * dimension, it_begin + (i + 1) * dimension, init_index + i * dimension - i);
	}
	
	if (++progress_threads_done == thread_count) {
		current_status = status::processed;
	}
}