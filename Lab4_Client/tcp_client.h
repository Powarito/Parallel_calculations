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
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

class tcp_client {
public:
	inline static bool is_host_big_endian();
	
	inline static void init_protocol();
	inline static void terminate_protocol();

	inline tcp_client();
	inline ~tcp_client();

	inline void connect_to_server(const std::string& ip_address, const int port) const;
	
	inline int send_data(const std::uint32_t array_size_in_bytes, const std::uint16_t dimension, const std::uint16_t thread_count, std::vector<std::int32_t>& array_data, bool cast_array_data_to_big_endian) const;
	inline int start_processing() const;
	inline int get_result(std::vector<std::int32_t>& out_matrix, std::int8_t& out_percentage_done, const std::uint32_t array_size_in_bytes) const;
	
	inline int close_connection() const;

	inline static const char* get_response_from_code(const uint8_t response_code);

public:
	inline constexpr explicit tcp_client(const tcp_client&)	= delete;
	inline constexpr explicit tcp_client(tcp_client&&)		= delete;

	inline tcp_client& operator=(const tcp_client&)			= delete;
	inline tcp_client& operator=(tcp_client&&)				= delete;

private:
	inline static std::string get_last_error_as_string(bool pass_error_code = false, int error_code = 0);

protected:


public:
	SOCKET m_socket;

	constexpr static bool is_big_endian = std::endian::native == std::endian::big;
};

inline bool tcp_client::is_host_big_endian() {
	return is_big_endian;
}

inline void tcp_client::init_protocol() {
	WSADATA wsaData;
	if (int error_code = WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::string error_message = "WSAStartup (underlying API) failed: " + get_last_error_as_string(true, error_code) + ".";
		throw std::exception(error_message.c_str());
	}
}

inline void tcp_client::terminate_protocol() {
	WSACleanup();
}

inline tcp_client::tcp_client() {
	m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_socket == INVALID_SOCKET) {
		std::string error_message = "Error creating socket: " + get_last_error_as_string() + ".";
		throw std::exception(error_message.c_str());
	}
}

inline tcp_client::~tcp_client() {
	closesocket(m_socket);
}

inline void tcp_client::connect_to_server(const std::string& ip_address, const int port) const {
	struct sockaddr_in server_addr;
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ip_address.c_str());
	server_addr.sin_port = htons(port);

	if (connect(m_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
		std::string error_message = "CLIENT (CONNECT): " + ip_address + ", port: " + std::to_string(port) + " - Connect failed: " + get_last_error_as_string() + ".";
		throw std::exception(error_message.c_str());
	}
}

inline int tcp_client::send_data(const std::uint32_t array_size_in_bytes, const std::uint16_t dimension, const std::uint16_t thread_count, std::vector<std::int32_t>& array_data, bool cast_array_data_to_big_endian) const {
	std::string to_insert;
	to_insert.reserve(9);

	to_insert += static_cast<char>(255);

	std::uint32_t big_endian_array_size_in_bytes;
	const char* to_send_array_size_in_bytes = reinterpret_cast<const char*>(is_big_endian ? &array_size_in_bytes : &(big_endian_array_size_in_bytes = std::byteswap(array_size_in_bytes)));
	to_insert += std::string(to_send_array_size_in_bytes, sizeof(array_size_in_bytes));

	std::uint16_t big_endian_dimension = htons(dimension);
	const char* to_send_dimension = reinterpret_cast<const char*>(&big_endian_dimension);
	to_insert += std::string(to_send_dimension, sizeof(dimension));

	std::uint16_t big_endian_thread_count = htons(thread_count);
	const char* to_send_thread_count = reinterpret_cast<const char*>(&big_endian_thread_count);
	to_insert += std::string(to_send_thread_count, sizeof(thread_count));

	if (cast_array_data_to_big_endian && !is_big_endian) {
		std::transform(array_data.begin(), array_data.end(), array_data.begin(), [](std::int32_t elem) {
			return std::byteswap(elem);
		});
	}

	if (send(m_socket, to_insert.c_str(), to_insert.size(), 0) == SOCKET_ERROR) {
		std::string error_message = "CLIENT (SEND): Send config info failed: " + get_last_error_as_string() + ".";
		throw std::exception(error_message.c_str());
	}

	const char* const data = reinterpret_cast<const char* const>(array_data.data());
	const int chunk_size = 1024;
	int total_sent = 0;

	while (total_sent < array_size_in_bytes) {
		int remaining = array_size_in_bytes - total_sent;
		int current_chunk_size = (remaining < chunk_size) ? remaining : chunk_size;

		int send_size = send(m_socket, data + total_sent, current_chunk_size, 0);
		if (send_size == SOCKET_ERROR) {
			std::string error_message = "CLIENT (SEND): Send array data failed: " + get_last_error_as_string() + ".";
			throw std::exception(error_message.c_str());
		}

		total_sent += current_chunk_size;
	}

	char recv_code;
	int recv_size = recv(m_socket, &recv_code, 1, 0);
	if (recv_size == SOCKET_ERROR) {
		std::string error_message = "CLIENT (RECEIVE): Receive <send data> responce code failed: " + get_last_error_as_string() + ".";
		throw std::exception(error_message.c_str());
	}

	return recv_code;
}

inline int tcp_client::start_processing() const {
	std::string to_send(9, '\0');
	to_send[0] = 254;

	int send_size = send(m_socket, to_send.c_str(), to_send.size(), 0);
	if (send_size == SOCKET_ERROR) {
		std::string error_message = "CLIENT (SEND): Send start processing failed: " + get_last_error_as_string() + ".";
		throw std::exception(error_message.c_str());
	}

	char recv_code;
	int recv_size = recv(m_socket, &recv_code, 1, 0);
	if (recv_size == SOCKET_ERROR) {
		std::string error_message = "CLIENT (RECEIVE): Get start processing response code failed: " + get_last_error_as_string() + ".";
		throw std::exception(error_message.c_str());
	}

	return recv_code;
}

inline int tcp_client::get_result(std::vector<std::int32_t>& out_matrix, std::int8_t& out_percentage_done, const std::uint32_t array_size_in_bytes) const {
	std::string to_send(9, '\0');
	to_send[0] = 253;

	int send_size = send(m_socket, to_send.c_str(), to_send.size(), 0);
	if (send_size == SOCKET_ERROR) {
		std::string error_message = "CLIENT (SEND): Send <get result> failed: " + get_last_error_as_string() + ".";
		throw std::exception(error_message.c_str());
	}

	char recv_code;
	int recv_size = recv(m_socket, &recv_code, 1, 0);
	if (recv_size == SOCKET_ERROR) {
		std::string error_message = "CLIENT (RECEIVE): Receive <get result> responce code failed: " + get_last_error_as_string() + ".";
		throw std::exception(error_message.c_str());
	}

	recv_size = recv(m_socket, reinterpret_cast<char*>(&out_percentage_done), 1, 0);
	if (recv_size == SOCKET_ERROR) {
		std::string error_message = "CLIENT (RECEIVE): Receive <get result> percentage failed: " + get_last_error_as_string() + ".";
		throw std::exception(error_message.c_str());
	}

	// Array has been processed.
	if (recv_code == 9) {
		out_matrix.reserve(array_size_in_bytes / sizeof(std::int32_t));
		char* const out_matrix_data = reinterpret_cast<char* const>(out_matrix.data());

		std::uint32_t total_received = 0;
		char temp_recv_buffer[1024];

		while (total_received < array_size_in_bytes) {
			int bytes_received = recv(m_socket, temp_recv_buffer, sizeof(temp_recv_buffer), 0);
			if (bytes_received == SOCKET_ERROR || bytes_received == 0) {
				std::string error_message = "CLIENT (RECEIVE): Receive array data failed: " + get_last_error_as_string() + ".";
				throw std::exception(error_message.c_str());
			}

			std::memcpy(out_matrix_data + total_received, temp_recv_buffer, bytes_received);
			total_received += bytes_received;
		}

		if (!is_big_endian) {
			std::transform(out_matrix.begin(), out_matrix.end(), out_matrix.begin(), [](std::int32_t elem) {
				return std::byteswap(elem);
			});
		}
	};
	return recv_code;
}

inline int tcp_client::close_connection() const {
	std::string to_send(9, '\0');
	to_send[0] = 252;

	int send_size = send(m_socket, to_send.c_str(), to_send.size(), 0);
	if (send_size == SOCKET_ERROR) {
		std::string error_message = "CLIENT (SEND): Send <close connection> failed: " + get_last_error_as_string() + ".";
		throw std::exception(error_message.c_str());
	}

	char recv_code;
	int recv_size = recv(m_socket, &recv_code, 1, 0);
	if (recv_size == SOCKET_ERROR) {
		std::string error_message = "CLIENT (RECEIVE): Receive <close connection> response code failed: " + get_last_error_as_string() + ".";
		throw std::exception(error_message.c_str());
	}

	return recv_code;
}

inline const char* tcp_client::get_response_from_code(const uint8_t response_code) {
	switch (response_code) {
	case (0): return "OK\0";
	case (1): return "invalid command\0";
	case (2): return "error receiving command\0";
	case (3): return "error reading array data\0";
	case (4): return "error assigning new array data - already processing\0";
	case (5): return "error starting processing - array has zero size\0";
	case (6): return "error starting processing - already processing\0";
	case (7): return "the array has not been processed\0";
	case (8): return "the array is still being processed\0";
	case (9): return "the array is processed\0";
	default: return "unknown response code\0"; 
	}
}

inline std::string tcp_client::get_last_error_as_string(bool pass_error_code, int error_code) {
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