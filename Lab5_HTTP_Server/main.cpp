#include <iostream>

#include "http_server.h"
#include "thread_pool.h"

int main(int argc, char* argv[]) {
	thread_pool clients_thead_pool;
	clients_thead_pool.initialize(std::thread::hardware_concurrency());
	
	http_server::init_protocol_and_load_files();

	http_server server;

	const std::string server_IP = "127.0.0.1";
	constexpr int server_port = 8080;

	try {
		server.init_server(server_IP, server_port);

		struct sockaddr_in client_addr;
		int client_addr_size = sizeof(client_addr);
		SOCKET client_socket;

		while (client_socket = accept(server.get_socket(), (sockaddr*)&client_addr, &client_addr_size)) {
			//std::thread client_thread(&http_server::serve_client, &server, client_socket);
			//client_thread.detach();
			clients_thead_pool.add_task(&http_server::serve_client, &server, client_socket);
		}
	}
	catch (const std::exception& e) {
		std::cout << e.what();
	}

	http_server::terminate_protocol();

	clients_thead_pool.terminate();

	return 0;
}