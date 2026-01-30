#include "server.h"
#include "client.h"

void broadcast(int received, const char* recvbuff, SOCKET sender) {
	std::vector<SOCKET> current_sockets;
	{
		std::lock_guard<std::mutex> lock(mx);
		for (auto& c : clients) {
			if (c.second != sender) {
				current_sockets.push_back(c.second);
			}
		}
	}
	for (SOCKET s : current_sockets) {
		send(s, recvbuff, received, 0);
	}
}

void clientAdd(SOCKET client_socket) {
	std::string key = std::to_string((uintptr_t)client_socket);
	{
		std::lock_guard<std::mutex> lock(mx);
		clients[key] = client_socket;
	}
	char recvbuff[1024];
	while (1) {
		int received = recv(client_socket, recvbuff, sizeof(recvbuff), 0);
		if (received > 0) {
			broadcast(received, recvbuff, client_socket);
		}
		else {
			break;
		}
	}
	{
		std::lock_guard<std::mutex> lock(mx);
		clients.erase(key);
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