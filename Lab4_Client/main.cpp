#include "tcp_client.h"
#include "init_matrix.h"
#include <iostream>
#include <chrono>

using std::numeric_limits;

// because of Windows max and min macroses, compiler can't see std::numeric_limits::max() and std::numeric_limits::min().
#undef max
#undef min

int main(int argc, char* argv[]) {
	using myType = std::int32_t;

	std::random_device randDevice;
	std::mt19937 randGen{ randDevice() };
	std::uniform_int_distribution<myType> uniDist(numeric_limits<myType>::min(), numeric_limits<myType>::max());

	constexpr std::uint16_t dimension = 1000;

	// Using 1D vector (array) is more cache-friendly since all the data is stored in contiguous memory.
	std::vector<myType> matrix(dimension * dimension);

	// Assign random generated values to a vector in multiple threads - fast for giant containers.
	const std::size_t hardware_concurrency = std::thread::hardware_concurrency();
	initMatrixVector(matrix, dimension, (dimension < hardware_concurrency ? dimension : hardware_concurrency), randGen, uniDist);

	tcp_client::init_protocol();

	tcp_client client;

	const std::string server_IP = "127.0.0.1";
	constexpr int server_port = 8888;
	
	try {
		client.connect_to_server(server_IP, server_port);

		std::uint32_t array_size_in_bytes = matrix.size() * sizeof(myType);
		std::uint16_t thread_count = 16;

		// Last argument true - for the first time, we need to cast array data to big endian (no affect if host is already big-endian).
		std::cout << "CLIENT: sending data...\n";
		int response_code = client.send_data(array_size_in_bytes, dimension, thread_count, matrix, true);
		std::cout << "SERVER RESPONSE: " << client.get_response_from_code(response_code) << ".\n";

		while (response_code) {
			// Repeat with false flag (don't cast array data to big-endian) until client gets OK response code (which is 0).
			std::this_thread::sleep_for(std::chrono::seconds(2));
			std::cout << "CLIENT: sending data...\n";
			response_code = client.send_data(array_size_in_bytes, dimension, thread_count, matrix, false);
			std::cout << "SERVER RESPONSE: " << client.get_response_from_code(response_code) << ".\n";
		}

		// Attempt to start processing data array.
		std::cout << "CLIENT: sending command start process...\n";
		while (response_code = client.start_processing()) {
			std::cout << "SERVER RESPONSE: " << client.get_response_from_code(response_code) << ".\n";
			std::this_thread::sleep_for(std::chrono::seconds(2));
			std::cout << "CLIENT: sending command start process...\n";
		}
		std::cout << "SERVER RESPONSE: " << client.get_response_from_code(response_code) << ".\n";

		// Attempt to get result.
		std::int8_t percentage_done = 0;

		std::cout << "CLIENT: sending command get result...\n";
		while ((response_code = client.get_result(matrix, percentage_done, array_size_in_bytes)) != 9) {
			std::cout << "SERVER RESPONSE: " << client.get_response_from_code(response_code) << ". Done: " << static_cast<int>(percentage_done) << "%.\n";
			std::this_thread::sleep_for(std::chrono::seconds(2));
			std::cout << "CLIENT: sending command get result...\n";
		}
		std::cout << "SERVER RESPONSE: " << client.get_response_from_code(response_code) << ". Done: " << static_cast<int>(percentage_done) << "%.\n";
		
		// Done. Attempt to close connection with the server.
		std::cout << "CLIENT: sending command close connection...\n";
		while (response_code = client.close_connection()) {
			std::cout << "SERVER RESPONSE: " << client.get_response_from_code(response_code) << ".\n";
			std::this_thread::sleep_for(std::chrono::seconds(2));
			std::cout << "CLIENT: sending command close connection...\n";
		}
		std::cout << "SERVER RESPONSE: " << client.get_response_from_code(response_code) << ".\n";
	}
	catch (const std::exception& e) {
		std::cout << e.what();
		return 1;
	}

	tcp_client::terminate_protocol();

	return 0;
}