#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <unordered_map>
#include <mutex>
#include <string>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

std::unordered_map<std::string, SOCKET> clients;
std::unordered_map<SOCKET, std::string> clientSockets;
std::mutex mx;
