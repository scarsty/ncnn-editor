#include "node_editor.h"

#include <imnodes.h>
#include <imgui.h>

#include <SDL.h>
#include <SDL_keycode.h>
#include <SDL_timer.h>
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <vector>
#include <string>
#include <imgui_stdlib.h>

#include "INIReader.h"
#include "fmt1.h"
#include "convert.h"
#include "File.h"

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

namespace example
{
namespace ex1
{

static float current_time_seconds = 0.f;
static bool  emulate_three_button_mouse = false;

class ColorNodeEditor
{
private:
    enum class UiNodeType
    {
        input,
        output,
        fc,
        conv,
        pool,
    };

    struct UiNode
    {
        UiNodeType type;
        // The identifying id of the ui node. For add, multiply, sine, and time
        // this is the "operation" node id. The additional text_id nodes are
        // stored in the structs.
        int id;
        std::string title, text;
        int text_id;
    };
    struct Link
    {
        int from, to;
        Link(int f, int t) { from = f; to = t; }
    };

    //Graph<Node> graph_;
    std::vector<UiNode> nodes_;
    std::vector<Link> links_;
    int root_node_id_;
    ImNodesMiniMapLocation minimap_location_;
    INIReaderNormal ini_;
    std::string current_file_;
    bool saved_ = true;
    UiNode& createUiNode()
    {
        static int n = 0;
        nodes_.emplace_back();
        auto& node = nodes_.back();
        node.id = n * 2;
        node.text_id = n * 2 + 1;
        n++;
        return node;
    }
    bool check_can_link(int from, int to)
    {
        int link_count = 0;
        for (auto& link : links_)
        {
            if (link.to == to)
            {
                link_count++;
            }
        }
        if (link_count >= 1)
        {
            for (auto& node : nodes_)
            {
                if (node.text_id == to && node.type == UiNodeType::conv)
                {
                    return true;
                }
            }
            return false;
        }
        return true;
    }
    std::string openfile()
    {
#ifdef _WIN32
        OPENFILENAMEA ofn;
        char szFile[1024];
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.lpstrFile[0] = '\0';
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "INI\0*.ini\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL;
        ofn.Flags = OFN_PATHMUSTEXIST;
        if (GetOpenFileNameA(&ofn))
        {
            return szFile;
        }
        else
        {
            return "";
        }
#endif
        return "";
    }
    void refresh_ini()
    {
        if (check_same_name()) { return; }
        auto ini0 = ini_;
        for (auto& section : ini_.getAllSections())
        {
            if (section.find("layer_") == 0)
            {
                ini_.eraseSection(section);
            }
        }
        std::map<int, std::string> m1;
        for (int i = 0; i < nodes_.size(); i++)
        {
            auto pos = ImNodes::GetNodeGridSpacePos(nodes_[i].id);
            auto str = fmt1::format("[{}]\n{}\neditor_position={}, {}", nodes_[i].title, nodes_[i].text, pos.x, pos.y);
            ini_.loadString(str, false);
            INIReaderNormal ini1;
            ini1.loadString(str, false);
            m1[nodes_[i].id] = ini1.getAllSections()[0];
            m1[nodes_[i].text_id] = ini1.getAllSections()[0];
            auto section = nodes_[i].title;
            ini_.setKey(section, "type", ini0.getString(section, "type"));
        }
        std::map<std::string, std::string> m2;
        for (const auto& link : links_)
        {
            m2[m1[link.from]] += m1[link.to] + ", ";
        }
        for (auto& kv : m2)
        {
            auto str = fmt1::format("[{}]\nnext={}", kv.first, kv.second);
            str.pop_back();
            str.pop_back();
            ini_.loadString(str, false);
        }
    }

    bool check_same_name()
    {
        bool res = false;
        std::map<std::string, int> m1;
        for (auto& node : nodes_)
        {
            m1[node.title]++;
        }
        for (auto& kv : m1)
        {
            if (kv.second > 1)
            {
                res = true;
                auto str = fmt1::format(u8"有{}个\"{}\"!", kv.second, kv.first);
                const SDL_MessageBoxButtonData buttons[] =
                {
                    //{ /* .flags, .buttonid, .text */ 0, 0, "no" },
                    { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, u8"知道了" },
                    //{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 2, "cancel" },
                };
                const SDL_MessageBoxColorScheme colorScheme =
                { { { 255, 0, 0 }, { 0, 255, 0 }, { 255, 255, 0 }, { 0, 0, 255 }, { 255, 0, 255 } } };
                const SDL_MessageBoxData messageboxdata =
                {
                    SDL_MESSAGEBOX_ERROR, /* .flags */
                    NULL,                       /* .window */
                    u8"错误",               /* .title */
                    str.c_str(),            /* .message */
                    SDL_arraysize(buttons),     /* .numbuttons */
                    buttons,                    /* .buttons */
                    &colorScheme                /* .colorScheme */
                };
                int buttonid;
                SDL_ShowMessageBox(&messageboxdata, &buttonid);
            }
        }
        return res;
    }
    void try_save(bool force = false)
    {
        if (saved_ == false || force)
        {
            refresh_ini();
            if (current_file_.empty())
            {
                auto file = openfile();
                if (!file.empty())
                {
                    if (File::getFileExt(file) != "ini")
                    {
                        file = File::changeFileExt(file, "ini");
                    }
                    current_file_ = file;
                    ini_.saveFile(current_file_);
                    saved_ = true;
                }
            }
            else
            {
                ini_.saveFile(current_file_);
                saved_ = true;
            }
        }
    }
    void try_exit()
    {
        if (saved_)
        {
            exit(0);
        }
        //ImGui::OpenPopup("popup");
        //if (ImGui::BeginPopupModal("popup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        //{
        //    ImGui::Text("Hello dsjfhds fhjs hfj dshfj hds");
        //    if (ImGui::Button("Close"))
        //        ImGui::CloseCurrentPopup();
        //    ImGui::EndPopup();
        //}


        const SDL_MessageBoxButtonData buttons[] =
        {
            { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 3, u8"取消" },
            { 0, 2, u8"否" },
            { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, u8"是" },
        };
        const SDL_MessageBoxColorScheme colorScheme =
        { { { 255, 0, 0 }, { 0, 255, 0 }, { 255, 255, 0 }, { 0, 0, 255 }, { 255, 0, 255 } } };
        const SDL_MessageBoxData messageboxdata =
        {
            SDL_MESSAGEBOX_INFORMATION, /* .flags */
            NULL,                       /* .window */
            u8"提示",               /* .title */
            u8"是否保存？",            /* .message */
            SDL_arraysize(buttons),     /* .numbuttons */
            buttons,                    /* .buttons */
            &colorScheme                /* .colorScheme */
        };
        int buttonid =3;
        SDL_ShowMessageBox(&messageboxdata, &buttonid);
        if (buttonid == 1)
        {
            try_save();
            exit(0);
        }
        else if (buttonid == 2)
        {
            exit(0);
        }
    }

public:
    SDL_Event event;

    ColorNodeEditor() : nodes_(), root_node_id_(-1),
        minimap_location_(ImNodesMiniMapLocation_BottomRight) {}

    void show()
    {
        // Update timer context
        current_time_seconds = 0.001f * SDL_GetTicks();

        auto flags = ImGuiWindowFlags_MenuBar;

        // The node editor window
        std::string window_title = u8"网络结构编辑";
        ImGui::SetWindowSize(window_title.c_str(), ImGui::GetIO().DisplaySize);
        ImGui::Begin(window_title.c_str(), NULL, flags);

        //if (ImGui::)   //close window

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu(u8"文件"))
            {
                if (ImGui::MenuItem(u8"打开..."))
                {
                    auto file = openfile();
                    if (!file.empty())
                    {
                        nodes_.clear();
                        ini_ = INIReaderNormal();
                        ini_.loadFile(file);
                        current_file_ = file;
                        ImVec2 pos;
                        pos.x = 100, pos.y = 200;
                        std::string prefix = "layer_";
                        std::map<std::string, UiNode> dd;
                        //ImNodes::BeginNodeEditor();
                        // restore mod
                        auto sections = ini_.getAllSections();
                        std::sort(sections.begin(), sections.end(), [this](const std::string& l, const std::string& r)
                        {
                            return ini_.getSectionNo(l) < ini_.getSectionNo(r);
                        });
                        for (auto& section : sections)
                        {
                            size_t size = prefix.size();
                            if (section.find(prefix) == 0)
                            {
                                auto& ui_node = createUiNode();
                                ui_node.type = UiNodeType::fc;
                                std::string name = section.substr(size);
                                std::string type = ini_.getString(section, "type");
                                if (type.find("null") == 0)
                                {
                                    ui_node.type = UiNodeType::input;
                                }
                                else if (type.find("out") == 0)
                                {
                                    ui_node.type = UiNodeType::output;
                                    root_node_id_ = ui_node.id;
                                }
                                else if (type.find("fc") == 0)
                                {
                                    ui_node.type = UiNodeType::pool;
                                }
                                else if (type.find("conv") == 0)
                                {
                                    ui_node.type = UiNodeType::conv;
                                }
                                else if (type.find("pool") == 0)
                                {
                                    ui_node.type = UiNodeType::pool;
                                }
                                ui_node.title = section;
                                for (auto& key : ini_.getAllKeys(section))
                                {
                                    if (key != "next" && key != "editor_position" && key != "type")
                                    {
                                        ui_node.text += key + "=" + ini_.getString(section, key) + "\n";
                                    }
                                }
                                if (!ui_node.text.empty())
                                {
                                    ui_node.text.pop_back();
                                }
                                if (ini_.hasKey(section, "editor_position"))
                                {
                                    std::vector<int> v = convert::findNumbers<int>(ini_.getString(section, "editor_position"));
                                    if (v.size() >= 2)
                                    {
                                        pos.x = v[0];
                                        pos.y = v[1];
                                    }
                                }
                                ImNodes::SetNodeGridSpacePos(ui_node.id, pos);
                                pos.x += 200;
                                pos.y += 20;
                                dd[section] = ui_node;
                            }
                        }
                        // restore link
                        links_.clear();
                        for (auto& section : ini_.getAllSections())
                        {
                            size_t size = prefix.size();
                            if (section.find(prefix) == 0)
                            {
                                if (ini_.hasKey(section, "next"))
                                {
                                    auto nexts = convert::splitString(ini_.getString(section, "next"), ",");
                                    for (auto sec1 : nexts)
                                    {
                                        if (check_can_link(dd[section].id, dd[sec1].text_id))
                                        {
                                            links_.emplace_back(dd[section].id, dd[sec1].text_id);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    saved_ = true;
                }
                if (ImGui::MenuItem(u8"保存"))
                {
                    try_save(true);
                }
                if (ImGui::MenuItem(u8"另存为..."))
                {
                    refresh_ini();
                    auto file = openfile();
                    if (!file.empty())
                    {
                        if (File::getFileExt(file) != "ini")
                        {
                            file = File::changeFileExt(file, "ini");
                        }
                        current_file_ = file;
                        ini_.saveFile(current_file_);
                        saved_ = true;
                    }
                }
                if (ImGui::MenuItem(u8"退出"))
                {
                    try_exit();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu(u8"缩略图位置"))
            {
                const char* names[] = {
                    u8"左上",
                    u8"右上",
                    u8"左下",
                    u8"右下",
                };
                int locations[] = {
                    ImNodesMiniMapLocation_TopLeft,
                    ImNodesMiniMapLocation_TopRight,
                    ImNodesMiniMapLocation_BottomLeft,
                    ImNodesMiniMapLocation_BottomRight,
                };

                for (int i = 0; i < 4; i++)
                {
                    bool selected = minimap_location_ == locations[i];
                    if (ImGui::MenuItem(names[i], NULL, &selected))
                        minimap_location_ = locations[i];
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu(u8"样式"))
            {
                if (ImGui::MenuItem(u8"经典"))
                {
                    ImGui::StyleColorsClassic();
                    ImNodes::StyleColorsClassic();
                }
                if (ImGui::MenuItem(u8"暗"))
                {
                    ImGui::StyleColorsDark();
                    ImNodes::StyleColorsDark();
                }
                if (ImGui::MenuItem(u8"亮"))
                {
                    ImGui::StyleColorsLight();
                    ImNodes::StyleColorsLight();
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }
        {
        //ImGui::Columns(2);
        //ImGui::TextUnformatted("A -- add node");
            ImGui::TextUnformatted(u8"Delete -- 删除选中的层或连接");
            //ImGui::NextColumn();
            std::string str = u8"没有打开的文件";
            if (!current_file_.empty())
            {
                str = fmt1::format(u8"当前文件：{}，", current_file_);
                if (saved_)
                {
                    str += u8"已保存";
                }
                else
                {
                    str += u8"未保存";
                }
            }
            ImGui::TextUnformatted(str.c_str());
        }
        //if (ImGui::Checkbox("emulate_three_button_mouse", &emulate_three_button_mouse))
        //{
        //    ImNodes::GetIO().EmulateThreeButtonMouse.Modifier =
        //        emulate_three_button_mouse ? &ImGui::GetIO().KeyAlt : NULL;
        //}
        //ImGui::Columns(1);

        ImNodes::BeginNodeEditor();

        // Handle new nodes
        // These are driven by the user, so we place this code before rendering the nodes
        {
            const bool open_popup = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                ImNodes::IsEditorHovered() &&
                (ImGui::IsMouseReleased(1));

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));
            if (!ImGui::IsAnyItemHovered() && open_popup)
            {
                ImGui::OpenPopup("add node");
            }

            if (ImGui::BeginPopup("add node"))
            {
                const ImVec2 click_pos = ImGui::GetMousePosOnOpeningCurrentPopup();
                if (ImGui::MenuItem(u8"输入"))
                {
                    int count = 0;
                    for (auto& node : nodes_)
                    {
                        if (node.title.find("layer_in") == 0) { count++; }
                    }
                    auto& ui_node = createUiNode();
                    ui_node.title = "layer_in" + std::to_string(count);
                    ui_node.type = UiNodeType::input;
                    ImNodes::SetNodeScreenSpacePos(ui_node.id, click_pos);
                    saved_ = false;
                }
                if (ImGui::MenuItem(u8"输出") && root_node_id_ == -1)
                {
                    auto& ui_node = createUiNode();
                    ui_node.type = UiNodeType::output;
                    ui_node.title = "layer_out";
                    ImNodes::SetNodeScreenSpacePos(ui_node.id, click_pos);
                    root_node_id_ = ui_node.id;
                    saved_ = false;
                }
                if (ImGui::MenuItem(u8"全连接"))
                {
                    auto& ui_node = createUiNode();
                    ui_node.type = UiNodeType::fc;
                    ui_node.title = fmt1::format("layer_fc{}", rand());
                    ImNodes::SetNodeScreenSpacePos(ui_node.id, click_pos);
                    saved_ = false;
                }
                if (ImGui::MenuItem(u8"卷积"))
                {
                    auto& ui_node = createUiNode();
                    ui_node.type = UiNodeType::conv;
                    ui_node.title = fmt1::format("layer_conv{}", rand());
                    ImNodes::SetNodeScreenSpacePos(ui_node.id, click_pos);
                    saved_ = false;
                }
                if (ImGui::MenuItem(u8"池化"))
                {
                    auto& ui_node = createUiNode();
                    ui_node.type = UiNodeType::pool;
                    ui_node.title = fmt1::format("layer_pool{}", rand());
                    ImNodes::SetNodeScreenSpacePos(ui_node.id, click_pos);
                    saved_ = false;
                }
                ImGui::EndPopup();
            }
            ImGui::PopStyleVar();
        }

        for (auto& node : nodes_)
        {
            switch (node.type)
            {
            case UiNodeType::output:
            {
                ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(11, 109, 191, 255));
            }
            break;
            case UiNodeType::input:
            {
                ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(0xcc, 0x33, 0x33, 0xff));
            }
            break;
            case UiNodeType::fc:
            {
                ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(0xff, 0xcc, 0x99, 0xff));
            }
            break;
            case UiNodeType::conv:
            {
                ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(0x00, 0x99, 0xcc, 0xff));
            }
            break;
            case UiNodeType::pool:
            {
                ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(0x99, 0xcc, 0x66, 0xff));
            }
            break;
            }
            const float node_width = 150;
            ImNodes::BeginNode(node.id);

            ImNodes::BeginNodeTitleBar();
            ImGui::PushItemWidth(node_width);
            ImGui::InputText("##hidelabel", &node.title);
            ImNodes::EndNodeTitleBar();

            ImNodes::BeginInputAttribute(node.text_id);
            //if (graph_.num_edges_from_node(node.text_id) == 0ull)
            {
                ImGui::PushItemWidth(node_width);
                ImGui::InputTextMultiline("##hidelabel", &node.text, ImVec2(node_width, 50));
                ImGui::PopItemWidth();
            }
            ImNodes::EndInputAttribute();
            ImGui::Spacing();
            {
                ImNodes::BeginOutputAttribute(node.id);
                const float label_width = ImGui::CalcTextSize("next").x;
                ImGui::Indent(node_width - label_width);
                ImGui::TextUnformatted("next");
                ImNodes::EndInputAttribute();
            }
            ImNodes::EndNode();
            ImNodes::PopColorStyle();
            //ImNodes::PopColorStyle();
            //ImNodes::PopColorStyle();
        }

        {
            int link_id = 0;
            for (const auto& link : links_)
            {
                ImNodes::Link(link_id++, link.from, link.to);
            }
        }
        ImNodes::MiniMap(0.2f, minimap_location_);
        ImNodes::EndNodeEditor();

        // Handle new links
        // These are driven by Imnodes, so we place the code after EndNodeEditor().

        {
            int start_attr, end_attr;
            if (ImNodes::IsLinkCreated(&start_attr, &end_attr))
            {
                if (start_attr % 2)
                {
                    // Ensure the edge is always directed from the text to
                    // whatever produces the text      
                    std::swap(start_attr, end_attr);
                }
                if (check_can_link(start_attr, end_attr))
                {
                    links_.emplace_back(start_attr, end_attr);
                }
                saved_ = false;
            }
        }

        // Handle deleted links

        {
            int link_id;
            if (ImNodes::IsLinkDestroyed(&link_id))
            {
                links_.erase(links_.begin() + link_id);
                saved_ = false;
            }
        }

        {
            const int num_selected = ImNodes::NumSelectedLinks();
            if (num_selected > 0 && ImGui::IsKeyReleased(SDL_SCANCODE_DELETE) && !ImGui::IsAnyItemActive())
            {
                static std::vector<int> selected_links;
                selected_links.resize(static_cast<size_t>(num_selected));
                ImNodes::GetSelectedLinks(selected_links.data());
                for (const int link_id : selected_links)
                {
                    links_.erase(links_.begin() + link_id);
                }
                saved_ = false;
            }
        }

        {
            const int num_selected = ImNodes::NumSelectedNodes();
            if (num_selected > 0 && ImGui::IsKeyReleased(SDL_SCANCODE_DELETE) && !ImGui::IsAnyItemActive())
            {
                static std::vector<int> selected_nodes;
                selected_nodes.resize(static_cast<size_t>(num_selected));
                ImNodes::GetSelectedNodes(selected_nodes.data());
                for (const int node_id : selected_nodes)
                {
                    auto iter = std::find_if(nodes_.begin(), nodes_.end(), [node_id](const UiNode& node) -> bool
                    {
                        return node.id == node_id;
                    });
                    nodes_.erase(iter);
                    for (auto it = links_.begin(); it != links_.end();)
                    {
                        if (it->from == node_id || it->to == node_id + 1)
                        {
                            it = links_.erase(it);
                        }
                        else
                        {
                            it++;
                        }
                    }
                }
                saved_ = false;
            }
        }

        if (ImGui::IsItemEdited())
        {
            saved_ = false;
        }

        if (ImGui::IsKeyReleased(SDL_SCANCODE_S) && ImGui::GetIO().KeyCtrl)
        {
            try_save(true);
        }

        ImGui::End();

        if (event.type == SDL_QUIT
            || event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)
        {
            try_exit();
        }
    }
};

static ColorNodeEditor color_editor;
} // namespace

void NodeEditorInitialize()
{
    ImGui::StyleColorsLight();
    //auto& io = ImGui::GetIO();
    //io.Fonts->AddFontFromFileTTF("c:/windows/fonts/msyh.ttc", 15.0f, NULL, io.Fonts->GetGlyphRangesChineseSimplifiedCommon());
    ImNodes::StyleColorsLight();
    ImNodesIO& ion = ImNodes::GetIO();
    ion.LinkDetachWithModifierClick.Modifier = &ImGui::GetIO().KeyCtrl;
    srand(time(0));
#ifdef IMGUI_HAS_VIEWPORT
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetWorkPos());
    ImGui::SetNextWindowSize(viewport->GetWorkSize());
    ImGui::SetNextWindowViewport(viewport->ID);
#else 
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
#endif
}

void NodeEditorShow() { ex1::color_editor.show(); }

void NodeEditorSetEvent(void* e) { ex1::color_editor.event = *(SDL_Event*)e; }

void NodeEditorShutdown() {}
} // namespace example
