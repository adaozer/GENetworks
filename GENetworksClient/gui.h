#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <functional>

struct ChatUIState
{
    std::vector<std::string> users;
    int selectedUser = -1;

    std::vector<std::string> roomMessages;

    std::unordered_map<std::string, std::vector<std::string>> dmMessages;

    char roomInput[512] = "";
    char dmInput[512] = "";

    bool dmOpen = false;

    std::mutex mx;
    std::queue<std::string> inbound;

    void pushInbound(std::string line)
    {
        std::lock_guard<std::mutex> lock(mx);
        inbound.push(std::move(line));
    }

    void pumpInbound();
};

void DrawChatUI(
    ChatUIState& state,
    const std::function<void(const std::string&)>& sendBroadcast,
    const std::function<void(const std::string&, const std::string&)>& sendUnicast
);
