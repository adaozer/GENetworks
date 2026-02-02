#include "server.h"

bool sendAll(SOCKET s, const char* data, int len) {
	int sentSum = 0;
	while (sentSum < len) {
		int sent = send(s, data + sentSum, len - sentSum, 0);
		if (sent == SOCKET_ERROR) return false;
		sentSum += sent;
	}
	return true;
}

bool sendLine(SOCKET s, const std::string& line) {
	std::string out = line;
	if (out.empty() || out.back() != '\n') out.push_back('\n');
	return sendAll(s, out.c_str(), (int)out.size());
}

std::string stripCR(std::string s) {
	s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
	return s;
}

std::string removeClient(SOCKET s) {
	std::string username;
	{
		std::lock_guard<std::mutex> lock(mx);
		auto it = clientSockets.find(s);
		if (it != clientSockets.end()) {
			username = it->second;
			clients.erase(username);
			clientSockets.erase(it);
		}
		recvBuffers.erase(s);
	}
	closesocket(s);
	return username;
}

void broadcast(const std::string& line, SOCKET sender = INVALID_SOCKET) {
	std::vector<SOCKET> sockets;
	{
		std::lock_guard<std::mutex> lock(mx);
		sockets.reserve(clients.size());
		for (auto& c : clients) {
			if (c.second != sender) sockets.push_back(c.second);
		}
	}

	std::vector<std::string> leavers;
	for (SOCKET s : sockets) {
		if (!sendLine(s, line)) {
			std::string u = removeClient(s);
			if (!u.empty()) leavers.push_back(u);
		}
	}

	if (leavers.empty()) return;

	std::vector<SOCKET> remaining;
	{
		std::lock_guard<std::mutex> lock(mx);
		remaining.reserve(clients.size());
		for (auto& c : clients) remaining.push_back(c.second);
	}

	for (const auto& u : leavers) {
		for (SOCKET s : remaining) {
			sendLine(s, u + " has left!");
		}
	}
}

std::string clientList() {
	std::lock_guard<std::mutex> lock(mx);
	std::string output = "Online: ";
	bool first = true;
	for (auto& client : clients) {
		output += (first ? " " : ", ");
		output += client.first;
		first = false;
	}
	return output;
}

bool readText(SOCKET s) {
	char buff[1024];
	int received = recv(s, buff, sizeof(buff), 0);
	if (received <= 0) return false;
	{
		std::lock_guard<std::mutex> lock(mx);
		recvBuffers[s].append(buff, buff + received);
	}
	return true;
}

bool completeLine(SOCKET s, std::string& output) {
	std::lock_guard<std::mutex> lock(mx);
	auto it = recvBuffers.find(s);
	if (it == recvBuffers.end()) return false;

	std::string& buf = it->second;
	size_t pos = buf.find('\n');
	if (pos == std::string::npos) return false;

	output = buf.substr(0, pos);
	buf.erase(0, pos + 1);
	output = stripCR(output);
	return true;
}

void clientAdd(SOCKET client_socket) {
	{
		std::lock_guard<std::mutex> lock(mx);
		recvBuffers[client_socket] = "";
	}

	std::string username;
	while (username.empty()) {
		std::string line;

		while (!completeLine(client_socket, line)) {
			if (!readText(client_socket)) {
				removeClient(client_socket);
				return;
			}
		}

		username = line;

		if (username.empty() || username.size() > 24) {
			sendLine(client_socket, "ERR invalid username (1-24 chars).");
			{
				std::lock_guard<std::mutex> lock(mx);
				recvBuffers.erase(client_socket);
			}
			closesocket(client_socket);
			return;
		}

		{
			std::lock_guard<std::mutex> lock(mx);
			if (clients.find(username) != clients.end()) {
				sendLine(client_socket, "ERR username taken.");
				recvBuffers.erase(client_socket);
				closesocket(client_socket);
				return;
			}
			clients[username] = client_socket;
			clientSockets[client_socket] = username;
		}

	}

	sendLine(client_socket, "Welcome " + username + "!");
	broadcast(username + " has joined!", client_socket);

	while (true) {
		std::string line;
		while (!completeLine(client_socket, line)) {
			if (!readText(client_socket)) {
				std::string u = removeClient(client_socket);
				if (!u.empty()) broadcast(u + " has left!");
				return;
			}
		}
		if (line.empty()) continue;

		if (line == "/leave") {
			std::string u = removeClient(client_socket);
			if (!u.empty()) broadcast(u + " has left!");
			return;
		}

		if (line == "/who") {
			sendLine(client_socket, clientList());
			continue;
		}
		if (line.rfind("/msg ", 0) == 0) {
			std::istringstream iss(line);
			std::string cmd, target;
			iss >> cmd >> target;
			std::string message;
			std::getline(iss, message);
			if (!message.empty() && message[0] == ' ')
				message.erase(0, 1);
			if (target.empty() || message.empty()) {
				sendLine(client_socket, "ERR usage: /msg <user> <text>");
				continue;
			}
			if (target == username) {
				sendLine(client_socket, "ERR you cannot DM yourself.");
				continue;
			}
			SOCKET receiver_socket = INVALID_SOCKET;
			{
				std::lock_guard<std::mutex> lock(mx);
				auto it = clients.find(target);
				if (it != clients.end()) receiver_socket = it->second;
			}
			if (receiver_socket == INVALID_SOCKET) {
				sendLine(client_socket, "ERR user not found: " + target);
				continue;
			}
			sendLine(receiver_socket, "(DM) " + username + ": " + message);
			sendLine(client_socket, "(DM to " + target + ") " + message);
			continue;
		}
		broadcast(username + ": " + line, client_socket);
	}
}


int main() {
	WSADATA wsaData;
	int iResult;

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
		std::thread t(clientAdd, client_socket);
		t.detach();
	}
	closesocket(server_socket);
	WSACleanup();
	return 0;
}