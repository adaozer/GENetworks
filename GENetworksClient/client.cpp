#include "client.h"

static bool sendAll(SOCKET s, const char* data, int len) {
    int sentSum = 0;
    while (sentSum < len) {
        int sent = send(s, data + sentSum, len - sentSum, 0);
        if (sent == SOCKET_ERROR) return false;
        sentSum += sent;
    }
    return true;
}

static bool sendLine(SOCKET s, const std::string& line) {
    std::string out = line;
    if (out.empty() || out.back() != '\n') out.push_back('\n');
    return sendAll(s, out.c_str(), (int)out.size());
}

static std::string stripCR(std::string s) {
    s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
    return s;
}

static void receiveMessage(SOCKET s, std::atomic<bool>& running) {
    std::string buf;
    char buff[1048];

    while (running.load()) {
        int received = recv(s, buff, sizeof(buff), 0);
        if (received <= 0) {
            running.store(false);
            break;
        }

        buf.append(buff, buff + received);

        while(1) {
            int pos = buf.find('\n');
            if (pos == std::string::npos) break;

            std::string line = buf.substr(0, pos);
            buf.erase(0, pos + 1);

            line = stripCR(line);

            std::lock_guard<std::mutex> lock(mx);
            std::cout << line << "\n";
        }
    }

    if (!buf.empty()) {
        std::lock_guard<std::mutex> lock(mx);
        std::cout << stripCR(buf) << "\n";
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Please enter your username: " << argv[0] << " + username" << std::endl;
        return 1;
    }

    const std::string username = argv[1];
    const char* host = "127.0.0.1";
    unsigned int port = 65432;

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl;
        return 1;
    }

    SOCKET client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in server_address = {};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &server_address.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }

    if (connect(client_socket, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address)) == SOCKET_ERROR) {
        std::cerr << "Connection failed with error: " << WSAGetLastError() << std::endl;
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }

    if (!sendLine(client_socket, username)) {
        std::cerr << "Failed to send username." << std::endl;
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }

    std::atomic<bool> running(true);

    std::thread t(receiveMessage, client_socket, std::ref(running));

    std::string line;
    while (running.load() && std::getline(std::cin, line)) {
        if (!sendLine(client_socket, line)) {
            std::lock_guard<std::mutex> lock(mx);
            std::cerr << "Message failed to send (server likely closed)." << std::endl;
            running.store(false);
            break;
        }

        if (line == "/leave") {
            running.store(false);
            break;
        }
    }

    shutdown(client_socket, SD_BOTH);
    closesocket(client_socket);

    running.store(false);

    t.join();

    WSACleanup();
    return 0;
}
