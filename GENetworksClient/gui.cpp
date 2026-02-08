#include "gui.h"
#include "imgui.h"

#include <algorithm>

#include <cctype>

static inline void trim(std::string& s)
{
    auto notSpace = [](unsigned char c) { return !std::isspace(c); };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
    s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
}


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
                dmMessages[from].push_back(line);
            }
            continue;
        }

        if (line.rfind("(DM to ", 0) == 0)
        {
            size_t close = line.find(')');
            if (close != std::string::npos)
            {
                std::string to = line.substr(7, close - 7);
                trim(to);
                dmMessages[to].push_back(line);
            }
            continue;
        }
        roomMessages.push_back(line);

        if (roomMessages.size() > 5000)
            roomMessages.erase(roomMessages.begin());
    }
}

void DrawChatUI(
    ChatUIState& st,
    const std::function<void(const std::string&)>& sendBroadcast,
    const std::function<void(const std::string&, const std::string&)>& sendUnicast)
{
    st.pumpInbound();

    ImGui::SetNextWindowSize(ImVec2(1100, 650), ImGuiCond_FirstUseEver);
    ImGui::Begin("Chat Client");

    float footerHeight = ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("Users", ImVec2(220, -footerHeight), true);
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

    ImGui::BeginChild("Room", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()), true);

    float scrollY = ImGui::GetScrollY();
    float scrollMaxY = ImGui::GetScrollMaxY();
    bool wasAtBottom = (scrollY >= scrollMaxY - 5.0f);

    for (const auto& msg : st.roomMessages)
        ImGui::TextWrapped("%s", msg.c_str());

    // Auto-scroll only if we were already at bottom
    if (wasAtBottom)
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();

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
            std::string to = user;
            trim(to);
            sendUnicast(user, st.dmInput);
            st.dmInput[0] = '\0';
        }

        ImGui::End();
    }
}
