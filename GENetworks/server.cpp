#include "server.h"
#include "client.h"



void clientAdd(sockaddr* client_address) {

}


int main() {
	SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in server_address = {};
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(65432);
	server_address.sin_addr.s_addr = INADDR_ANY;
	listen(server_socket, SOMAXCONN);

	while (1) {
		sockaddr_in client_address = {};
		int client_address_len = sizeof(client_address);
		SOCKET client_socket = accept(server_socket, (sockaddr*)&client_address, &client_address_len);
		char client_ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
		std::thread clientAdd(client_address);
	}

	return 0;
}