#include "server.h"
// SendAll implementation to make sure all bytes are sent.
// Takes socket to send to, the data to send and the length of the data as args
bool sendAll(SOCKET s, const char* data, int len) {
	int sentSum = 0;
	while (sentSum < len) {
		int sent = send(s, data + sentSum, len - sentSum, 0);
		if (sent == SOCKET_ERROR) return false;
		sentSum += sent;
	}
	return true;
}
// Add a newline (\n) character to the end of a line
// Args are socket to send to, line that will be sent
bool sendLine(SOCKET s, const std::string& line) {
	std::string out = line;
	if (out.empty() || out.back() != '\n') out.push_back('\n');
	return sendAll(s, out.c_str(), (int)out.size());
}

// Removes \r character
// Takes the line as arg
std::string stripCR(std::string& s) {
	s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
	return s;
}

// Remove client, takes in socket of the client as arg,
// erases the client's information from every dictionary, closes the socket and returns username so 
// it can be broadcasted to the other users that this user left.
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

// Broadcast function, takes the line to be broadcasted and the socket of the sender as an arg.
// Parses every socket that isn't the sender's socket and adds them to a list.
// Calls sendLine to send the message to each socket that is not the sender's socket.
// SendLine uses sendAll to send every byte.
// Used for "client has left" or "client has joined" type messages
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

// Like broadcast but sends it to every user including the sender itself. Used to broadcast a message to the whole server
// Only takes the line as arg as socket is not required.
void broadcastAll(const std::string& line) {
	std::vector<SOCKET> sockets;
	{
		std::lock_guard<std::mutex> lock(mx);
		for (auto& c : clients) {
			sockets.push_back(c.second);
		}
	}
	for (const auto& s : sockets) {
		sendLine(s, line);
	}
}

// Broadcast list of users. Used to construct the users list in the GUI.
// Parses clients map and broadcasts all the users.
void broadcastUsers() {
	std::string list = "USERS ";
	{
		std::lock_guard<std::mutex> lock(mx);
		bool first = true;
		for (auto& c : clients) {
			if (!first) list += ",";
			list += c.first;
			first = false;
		}
	}
	broadcastAll(list);
}

// For receiving text from a client socket. Its then written into the receive buffer of that client.
// Takes the client socket as an arg.
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

// Checks if a line inputted to a receive buffer is complete with a \n newline character at the end.
// Takes the socket that the message is being received from and a reference to the output as arguments.
bool completeLine(SOCKET s, std::string& output) {
	std::lock_guard<std::mutex> lock(mx);
	auto it = recvBuffers.find(s);
	if (it == recvBuffers.end()) return false;

	std::string& buf = it->second;
	size_t pos = buf.find('\n');
	if (pos == std::string::npos) return false;

	output = buf.substr(0, pos);
	buf.erase(0, pos + 1);
	stripCR(output);
	return true;
}

// Main server functionality. Takes the client socket as an argument after the threading.
// Receives the username of the client as its first message.
// Adds the client's username, socket, recvBuffer information to the relevant maps.
// Listens for messages and sends messages based on commands (broadcast or unicast)
// Uses the helpers (broadcast, completeline, readtext etc.)
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

		username = line; // Receive username

		if (username.empty() || username.size() > 24) {
			sendLine(client_socket, "Invalid username (Should be 1-24 characters).");
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
				sendLine(client_socket, "Username already taken.");
				recvBuffers.erase(client_socket);
				closesocket(client_socket);
				return;
			}
			clients[username] = client_socket;
			clientSockets[client_socket] = username;
		}

	}

	sendLine(client_socket, "Welcome " + username + "!");
	broadcastUsers();
	broadcast(username + " has joined!", client_socket);

	while (true) {
		std::string line;
		while (!completeLine(client_socket, line)) {
			if (!readText(client_socket)) { // Complete Line and Read text. Gracefully disconnect if they fail.
				std::string u = removeClient(client_socket);
				if (!u.empty()) {
					broadcastUsers();
					broadcast(u + " has left!");
				}
				return;
			}
		}
		if (line.empty()) continue;

		if (line == "/leave") {
			std::string u = removeClient(client_socket);
			if (!u.empty()) {
				broadcastUsers();
				broadcast(u + " has left!");
			}
			return; // Remove client and broadcast that they've left
		}

		if (line.rfind("/msg ", 0) == 0) {
			std::istringstream iss(line);
			std::string cmd, target;
			iss >> cmd >> target;
			std::string message;
			std::getline(iss, message); // Extract message from the DM.
			if (!message.empty() && message[0] == ' ')
				message.erase(0, 1);
			if (target.empty() || message.empty()) {
				sendLine(client_socket, "Format: /msg <user> <message>");
				continue;
			}
			if (target == username) {
				sendLine(client_socket, "You cannot DM yourself.");
				continue;
			}
			SOCKET receiver_socket = INVALID_SOCKET; // Extract receiver socket from the DM message.
			{
				std::lock_guard<std::mutex> lock(mx);
				auto it = clients.find(target);
				if (it != clients.end()) receiver_socket = it->second;
			}
			if (receiver_socket == INVALID_SOCKET) {
				sendLine(client_socket, "User not found: " + target);
				continue;
			}
			sendLine(receiver_socket, "(DM) " + username + ": " + message);
			sendLine(client_socket, "(DM to " + target + ") " + message);
			continue; // DMing functionality
		}
		broadcastAll(username + ": " + line); // If not DM, simply broadcast message to all (including the user who sent it so they can see it on their screen)
	}
}

// Main function. Initialises WinSock. Creates a server socket. Binds the socket to an address and port (65432)
// Listens for connections, starts a thread to ClientAdd on connection.
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