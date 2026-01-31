#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <algorithm>

#pragma comment(lib, "Ws2_32.lib")

std::mutex mx;
