#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <queue>
#include <random>
#include <limits>
#include <iostream>

#include "init_vector.h"
#include "find_n_max_algorithms.h"

using std::chrono::nanoseconds;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;

template <typename T, typename U>
inline void print_result_info(const std::vector<T>& vec, const std::size_t thread_count_info, const std::vector<U>& max_values, T& sum, const nanoseconds& elapsed_ns);

int main(int argc, char* argv[]) {
	using myType = int;

	constexpr unsigned int elem_num = 3;

	std::random_device rand_device;
	std::mt19937 rand_gen{ rand_device() };
	std::uniform_int_distribution<myType> uni_dist(std::numeric_limits<myType>::min(), std::numeric_limits<myType>::max() / elem_num);

	constexpr std::size_t vec_size = 100'000'000;
	std::vector<myType> vec(vec_size);

	const std::size_t thread_count = std::thread::hardware_concurrency();

	// Assign random generated values to a vector in multiple threads - fast for giant containers.
	init_vector(vec, std::min(vec_size, thread_count), rand_gen, uni_dist);

	myType sum = 0;

	// Atomic algorithm.
	std::vector<std::atomic<myType>> vec_of_max_atomics(elem_num);
	for (auto& atomic : vec_of_max_atomics) {
		atomic = std::numeric_limits<myType>::min();
	}

	auto atomic_algorithm_start = high_resolution_clock::now();
	find_n_max_elem_in_vector_atomic(vec, elem_num, thread_count, vec_of_max_atomics);
	auto atomic_algorithm_end   = high_resolution_clock::now();
	auto atomic_algorithm_elapsed = duration_cast<nanoseconds>(atomic_algorithm_end - atomic_algorithm_start);

	std::printf("\n=== Atomic algorithm ===\n");
	print_result_info(vec, thread_count, vec_of_max_atomics, sum, atomic_algorithm_elapsed);

	// Mutex algorithm.
	std::vector<myType> vec_of_max_values_multithread(elem_num, std::numeric_limits<myType>::min());
	std::mutex max_values_mutex;

	auto mutex_algorithm_start = high_resolution_clock::now();
	find_n_max_elem_in_vector_mutex(vec, elem_num, thread_count, vec_of_max_values_multithread, max_values_mutex);
	auto mutex_algorithm_end = high_resolution_clock::now();
	auto mutex_algorithm_elapsed = duration_cast<nanoseconds>(mutex_algorithm_end - mutex_algorithm_start);

	std::printf("\n=== Mutex algorithm ===\n");
	print_result_info(vec, thread_count, vec_of_max_values_multithread, sum, mutex_algorithm_elapsed);

	// Singlethreaded algorithm.
	std::vector<myType> vec_of_max_values_single_thread(elem_num, std::numeric_limits<myType>::min());

	auto single_thread_algorithm_start = high_resolution_clock::now();
	find_n_max_elem_in_vector(vec, elem_num, vec_of_max_values_single_thread);
	auto single_thread_algorithm_end = high_resolution_clock::now();
	auto single_thread_algorithm_elapsed = duration_cast<nanoseconds>(single_thread_algorithm_end - single_thread_algorithm_start);

	std::printf("\n=== Singlethreaded algorithm ===\n");
	print_result_info(vec, thread_count, vec_of_max_values_single_thread, sum, single_thread_algorithm_elapsed);

	return 0;
}

template <typename T, typename U>
inline void print_result_info(const std::vector<T>& vec, const std::size_t thread_count_info, const std::vector<U>& max_values, T& sum, const nanoseconds& elapsed_ns) {
	std::printf("Vector size            : %llu.\n", vec.size());
	std::printf("Amount of threads      : %llu.\n", thread_count_info);
	std::printf("Amount of max elements : %llu.\n", max_values.size());
	std::printf("Max elements are       : ");

	sum = 0;
	for (auto& max_value : max_values) {
		T value = static_cast<T>(max_value);
		std::cout << value << "; ";
		sum += value;
	}
	std::cout << "\nThe sum is             : " << sum << ".\n";
	std::printf("Execution time         : %.4f seconds.\n", elapsed_ns.count() * 1e-9);
}