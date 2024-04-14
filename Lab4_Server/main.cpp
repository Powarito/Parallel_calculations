#include "tcp_server.h"

#include <iostream>

//#pragma comment(linker, "/HEAP:3000000")

int main(int argc, char* argv[]) {
	tcp_server::init_protocol();

	tcp_server server;

	const std::string server_IP = "127.0.0.1";
	constexpr int server_port = 8888;

	try {
		server.init_server(server_IP, server_port);

		struct sockaddr_in client_addr;
		int client_addr_size = sizeof(client_addr);
		SOCKET client_socket;
		
		while(client_socket = accept(server.get_socket(), (sockaddr*)&client_addr, &client_addr_size)) {
			std::thread client_thread(&tcp_server::serve_client, &server, client_socket);
			client_thread.detach();
		}
	}
	catch (const std::exception& e) {
		std::cout << e.what();
	}
	
	tcp_server::terminate_protocol();

	return 0;
}