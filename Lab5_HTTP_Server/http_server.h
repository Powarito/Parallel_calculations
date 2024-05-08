#pragma once

#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <WinSock2.h>
#include <WinBase.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <sstream>
#include <exception>
#include <thread>
#include <mutex>

#pragma comment(lib, "ws2_32.lib")

#include "files_hash_table.h"

class http_server {
public:
	inline static void init_protocol_and_load_files();
	inline static void terminate_protocol();

	inline http_server();
	inline ~http_server();

	inline void init_server(const std::string& ip_address, const int port) const;

	inline void serve_client(SOCKET client_socket) const;

	inline SOCKET get_socket() const;

private:
	inline static void load_files();

	inline static std::string get_last_error_as_string(bool pass_error_code = false, int error_code = 0);

private:
	SOCKET m_socket;
};

inline void http_server::init_protocol_and_load_files() {
	load_files();

	WSADATA wsaData;
	if (int error_code = WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::string error_message = "WSAStartup (underlying API) failed: " + get_last_error_as_string(true, error_code) + ".";
		throw std::exception(error_message.c_str());
	}
}

inline void http_server::terminate_protocol() {
	WSACleanup();
}

inline http_server::http_server() {
	m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (m_socket == INVALID_SOCKET) {
		std::string error_message = "Error creating socket: " + get_last_error_as_string() + ".";
		throw std::exception(error_message.c_str());
	}
}

inline http_server::~http_server() {
	closesocket(m_socket);
}

inline void http_server::init_server(const std::string& ip_address, const int port) const {
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

inline void http_server::serve_client(SOCKET client_socket) const {
	std::string get_request(1024, '\0');
	recv(client_socket, const_cast<char*>(get_request.data()), get_request.size(), 0);

	std::stringstream sstream(std::string(get_request.begin(), std::find(get_request.begin(), get_request.end(), '\n')));
	std::string path;

	while (sstream >> path) {
		if (path[0] == '/') {
			if (path.size() == 1) {
				path += "index.html";
			}
			break;
		}
	}

	path.erase(0, 1);

	std::string http_response;

	if (file_to_content_umap.find(path) != file_to_content_umap.end()) {
		http_response = "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(file_to_content_umap[path].length())
			+ "\r\n\r\n"
			+ file_to_content_umap[path];
	}
	else {
		http_response = "HTTP/1.1 404 Not Found\r\nContent-Length: " + std::to_string(file_to_content_umap[error404_page_path].length())
			+ "\r\n\r\n"
			+ file_to_content_umap[error404_page_path];
	}

	send(client_socket, http_response.c_str(), http_response.size(), 0);

	closesocket(client_socket);
}

inline SOCKET http_server::get_socket() const {
	return m_socket;
}

inline void http_server::load_files() {
	for (const auto& file_content_pair : file_to_content_umap) {
		std::ifstream file(file_content_pair.first, std::ios::in | std::ios::binary);
		std::stringstream file_buffer;
		file_buffer << file.rdbuf();
		file_to_content_umap[file_content_pair.first] = file_buffer.str();
	}
}

inline std::string http_server::get_last_error_as_string(bool pass_error_code, int error_code) {
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