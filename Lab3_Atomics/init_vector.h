#pragma once

#include <thread>
#include <vector>
#include <random>

// Assign random generated values into the given vector range. Part of the vector initialization init_vector function.
template <typename T>
inline void init_vector_range(const typename std::vector<T>::iterator it_begin, const typename std::vector<T>::iterator it_end, std::mt19937& rand_gen, std::uniform_int_distribution<T>& uni_dist) {
	std::generate(it_begin, it_end, [&]() {
		return uni_dist(rand_gen);
	});
}

// Initialize given vector with random values.
template <typename T>
inline void init_vector(std::vector<T>& vec, const std::size_t thread_count, std::mt19937& rand_gen, std::uniform_int_distribution<T>& uni_dist) {
	const std::size_t vec_size = vec.size();

	const std::size_t step = vec_size / thread_count;
	const std::size_t remainder = (vec_size - step * thread_count == 0 ? 0 : vec_size - step * (thread_count - 1));

	std::vector<std::thread> init_vec_threads(thread_count);

	for (std::size_t i = 0; i < thread_count - 1; ++i) {
		init_vec_threads[i] = std::thread(init_vector_range<T>, vec.begin() + i * step, vec.begin() + (i + 1) * step, std::ref(rand_gen), std::ref(uni_dist));
	}
	init_vec_threads[thread_count - 1] = std::thread(init_vector_range<T>, vec.end() - (remainder == 0 ? step : remainder), vec.end(), std::ref(rand_gen), std::ref(uni_dist));

	for (auto& init_vec_thread : init_vec_threads) {
		init_vec_thread.join();
	}
}