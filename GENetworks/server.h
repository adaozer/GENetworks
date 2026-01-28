#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <unordered_map>
#include <string>

std::unordered_map<std::string, sockaddr*> clients;
int main();
void clientAdd(sockaddr*);