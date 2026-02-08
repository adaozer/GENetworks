#include "gui.h"
#include "imgui.h"

#include <algorithm>
#include <cctype>

// Remove empty space
void trim(std::string& s)
{
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
}

// Pops the sound event from the sound events queue after its played.
bool GUI::popSoundEvent(SoundEvent& out)
{
    std::lock_guard<std::mutex> lock(mx);
    if (sounds.empty()) return false;
    out = sounds.front();
    sounds.pop();
    return true;
}

// Used to get incoming information and process it onto the GUI.
// Checks information in messages to see if they're updating the users, DMing, or simply broadcasting.
void GUI::getMessages(const std::string& self)
{
    std::queue<std::string> local;
    {
        std::lock_guard<std::mutex> lock(mx);
        std::swap(local, incomingMessages);
    }

    while (!local.empty())
    {
        std::string line = std::move(local.front());
        local.pop();

        if (line.rfind("USERS ", 0) == 0) // This is used to build the clients list on the left side.
        {
            users.clear();
            std::string list = line.substr(6);

            size_t start = 0;
            while (start < list.size())
            {
                size_t comma = list.find(',', start);
                std::string name = (comma == std::string::npos)
                    ? list.substr(start)
                    : list.substr(start, comma - start);
                trim(name);
                if (!name.empty())
                    users.push_back(name);

                if (comma == std::string::npos) break;
                start = comma + 1;
            }

            std::sort(users.begin(), users.end());
            if (selectedUser >= (int)users.size())
                selectedUser = -1;

            continue;
        }

        if (line.rfind("(DM) ", 0) == 0)
        {
            size_t nameStartIndex = 5;
            size_t colon = line.find(':', nameStartIndex);
            if (colon != std::string::npos)
            {
                std::string from = line.substr(nameStartIndex, colon - nameStartIndex);
                trim(from);
                DMs[from].push_back(line);
                if (from != self) {
                    std::lock_guard<std::mutex> lock(mx);
                    sounds.push(SoundEvent::DM);
                }
            }
            continue; // DM logic, find sender
        }

        if (line.rfind("(DM to ", 0) == 0)
        {
            size_t close = line.find(')');
            if (close != std::string::npos)
            {
                std::string to = line.substr(7, close - 7);
                trim(to);
                DMs[to].push_back(line);

            }
            continue; // DM logic, find receiver.
        }
        size_t colon = line.find(": ");
        if (colon != std::string::npos)
        {
            std::string from = line.substr(0, colon);
            trim(from);

            if (!from.empty() && from != self)
            {
                std::lock_guard<std::mutex> lock(mx);
                sounds.push(SoundEvent::Broadcast);
            }
        }

        roomMessages.push_back(line); // Broadcast messages

        if (roomMessages.size() > 5000)
            roomMessages.erase(roomMessages.begin());
    }
}

// Push a message into the queue
void GUI::pushToQueue(std::string line)
{
    std::lock_guard<std::mutex> lock(mx);
    incomingMessages.push(std::move(line));
}

// This draws the whole GUI. Takes the GUI class, the name of the user, and sendBroadcast and sendUnicast functions as args.
// Builds and draws the whole UI, uses the broadcast and unicast functions to link the client/server code and the GUI.
void DrawChatUI(GUI& ui, const std::string& username, const std::function<void(const std::string&)>& sendBroadcast, const std::function<void(const std::string&, const std::string&)>& sendUnicast)
{
    ui.getMessages(username); // Update GUI state from any newly received network messages
    ImGui::SetNextWindowSize(ImVec2(1100, 650), ImGuiCond_FirstUseEver);
    ImGui::Begin("Chat Client");

    float footerHeight = ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("Users", ImVec2(220, -footerHeight), true);
    ImGui::Text("Active Users");
    ImGui::Separator();

    for (int i = 0; i < (int)ui.users.size(); i++)
    {
        bool selected = (ui.selectedUser == i);
        if (ImGui::Selectable(ui.users[i].c_str(), selected))
            ui.selectedUser = i;

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            ui.dmOpen = true;
    }

    if (ui.selectedUser >= 0)
    {
        ImGui::Spacing();
        if (ImGui::Button("Private Message"))
            ui.dmOpen = true;
    }

    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("Room", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true);

    float scrollY = ImGui::GetScrollY();
    float scrollMaxY = ImGui::GetScrollMaxY();
    bool wasAtBottom = (scrollY >= scrollMaxY - 5.0f); // Keep scroll pinned to bottom unless the user has manually scrolled up

    for (const auto& msg : ui.roomMessages)
        ImGui::TextWrapped("%s", msg.c_str());

    if (wasAtBottom)
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();

    ImGui::PushItemWidth(-80);
    bool send = ImGui::InputText(
        "##room",
        ui.roomInput,
        sizeof(ui.roomInput),
        ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopItemWidth();

    ImGui::SameLine();
    if (ImGui::Button("Send")) send = true;

    if (send && ui.roomInput[0])
    {
        sendBroadcast(ui.roomInput);
        ui.roomInput[0] = '\0';
    }

    ImGui::End();

    if (ui.dmOpen && ui.selectedUser >= 0 && ui.selectedUser < (int)ui.users.size())
    {
        const std::string& user = ui.users[ui.selectedUser];
        std::string title = "Private Chat: " + user;

        ImGui::Begin(title.c_str(), &ui.dmOpen);

        auto& log = ui.DMs[user];
        for (auto& msg : log)
            ImGui::TextWrapped("%s", msg.c_str());

        ImGui::Separator();

        bool sendDM = ImGui::InputText(
            "##dm",
            ui.dmInput,
            sizeof(ui.dmInput),
            ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::SameLine();
        if (ImGui::Button("Send")) sendDM = true;

        if (sendDM && ui.dmInput[0])
        {
            std::string to = user;
            trim(to);
            sendUnicast(user, ui.dmInput);
            ui.dmInput[0] = '\0';
        }

        ImGui::End();
    }
}
