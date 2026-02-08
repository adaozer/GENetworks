#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <functional>

enum class SoundEvent { Broadcast, DM }; // Play a different sound based on if its a DM or a Broadcast

class GUI {
public:
    std::vector<std::string> users;
    std::queue<SoundEvent> sounds;
    std::vector<std::string> roomMessages;
    std::unordered_map<std::string, std::vector<std::string>> DMs;

    char roomInput[1024] = "";
    char dmInput[1024] = "";

    bool dmOpen = false;
    bool roomAutoScroll = true;

    int selectedUser = -1;

    std::mutex mx;
    std::queue<std::string> incomingMessages;

    void pushToQueue(std::string line);

    bool popSoundEvent(SoundEvent& out);

    void getMessages(const std::string& self);
};

void DrawChatUI(
    GUI& state,
    const std::string& selfName,
    const std::function<void(const std::string&)>& sendBroadcast,
    const std::function<void(const std::string&, const std::string&)>& sendUnicast
);
