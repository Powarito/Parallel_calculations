#pragma once

#include <thread>
#include <vector>
#include <random>

// Assign random generated values into the given range. Part of the matrix initialization initMatrixVector function.
template <typename T>
inline void initVecRange(const typename std::vector<T>::iterator itBegin, const typename std::vector<T>::iterator itEnd, std::mt19937& randGen, std::uniform_int_distribution<T>& uniDist) {
	std::generate(itBegin, itEnd, [&]() {
		return uniDist(randGen);
		});
}

// Initialize given matrix with random values.
template <typename T>
inline void initMatrixVector(std::vector<T>& matrix, const std::size_t dimension, std::size_t thread_count, std::mt19937& rand_gen, std::uniform_int_distribution<T>& uni_dist) {
	if (dimension < thread_count) {
		thread_count = dimension;
	}
	
	const std::size_t rowStep = dimension / thread_count;
	const std::size_t remainder = (dimension - rowStep * thread_count == 0 ? 0 : dimension - rowStep * (thread_count - 1));

	std::vector<std::thread> initVecThreads(thread_count);

	if (remainder == 0) {
		for (std::size_t i = 0; i < thread_count; ++i) {
			initVecThreads[i] = std::thread(initVecRange<T>, matrix.begin() + i * dimension * rowStep, matrix.begin() + (i + 1) * dimension * rowStep, std::ref(rand_gen), std::ref(uni_dist));
		}
	}
	else {
		for (std::size_t i = 0; i < thread_count - 1; ++i) {
			initVecThreads[i] = std::thread(initVecRange<T>, matrix.begin() + i * dimension * rowStep, matrix.begin() + (i + 1) * dimension * rowStep, std::ref(rand_gen), std::ref(uni_dist));
		}
		initVecThreads[thread_count - 1] = std::thread(initVecRange<T>, matrix.end() - dimension * remainder, matrix.end(), std::ref(rand_gen), std::ref(uni_dist));
	}

	for (auto& initVecThread : initVecThreads) {
		initVecThread.join();
	}
}