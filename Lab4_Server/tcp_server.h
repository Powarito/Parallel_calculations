#pragma once

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WinSock2.h>
#include <WinBase.h>
#include <cstddef>
#include <cstdint>
#include <bit>
#include <string>
#include <vector>
#include <exception>
#include <thread>
#include <mutex>

#include "lab1_logic.h"

#pragma comment(lib, "ws2_32.lib")

class tcp_server {
public:
	inline static bool is_host_big_endian();

	inline static void init_protocol();
	inline static void terminate_protocol();

	inline tcp_server();
	inline ~tcp_server();

	inline void init_server(const std::string& ip_address, const int port) const;

	inline void serve_client(SOCKET client_socket) const;

	inline SOCKET get_socket() const;

private:
	inline void recv_array_data(SOCKET& client_socket, std::vector<std::int32_t>& client_matrix, std::uint32_t array_size_in_bytes, std::atomic<status>& current_status) const;

	inline void start_processing(std::vector<std::int32_t>& client_matrix, std::int16_t dimension, std::int16_t thread_count, std::atomic<int>& progress_threads_done, std::atomic<status>& current_status) const;

	inline void get_result(SOCKET& client_socket, std::vector<std::int32_t>& client_matrix, std::uint32_t last_processing_array_size_in_bytes, std::int16_t last_processing_thread_count, std::atomic<int>& progress_threads_done, std::atomic<status>& current_status) const;

	inline void close_connection(SOCKET& client_socket, bool& need_to_close_connection) const;

private:
	inline static std::string get_last_error_as_string(bool pass_error_code = false, int error_code = 0);

protected:


private:
	SOCKET m_socket;

	constexpr static bool is_big_endian = std::endian::native == std::endian::big;
};

inline bool tcp_server::is_host_big_endian() {
	return is_big_endian;
}

inline void tcp_server::init_protocol() {
	WSADATA wsaData;
	if (int error_code = WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::string error_message = "WSAStartup (underlying API) failed: " + get_last_error_as_string(true, error_code) + ".";
		throw std::exception(error_message.c_str());
	}
}

inline void tcp_server::terminate_protocol() {
	WSACleanup();
}

inline tcp_server::tcp_server() {
	m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_socket == INVALID_SOCKET) {
		std::string error_message = "Error creating socket: " + get_last_error_as_string() + ".";
		throw std::exception(error_message.c_str());
	}
}

inline tcp_server::~tcp_server() {
	closesocket(m_socket);
}

inline void tcp_server::init_server(const std::string& ip_address, const int port) const {
	struct sockaddr_in serverAddr;
	
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = inet_addr(ip_address.c_str());
	serverAddr.sin_port = htons(port);

	if (bind(m_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		std::string error_message = "SERVER (BIND): " + ip_address + ", port: " + std::to_string(port) + " - Bind failed: " + get_last_error_as_string() + ".";
		throw std::exception(error_message.c_str());
	}

	if (listen(m_socket, SOMAXCONN) == SOCKET_ERROR) {
		std::string error_message = "SERVER (LISTEN): Listen failed: " + get_last_error_as_string() + ".";
		throw std::exception(error_message.c_str());
	}
}

inline void tcp_server::serve_client(SOCKET client_socket) const {
	std::int32_t array_size_in_bytes = 0;
	std::int16_t dimension = 0;
	std::int16_t thread_count = 0;
	std::vector<std::int32_t> client_matrix;

	std::atomic<status> current_status = status::not_processed;
	std::atomic<int> progress_threads_done = 0;
	std::int16_t last_processing_thread_count = 0;
	std::int32_t last_processing_array_size_in_bytes = 0;

	bool need_to_close_connection = false;

	while (!need_to_close_connection) {
		char response_code = 0;
		char recv_buffer[9];
		
		int recv_size = recv(client_socket, recv_buffer, sizeof(recv_buffer), 0);
		if (recv_size == SOCKET_ERROR) {
			// Send error code to client.
			response_code = 2;
			send(client_socket, &response_code, 1, 0);
			// Not continuing - letting recv_array_data get array data first.
		}
		
		// Configuration and array data.
		if (recv_buffer[0] == static_cast<char>(255)) {
			array_size_in_bytes = *reinterpret_cast<std::uint32_t*>(&recv_buffer[1]);
			if (!is_big_endian) { array_size_in_bytes = std::byteswap(array_size_in_bytes); }

			dimension = *reinterpret_cast<std::uint16_t*>(&recv_buffer[5]);
			dimension = ntohs(dimension);

			thread_count = *reinterpret_cast<std::uint16_t*>(&recv_buffer[7]);
			thread_count = ntohs(thread_count);

			recv_array_data(client_socket, client_matrix, array_size_in_bytes, current_status);
		}
		// Start processing.
		else if (recv_buffer[0] == static_cast<char>(254)) {
			if (client_matrix.size() == 0) {
				// Send error code to client.
				response_code = 5;
				send(client_socket, &response_code, 1, 0);
				continue;
			}
			else if (current_status == status::in_progress) {
				// Send error code to client.
				response_code = 6;
				send(client_socket, &response_code, 1, 0);
				continue;
			}

			current_status = status::in_progress;

			last_processing_thread_count = (dimension < thread_count) ? dimension : thread_count;
			last_processing_array_size_in_bytes = array_size_in_bytes;
			start_processing(client_matrix, dimension, last_processing_thread_count, progress_threads_done, current_status);
			
			response_code = 0;
			send(client_socket, &response_code, 1, 0);
		}
		// Get result.
		else if (recv_buffer[0] == static_cast<char>(253)) {
			get_result(client_socket, client_matrix, last_processing_array_size_in_bytes, last_processing_thread_count, progress_threads_done, current_status);
		}
		else if (recv_buffer[0] == static_cast<char>(252)) {
			close_connection(client_socket, need_to_close_connection);
		}
	}
}

inline SOCKET tcp_server::get_socket() const {
	return m_socket;
}

inline void tcp_server::recv_array_data(SOCKET& client_socket, std::vector<std::int32_t>& client_matrix, std::uint32_t array_size_in_bytes, std::atomic<status>& current_status) const {
	char response_code = 0;
	std::uint32_t total_received = 0;
	
	char* data_recv_buffer = new char[array_size_in_bytes];
	char temp_recv_buffer[1024];
	
	while (total_received < array_size_in_bytes) {
		int bytes_received = recv(client_socket, temp_recv_buffer, sizeof(temp_recv_buffer), 0);
		if (bytes_received == SOCKET_ERROR || bytes_received == 0) {
			// Send error code to client.
			response_code = 3;
			send(client_socket, &response_code, 1, 0);
			break;
		}

		std::memcpy(data_recv_buffer + total_received, temp_recv_buffer, bytes_received);
		total_received += bytes_received;
	}

	if (current_status == status::in_progress) {
		// Send error code to client.
		response_code = 4;
		send(client_socket, &response_code, 1, 0);

		delete[] data_recv_buffer;
		return;
	}

	current_status = status::not_processed;

	std::vector<std::int32_t> received_array(reinterpret_cast<std::int32_t*>(data_recv_buffer), reinterpret_cast<std::int32_t*>(data_recv_buffer + array_size_in_bytes));
	
	if (!is_big_endian) {
		std::transform(received_array.begin(), received_array.end(), received_array.begin(), [](std::int32_t elem) {
			return std::byteswap(elem);
		});
	}

	delete[] data_recv_buffer;

	client_matrix = std::move(received_array);
	
	response_code = 0;
	send(client_socket, &response_code, 1, 0);
}

inline void tcp_server::start_processing(std::vector<std::int32_t>& client_matrix, std::int16_t dimension, std::int16_t thread_count, std::atomic<int>& progress_threads_done, std::atomic<status>& current_status) const {
	progress_threads_done = 0;
	
	if (dimension < thread_count) {
		thread_count = dimension;
	}

	const std::size_t row_step = dimension / thread_count;
	const std::size_t remainder = (dimension - row_step * thread_count == 0 ? 0 : dimension - row_step * (thread_count - 1));

	std::vector<std::thread> threads(thread_count);

	if (remainder == 0) {
		for (std::size_t i = 0; i < thread_count; ++i) {
			threads[i] = std::thread(parse_matrix_rows<std::int32_t>, std::ref(client_matrix), client_matrix.begin() + i * row_step * dimension, dimension, row_step, dimension + i * row_step * dimension - 1 - i * row_step, std::ref(progress_threads_done), thread_count, std::ref(current_status));
		}
	}
	else {
		for (std::size_t i = 0; i < thread_count - 1; ++i) {
			threads[i] = std::thread(parse_matrix_rows<std::int32_t>, std::ref(client_matrix), client_matrix.begin() + i * row_step * dimension, dimension, row_step, dimension + i * row_step * dimension - 1 - i * row_step, std::ref(progress_threads_done), thread_count, std::ref(current_status));
		}
		threads[thread_count - 1] = std::thread(parse_matrix_rows<std::int32_t>, std::ref(client_matrix), client_matrix.end() - dimension * remainder, dimension, remainder, client_matrix.size() - remainder * dimension + remainder - 1, std::ref(progress_threads_done), thread_count, std::ref(current_status));
	}

	for (auto& thread : threads) {
		thread.detach();
	}
}

inline void tcp_server::get_result(SOCKET& client_socket, std::vector<std::int32_t>& client_matrix, std::uint32_t last_processing_array_size_in_bytes, std::int16_t last_processing_thread_count, std::atomic<int>& progress_threads_done, std::atomic<status>& current_status) const {
	char response_code_and_progress[2];
	response_code_and_progress[0] = 0;

	status stutus_at_moment = current_status;

	if (stutus_at_moment == status::not_processed) { response_code_and_progress[0] = 7; }
	else if (stutus_at_moment == status::in_progress) { response_code_and_progress[0] = 8; }
	else if (stutus_at_moment == status::processed) { response_code_and_progress[0] = 9; }
	
	std::uint8_t percentage = 0;
	if (last_processing_thread_count > 0) {
		percentage = progress_threads_done / static_cast<float>(last_processing_thread_count) * 100;
	}
	response_code_and_progress[1] = percentage;

	send(client_socket, response_code_and_progress, 2, 0);

	if (stutus_at_moment == status::processed) {
		if (!is_big_endian) {
			std::transform(client_matrix.begin(), client_matrix.end(), client_matrix.begin(), [](std::int32_t elem) {
				return std::byteswap(elem);
			});
		}

		const char* const data = reinterpret_cast<const char*>(client_matrix.data());
		const int chunk_size = 1024;
		int total_sent = 0;

		while (total_sent < last_processing_array_size_in_bytes) {
			int remaining = last_processing_array_size_in_bytes - total_sent;
			int current_chunk_size = (remaining < chunk_size) ? remaining : chunk_size;

			int send_size = send(client_socket, data + total_sent, current_chunk_size, 0);
			if (send_size == SOCKET_ERROR) {
				break;
			}

			total_sent += current_chunk_size;
		}
	}
}

inline void tcp_server::close_connection(SOCKET& client_socket, bool& need_to_close_connection) const {
	need_to_close_connection = true;

	char response_code = 0;
	send(client_socket, &response_code, 1, 0);

	closesocket(client_socket);
}

inline std::string tcp_server::get_last_error_as_string(bool pass_error_code, int error_code) {
	DWORD error_message_id = error_code;

	if (!pass_error_code) {
		error_message_id = ::GetLastError();
		if (error_message_id == 0) {
			return std::string(); // No error message has been recorded
		}
	}

	LPSTR message_buffer = nullptr;

	// Ask Win32 to give us the string version of that message ID.
	// The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
	std::size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, error_message_id, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&message_buffer, 0, NULL);

	// Copy the error message into a std::string.
	std::string message(message_buffer, size);

	// Free the Win32's string's buffer.
	LocalFree(message_buffer);

	return message;
}