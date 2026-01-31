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

static void recvLoop(SOCKET s, std::atomic<bool>& running) {
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
            size_t pos = buf.find('\n');
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

static bool connectTo(const std::string& host, const std::string& portStr, SOCKET& outSock) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;   
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = nullptr;
    int r = getaddrinfo(host.c_str(), portStr.c_str(), &hints, &result);
    if (r != 0) {
        std::cerr << "getaddrinfo failed: " << r << "\n";
        return false;
    }

    SOCKET s = INVALID_SOCKET;

    for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        s = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
        if (s == INVALID_SOCKET) continue;

        if (connect(s, ptr->ai_addr, (int)ptr->ai_addrlen) == SOCKET_ERROR) {
            closesocket(s);
            s = INVALID_SOCKET;
            continue;
        }

        break;
    }

    freeaddrinfo(result);

    if (s == INVALID_SOCKET) {
        std::cerr << "Unable to connect to " << host << ":" << portStr << "\n";
        return false;
    }

    outSock = s;
    return true;
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <username> <host> <port>" << std::endl;
        return 1;
    }

    const std::string username = argv[1];
    const std::string host = argv[2];
    const std::string port = argv[3];

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl;
        return 1;
    }

    SOCKET client_socket = INVALID_SOCKET;
    if (!connectTo(host, port, client_socket)) {
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

    std::thread t(recvLoop, client_socket, std::ref(running));
    std::string line;
    while (running.load() && std::getline(std::cin, line)) {
        if (!sendLine(client_socket, line)) {
            std::lock_guard<std::mutex> lock(mx);
            std::cerr << "Send failed (server likely closed)." << std::endl;
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
