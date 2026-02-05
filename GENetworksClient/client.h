#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>

#include <string>
#include <thread>
#include <atomic>
#include <algorithm>
#include <iostream>

#pragma comment(lib, "Ws2_32.lib")

struct ChatUIState;

bool sendAll(SOCKET s, const char* data, int len);
bool sendLine(SOCKET s, const std::string& line);

bool connectToServer(SOCKET& sock, const char* host, unsigned int port);
void startReceive(SOCKET sock, std::atomic<bool>& running, ChatUIState& ui, std::thread& t);
