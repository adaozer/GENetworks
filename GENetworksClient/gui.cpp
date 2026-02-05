#include "gui.h"
#include "imgui.h"

#include <algorithm>

// ---------------- Inbound processing ----------------
void ChatUIState::pumpInbound()
{
    std::queue<std::string> local;
    {
        std::lock_guard<std::mutex> lock(mx);
        std::swap(local, inbound);
    }

    while (!local.empty())
    {
        std::string line = std::move(local.front());
        local.pop();

        // USERS list from server
        if (line.rfind("USERS ", 0) == 0)
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

        // Private messages
        if (line.rfind("(DM)", 0) == 0 || line.rfind("(DM to ", 0) == 0)
        {
            // Just store whole line; simple and robust
            size_t colon = line.find(':');
            std::string key = (colon == std::string::npos) ? "DM" : line.substr(0, colon);
            dmMessages[key].push_back(line);
            continue;
        }

        // Room message
        roomMessages.push_back(line);

        if (roomMessages.size() > 5000)
            roomMessages.erase(roomMessages.begin());
    }
}

// ---------------- UI drawing ----------------
void DrawChatUI(
    ChatUIState& st,
    const std::function<void(const std::string&)>& sendBroadcast,
    const std::function<void(const std::string&, const std::string&)>& sendUnicast)
{
    st.pumpInbound();

    ImGui::SetNextWindowSize(ImVec2(900, 520), ImGuiCond_FirstUseEver);
    ImGui::Begin("Chat Client");

    // ---- LEFT: USERS ----
    ImGui::BeginChild("Users", ImVec2(220, 0), true);
    ImGui::Text("Active Users");
    ImGui::Separator();

    for (int i = 0; i < (int)st.users.size(); i++)
    {
        bool selected = (st.selectedUser == i);
        if (ImGui::Selectable(st.users[i].c_str(), selected))
            st.selectedUser = i;

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            st.dmOpen = true;
    }

    if (st.selectedUser >= 0)
    {
        ImGui::Spacing();
        if (ImGui::Button("Private Message"))
            st.dmOpen = true;
    }

    ImGui::EndChild();
    ImGui::SameLine();

    // ---- RIGHT: ROOM CHAT ----
    ImGui::BeginChild("Room", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true);

    for (const auto& msg : st.roomMessages)
        ImGui::TextWrapped("%s", msg.c_str());

    ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();

    // Input
    ImGui::PushItemWidth(-80);
    bool send = ImGui::InputText(
        "##room",
        st.roomInput,
        sizeof(st.roomInput),
        ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopItemWidth();

    ImGui::SameLine();
    if (ImGui::Button("Send")) send = true;

    if (send && st.roomInput[0])
    {
        sendBroadcast(st.roomInput);
        st.roomInput[0] = '\0';
    }

    ImGui::End();

    // ---- PRIVATE WINDOW ----
    if (st.dmOpen && st.selectedUser >= 0 && st.selectedUser < (int)st.users.size())
    {
        const std::string& user = st.users[st.selectedUser];
        std::string title = "Private Chat: " + user;

        ImGui::Begin(title.c_str(), &st.dmOpen);

        auto& log = st.dmMessages[user];
        for (auto& msg : log)
            ImGui::TextWrapped("%s", msg.c_str());

        ImGui::Separator();

        bool sendDM = ImGui::InputText(
            "##dm",
            st.dmInput,
            sizeof(st.dmInput),
            ImGuiInputTextFlags_EnterReturnsTrue);

        ImGui::SameLine();
        if (ImGui::Button("Send")) sendDM = true;

        if (sendDM && st.dmInput[0])
        {
            sendUnicast(user, st.dmInput);
            log.push_back("Me: " + std::string(st.dmInput));
            st.dmInput[0] = '\0';
        }

        ImGui::End();
    }
}
