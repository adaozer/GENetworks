#include "server.h"
#include "client.h"



void clientAdd(SOCKET client_socket) {
	std::string username;
	std::string message;
	{
		std::lock_guard<std::mutex> lock(mx);
		std::string key = std::to_string((uintptr_t)client_socket);
		clients[username] = client_socket;
	}
	while (1) {
		//send receive
	}

	closesocket(client_socket);
}


int main() {
	WSADATA wsaData;
	int iResult;

	// Initialize Winsock
	if (WSAStartup(MAKEWORD(2, 2), &wsaData)!= 0) {
		std::cerr << "WSAStartup failed: \n" << WSAGetLastError() << std::endl;
		return 1;
	}

	SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (server_socket == INVALID_SOCKET) {
		std::cerr << "Error at socket(): \n" << WSAGetLastError() << std::endl;
		WSACleanup();
		return 1;
	}

	sockaddr_in server_address = {};
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(65432);
	server_address.sin_addr.s_addr = INADDR_ANY;

	if (bind(server_socket, (sockaddr*)&server_address, sizeof(server_address)) == SOCKET_ERROR) {
		std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
		closesocket(server_socket);
		WSACleanup();
		return 1;
	}

	if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
		std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
		closesocket(server_socket);
		WSACleanup();
		return 1;
	}

	while (1) {
		sockaddr_in client_address = {};
		int client_address_len = sizeof(client_address);
		SOCKET client_socket = accept(server_socket, (sockaddr*)&client_address, &client_address_len);
		if (client_socket == INVALID_SOCKET) {
			std::cerr << "accept failed: " << WSAGetLastError() << std::endl;
			continue;
		}
		//char client_ip[INET_ADDRSTRLEN];
		//inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
		std::thread t(clientAdd, client_socket);
		t.detach();
	}
	closesocket(server_socket);
	WSACleanup();
	return 0;
}