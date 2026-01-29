#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <unordered_map>
#include <string>
#include <iostream>
#include <mutex>

std::unordered_map<std::string, SOCKET> clients;
std::mutex mx;
