// clients.cpp
// Simple TCP chat client for your line-based C++ server (Winsock).
// Usage: clients.exe <username> <host> <port>
// Example: clients.exe bob 127.0.0.1 65432

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

static std::mutex coutMx;

// Receives bytes, prints complete newline-delimited messages.
// Keeps partial data in a buffer until '\n' arrives.
static void recvLoop(SOCKET s, std::atomic<bool>& running) {
    std::string buf;
    buf.reserve(4096);

    char tmp[2048];

    while (running.load()) {
        int received = recv(s, tmp, (int)sizeof(tmp), 0);
        if (received <= 0) {
            running.store(false);
            break;
        }

        buf.append(tmp, tmp + received);

        for (;;) {
            size_t pos = buf.find('\n');
            if (pos == std::string::npos) break;

            std::string line = buf.substr(0, pos);
            buf.erase(0, pos + 1);

            line = stripCR(line);

            std::lock_guard<std::mutex> lock(coutMx);
            std::cout << line << "\n";
        }
    }

    // Flush any trailing bytes (optional)
    if (!buf.empty()) {
        std::lock_guard<std::mutex> lock(coutMx);
        std::cout << stripCR(buf) << "\n";
    }
}

static bool connectTo(const std::string& host, const std::string& portStr, SOCKET& outSock) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;     // IPv4 or IPv6
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
        std::cerr << "Usage: " << argv[0] << " <username> <host> <port>\n";
        return 1;
    }

    const std::string username = argv[1];
    const std::string host = argv[2];
    const std::string portStr = argv[3];

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << "\n";
        return 1;
    }

    SOCKET sock = INVALID_SOCKET;
    if (!connectTo(host, portStr, sock)) {
        WSACleanup();
        return 1;
    }

    // Send username as the first line (server expects newline framing).
    if (!sendLine(sock, username)) {
        std::cerr << "Failed to send username.\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    std::atomic<bool> running(true);

    // Start receiver thread.
    std::thread rx(recvLoop, sock, std::ref(running));

    // Main loop: read stdin, send lines.
    // Commands supported by your server: /leave, /who, /msg <user> <text>
    std::string line;
    while (running.load() && std::getline(std::cin, line)) {
        if (!sendLine(sock, line)) {
            std::lock_guard<std::mutex> lock(coutMx);
            std::cerr << "Send failed (server likely closed).\n";
            running.store(false);
            break;
        }

        if (line == "/leave") {
            running.store(false);
            break;
        }
    }

    // Graceful shutdown.
    // shutdown() helps unblock recv() on some stacks.
    shutdown(sock, SD_BOTH);
    closesocket(sock);

    running.store(false);

    if (rx.joinable()) rx.join();

    WSACleanup();
    return 0;
}
