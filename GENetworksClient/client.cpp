#include "client.h"
#include "gui.h"

// Same sendall, sendline and stripCR functions as the server
// Helpers to make sure all bytes are sent and they have a \n at the end of the sentence and no \r characters
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

// Receive message from server and push it into the queue of the UI to be displayed
void receiveMessage(SOCKET s, std::atomic<bool>& running, GUI& ui) {
    std::string buf;
    char buff[1048];

    while (running.load()) {
        int received = recv(s, buff, sizeof(buff), 0); // Receive from server while the socket is running
        if (received <= 0) {
            running.store(false);
            break;
        }

        buf.append(buff, buff + received);

        while(1) {
            size_t pos = buf.find('\n'); // See if the message is complete
            if (pos == std::string::npos) break;

            std::string line = buf.substr(0, pos);
            buf.erase(0, pos + 1);

            stripCR(line);

            ui.pushToQueue(std::move(line)); // push to GUI
        }
    }

    if (!buf.empty()) {
        stripCR(buf);
        ui.pushToQueue(std::move(buf));
    }
}

// Function to connect to server. Takes in a socket reference as the client socket and host/port information and connects
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

// Create a thread to start receiving messages from the server
void startReceive(SOCKET sock, std::atomic<bool>& running, GUI& ui, std::thread& t) {
    if (t.joinable())
        t.join();
    running.store(true);
    t = std::thread(receiveMessage, sock, std::ref(running), std::ref(ui));
}