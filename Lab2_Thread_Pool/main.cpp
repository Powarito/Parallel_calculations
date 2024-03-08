#include <iostream>
#include <sstream>
#include <random>
#include "concurrent_queue.h"
#include "thread_pool.h"

using std::chrono::seconds;

constexpr int min_sleep_seconds = 2;
constexpr int max_sleep_seconds = 8;

std::random_device randDevice;
std::mt19937 randGen{ randDevice() };
std::uniform_int_distribution<int> uniDist(min_sleep_seconds, max_sleep_seconds);

read_write_lock out_mutex;

void foo() {
	const int sleep_seconds = uniDist(randGen);
	std::this_thread::sleep_for(seconds(sleep_seconds));

	write_lock w_lock(out_mutex);
	std::cout << "thread id: " << std::this_thread::get_id() << "; \tfoo();         \ttook " << sleep_seconds << "s \n";
}

//void boo(int a) {
//	const int sleep_seconds = uniDist(randGen);
//	std::this_thread::sleep_for(seconds(sleep_seconds));
//
//	write_lock w_lock(out_mutex);
//	std::cout << "thread id: " << std::this_thread::get_id() << "; \tboo(int);      \ta = " << a << "; \ttook " << sleep_seconds << "s \n";
//}
//
//int roo(int a, int b) {
//	const int sleep_seconds = uniDist(randGen);
//	std::this_thread::sleep_for(seconds(sleep_seconds));
//
//	write_lock w_lock(out_mutex);
//	std::cout << "thread id: " << std::this_thread::get_id() << "; \troo(int, int); \ta = " << a << ";  b = " << b << "; \ttook " << sleep_seconds << "s \n";
//	return a;
//}

int main(int argc, char* argv[]) {
	constexpr std::size_t worker_thread_count = 2;
	constexpr std::size_t tp_interval_seconds = 10;

	thread_pool<true> tp(tp_interval_seconds);
	tp.initialize(worker_thread_count);

	std::vector<std::thread> add_task_threads(10);
	for (int i = 0; i < add_task_threads.size(); ++i) {
		//add_task_threads[i] = std::thread([&tp] {
			tp.add_task(foo);
		//});
	}

	//std::this_thread::sleep_for(seconds(30));
	//for (int i = 0; i < 5; ++i) {
	//	tp.add_task(foo);
	//}
	//
	//std::this_thread::sleep_for(seconds(15));
	//for (int i = 0; i < 10; ++i) {
	//	tp.add_task(foo);
	//}
	std::this_thread::sleep_for(seconds(15));
	tp.terminate(true);
	
	//std::for_each(add_task_threads.begin(), add_task_threads.end(), std::mem_fn(&std::thread::join));

	std::cout << "\nreturn main()\n";

	return 0;
}