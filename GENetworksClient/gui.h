#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <functional>

enum class SoundEvent { Broadcast, DM };

struct ChatUIState
{
    std::vector<std::string> users;
    int selectedUser = -1;
    std::queue<SoundEvent> soundEvents;

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
    bool popSoundEvent(SoundEvent& out)
    {
        std::lock_guard<std::mutex> lock(mx);
        if (soundEvents.empty()) return false;
        out = soundEvents.front();
        soundEvents.pop();
        return true;
    }
    void pumpInbound(const std::string& selfName);
    bool roomAutoScroll = true;
};

void DrawChatUI(
    ChatUIState& state,
    const std::string& selfName,
    const std::function<void(const std::string&)>& sendBroadcast,
    const std::function<void(const std::string&, const std::string&)>& sendUnicast
);
