#include "client.h"
#include "gui.h"

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

void stripCR(std::string& s) {
    s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
}

void receiveMessage(SOCKET s, std::atomic<bool>& running, ChatUIState& ui) {
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

            stripCR(line);

            ui.pushInbound(std::move(line));
        }
    }

    if (!buf.empty()) {
        stripCR(buf);
        ui.pushInbound(std::move(buf));
    }
}

bool connectToServer(SOCKET& sock, const char* host, unsigned int port) {
    sock = INVALID_SOCKET;

    SOCKET client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
        return false;
    }

    sockaddr_in server_address = {};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &server_address.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        closesocket(client_socket);
        return false;
    }

    if (connect(client_socket, reinterpret_cast<sockaddr*>(&server_address), sizeof(server_address)) == SOCKET_ERROR) {
        std::cerr << "Connection failed with error: " << WSAGetLastError() << std::endl;
        closesocket(client_socket);
        return false;
    }

    sock = client_socket;
    return true;
}

void startReceive(SOCKET sock, std::atomic<bool>& running, ChatUIState& ui, std::thread& t) {
    if (t.joinable())
        t.join();
    running.store(true);
    t = std::thread(receiveMessage, sock, std::ref(running), std::ref(ui));
}