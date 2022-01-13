#include "node_editor.h"

#include <imnodes.h>
#include <imgui.h>
#include <ImGuiFileDialog.h>

#include "node.h"

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

#include "fmt1.h"
#include "convert.h"
#include "File.h"

#include "loader.h"

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
    struct Link
    {
        int from, to;
        Link(int f, int t) { from = f; to = t; }
    };

    NodeLoader* loader_ = nullptr;
    std::deque<Node> nodes_;    //在一次编辑期间,只可增不可减
    std::vector<Link> links_;
    int root_node_id_;
    ImNodesMiniMapLocation minimap_location_;
    std::string current_file_;
    bool saved_ = true;
    int need_dialog_ = 0;    //1: when exist, 2: openfile to open
    std::string begin_file_;
    int first_run_ = 1;
    int select_id_ = -1;

    int erase_select_ = 0;

    Node& createNode()
    {
        int n = nodes_.size();
        nodes_.emplace_back();
        auto& node = nodes_.back();
        node.id = n * Node::MAX_PIN;
        node.text_id = n * Node::MAX_PIN + Node::HALF_MAX_PIN;
        n++;
        return node;
    }

    bool check_can_link(int from, int to, bool manully = false)
    {
        //allow one pin has only one link
        if (manully)
        {
            for (auto& link : links_)
            {
                if (link.from == from || link.to == to)
                {
                    return false;
                }
            }
            return true;
        }
        //allow one pin has multi links
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
                if (node.text_id == to)
                {
                    return true;
                }
            }
            return false;
        }
        return true;
    }

    void add_link(int from, int to, bool manully = false)
    {
        if (manully)
        {
            links_.emplace_back(from, to);
            return;
        }
        std::function<int(int)> get = [&](int i)
        {
            bool have_same = false;
            for (auto& l : links_)
            {
                if (l.from == i || l.to == i)
                {
                    i += 1;
                    have_same = true;
                    break;
                }
            }
            if (have_same) { i = get(i); }
            return i;
        };
        links_.emplace_back(get(from), get(to));

        //from = from / Node::MAX_PIN;
        //to = to / Node::MAX_PIN;

    }

    bool link_on_node(const Link& l, int node_id)
    {
        int i = l.from - node_id;
        int j = l.to - node_id;
        return (i >= 0 && i < Node::MAX_PIN) || (j >= 0 && j < Node::MAX_PIN);
    }


    void refresh_pos_link()
    {
        if (check_same_name()) { return; }

        std::map<int, std::pair<Node*, Node*>> n1;//pin->link(node*->node*)
        for (const auto& link : links_)
        {
            int from = link.from / Node::MAX_PIN;
            int to = link.to / Node::MAX_PIN;

            n1[link.from] = { &nodes_[from],&nodes_[to] };
            n1[link.to] = { &nodes_[from],&nodes_[to] };
        }

        for (auto& node : nodes_)
        {
            auto pos = ImNodes::GetNodeGridSpacePos(node.id);
            node.position_x = pos.x;
            node.position_y = pos.y;
            node.prevs.clear();
            node.nexts.clear();

            for (int i = node.id; i < node.id + node.next_pin; i++)
            {
                if (n1.count(i))
                {
                    node.nexts.push_back(n1[i].second);
                }
            }
            for (int i = node.text_id; i < node.text_id + node.prev_pin; i++)
            {
                if (n1.count(i))
                {
                    node.prevs.push_back(n1[i].first);
                }
            }
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
                ImGui::OpenPopup(u8"提示");
                if (ImGui::BeginPopupModal(u8"退出", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
                {
                    ImGui::Text(str.c_str());
                    if (ImGui::Button(u8"知道了"))
                    {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }
            }
        }
        return res;
    }

    void try_save(bool force = false)
    {
        if (saved_ == false || force)
        {
            refresh_pos_link();
            if (current_file_.empty())
            {
                auto file = openfile();
                if (!file.empty())
                {
                    current_file_ = file;
                    loader_->nodesToFile(nodes_, current_file_);
                    saved_ = true;
                }
            }
            else
            {
                loader_->nodesToFile(nodes_, current_file_);
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
        ImGui::OpenPopup(u8"退出");
        if (ImGui::BeginPopupModal(u8"退出", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text(u8"是否保存？");
            if (ImGui::Button(u8"是"))
            {
                ImGui::CloseCurrentPopup();
                try_save();
                exit(0);
                need_dialog_ = 0;
            }
            ImGui::SameLine();
            if (ImGui::Button(u8"否"))
            {
                ImGui::CloseCurrentPopup();
                exit(0);
                need_dialog_ = 0;
            }
            ImGui::SameLine();
            if (ImGui::Button(u8"取消"))
            {
                ImGui::CloseCurrentPopup();
                need_dialog_ = 0;
            }
            ImGui::EndPopup();
        }
    }

    std::string openfile()
    {
#ifdef _WIN32
        need_dialog_ = 0;
        OPENFILENAMEA ofn;
        char szFile[1024];
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = NULL;
        ofn.lpstrFile = szFile;
        ofn.lpstrFile[0] = '\0';
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "All files\0*.*\0";
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
#else
        std::string filePathName;
        ImGuiFileDialog::Instance()->OpenModal("ChooseFileDlgKey", "Choose File", "{.ini,.param}", ".");
        if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey"))
        {
            // action if OK
            if (ImGuiFileDialog::Instance()->IsOk())
            {
                filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
                std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
                // action
            }

            // close
            ImGuiFileDialog::Instance()->Close();
            need_dialog_ = 0;
        }
        return filePathName;
#endif
    }

public:
    SDL_Event event;

    ColorNodeEditor() : nodes_(), root_node_id_(-1),
        minimap_location_(ImNodesMiniMapLocation_BottomRight)
    {
        loader_ = create_loader("");
    }

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
                    need_dialog_ = 2;

                }
                if (ImGui::MenuItem(u8"保存"))
                {
                    try_save(true);
                }
                if (ImGui::MenuItem(u8"另存为..."))
                {
                    refresh_pos_link();
                    auto file = openfile();
                    if (!file.empty())
                    {
                        if (File::getFileExt(file) != "ini")
                        {
                            file = File::changeFileExt(file, "ini");
                        }
                        current_file_ = file;
                        //loader_->saveFile(current_file_);
                        saved_ = true;
                    }
                }
                if (ImGui::MenuItem(u8"退出"))
                {
                    //try_exit();
                    need_dialog_ = 1;
                    //ImGui::OpenPopup(u8"退出");
                    //
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

        {
            select_id_ = -1;
            if (ImNodes::NumSelectedNodes() == 1)
            {
                ImNodes::GetSelectedNodes(&select_id_);
            }
            erase_select_ = 0;
        }

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
                Node* node = nullptr;
                for (auto& n : nodes_)
                {
                    auto p = ImNodes::GetNodeScreenSpacePos(n.id);
                    auto size = ImNodes::GetNodeDimensions(n.id);
                    int x = click_pos.x - p.x;
                    int y = click_pos.y - p.y;
                    //fmt1::print("{},{};{},{};{},{};{},{}\n", click_pos.x, click_pos.y, p.x, p.y, x, y,size.x,size.y);
                    if (x >= 0 && y >= 0 && x < size.x && y < size.y)
                    {
                        node = &n;
                        break;
                    }
                }
                if (node)
                {
                    if (ImGui::MenuItem(u8"添加输入点"))
                    {
                        node->prev_pin++;
                    }
                    if (ImGui::MenuItem(u8"添加输出点"))
                    {
                        node->next_pin++;
                    }
                    if (ImGui::MenuItem(u8"清除无连接点"))
                    {
                        std::vector<Link*> from_this_node;
                        std::vector<Link*> to_this_node;
                        std::map<int, Node*> n1;//pin->link(node*)
                        for (auto& link : links_)
                        {
                            int from = link.from / Node::MAX_PIN * Node::MAX_PIN;
                            int to = link.to / Node::MAX_PIN * Node::MAX_PIN;
                            if (from == node->id)
                            {
                                from_this_node.push_back(&link);
                            }
                            if (to == node->id)
                            {
                                to_this_node.push_back(&link);
                            }
                        }

                        auto have_link = [&](int id, std::vector<Link*>& this_node)
                        {
                            for (auto& l : this_node)
                            {
                                if (l->from == id || l->to == id)
                                {
                                    return true;
                                }
                            }
                            return false;
                        };

                        auto remove = [&have_link](int& begin_id, int &current_count, std::vector<Link*>& this_node) 
                        {
                            while (current_count > this_node.size())
                            {
                                for (int i = begin_id; i <= begin_id + current_count; i++)
                                {
                                    if (!have_link(i, this_node))
                                    {
                                        for (auto& l : this_node)
                                        {
                                            if (l->from > i)
                                            {
                                                l->from--;
                                            }
                                        }
                                        current_count--;
                                        break;
                                    }
                                }
                            }
                        };
                        remove(node->id, node->next_pin, from_this_node);
                        remove(node->text_id, node->prev_pin, to_this_node);
                    }
                    if (ImGui::MenuItem(u8"删除"))
                    {
                        node->erased = 1;
                        for (auto it = links_.begin(); it != links_.end();)
                        {
                            if (link_on_node(*it, node->id))
                            {
                                it = links_.erase(it);
                            }
                            else
                            {
                                it++;
                            }
                        }
                    }
                }
                else
                {
                    if (ImGui::MenuItem(u8"新建操作层"))
                    {
                        auto& ui_node = createNode();
                        ui_node.title = fmt1::format("layer_{}", rand());
                        ui_node.type = "data";
                        ImNodes::SetNodeScreenSpacePos(ui_node.id, click_pos);
                        saved_ = false;
                    }
                    if (ImGui::MenuItem(u8"删除选中"))
                    {
                        erase_select_ = 1;
                    }
                }
                ImGui::EndPopup();
            }
            ImGui::PopStyleVar();
        }

        // draw nodes
        {
            for (auto& node : nodes_)
            {
                if (node.erased)
                {
                    continue;
                }

                auto type = convert::toLowerCase(node.type);
                if (type.find("data") != std::string::npos || type.find("input") != std::string::npos)
                {
                    ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(0xcc, 0x33, 0x33, 0xff));
                }
                else if (type.find("fc") != std::string::npos || type.find("inner") != std::string::npos)
                {
                    ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(0xff, 0xcc, 0x99, 0xff));
                }
                else if (type.find("conv") != std::string::npos)
                {
                    ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(0xcc, 0xff, 0xff, 0xff));
                }
                else if (type.find("pool") != std::string::npos || type.find("up") != std::string::npos)
                {
                    ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(0x99, 0xcc, 0x66, 0xff));
                }
                else if (type.find("split") != std::string::npos)
                {
                    ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(0xff, 0xcc, 0x99, 0xff));
                }
                else if (type.find("cat") != std::string::npos)
                {
                    ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(0xff, 0xff, 0x99, 0xff));
                }
                else
                {
                    ImNodes::PushColorStyle(ImNodesCol_TitleBar, IM_COL32(0xff, 0x66, 0x66, 0xff));
                }
                const float node_width = 200;
                ImNodes::BeginNode(node.id);

                ImNodes::BeginNodeTitleBar();
                ImGui::PushItemWidth(node_width);
                ImGui::InputText("##hidelabel", &node.title);
                ImNodes::EndNodeTitleBar();

                int table_width = node_width - 24;

                {
                    int pin = node.prev_pin;
                    ImGui::BeginTable("inb", pin + 9, 0, ImVec2(node_width - 10, 1), 0);
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    for (int i = 0; i < pin; i++)
                    {
                        ImGui::TableNextColumn();
                        ImNodes::BeginInputAttribute(node.text_id + i);
                        ImNodes::EndInputAttribute();
                    }
                    ImGui::EndTable();
                }

                //if (graph_.num_edges_from_node(node.text_id) == 0ull)
                ImGui::TextUnformatted("type");
                ImGui::SameLine();
                ImGui::PushItemWidth(node_width - ImGui::CalcTextSize("type").x - 8);
                ImGui::InputText("##type", &node.type);
                ImGui::PopItemWidth();

                if (node.id == select_id_)
                {
                    loader_->refreshNodeValues(node);
                    ImGui::BeginTable("value", 2, 0, { node_width, 0 });
                    ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthFixed, 80);
                    for (auto& kv : node.values)
                    {
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(kv.first.c_str());
                        ImGui::TableNextColumn();
                        ImGui::PushItemWidth(100);
                        ImGui::InputText(("##" + kv.first).c_str(), &kv.second);
                        ImGui::PopItemWidth();
                        ImGui::TableNextRow();
                    }
                    ImGui::EndTable();
                    ImGui::PushItemWidth(node_width);
                    ImGui::InputTextMultiline("##text", &node.text, ImVec2(0, 20));
                }

                {
                    int pin = node.next_pin;
                    ImGui::BeginTable("out", pin + 9, 0, ImVec2(node_width - 10, 1), 0);
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    ImGui::TableNextColumn();
                    for (int i = 0; i < pin; i++)
                    {
                        ImGui::TableNextColumn();
                        ImNodes::BeginOutputAttribute(node.id + i);
                        ImNodes::EndOutputAttribute();
                    }
                    ImGui::EndTable();
                }

                ImNodes::EndNode();
                ImNodes::PopColorStyle();
                //ImNodes::PopColorStyle();
                //ImNodes::PopColorStyle();
            }
        }

        {
            int link_id = 0;
            for (const auto& link : links_)
            {
                ImNodes::Link(link_id++, link.from, link.to);
            }
        }
        ImNodes::MiniMap(0.5f, minimap_location_);
        ImNodes::EndNodeEditor();

        // Handle new links
        // These are driven by Imnodes, so we place the code after EndNodeEditor().

        {
            int start_attr, end_attr;
            if (ImNodes::IsLinkCreated(&start_attr, &end_attr))
            {
                if (start_attr % Node::MAX_PIN >= Node::HALF_MAX_PIN)
                {
                    // Ensure the edge is always directed from the text to
                    // whatever produces the text      
                    std::swap(start_attr, end_attr);
                }
                if (check_can_link(start_attr, end_attr, true))
                {
                    add_link(start_attr, end_attr, true);
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
            if (num_selected > 0 && (erase_select_ || (ImGui::IsKeyReleased(SDL_SCANCODE_DELETE) && !ImGui::IsAnyItemActive())))
            {
                static std::vector<int> selected_links;
                selected_links.resize(static_cast<size_t>(num_selected));
                ImNodes::GetSelectedLinks(selected_links.data());
                for (const int link_id : selected_links)
                {
                    links_.erase(links_.begin() + link_id);
                }
                ImNodes::ClearLinkSelection();
                saved_ = false;
            }
        }

        {
            const int num_selected = ImNodes::NumSelectedNodes();
            if (num_selected > 0 && (erase_select_ || (ImGui::IsKeyReleased(SDL_SCANCODE_DELETE) && !ImGui::IsAnyItemActive())))
            {
                static std::vector<int> selected_nodes;
                selected_nodes.resize(static_cast<size_t>(num_selected));
                ImNodes::GetSelectedNodes(selected_nodes.data());
                for (const int node_id : selected_nodes)
                {
                    auto iter = std::find_if(nodes_.begin(), nodes_.end(), [node_id](const Node& node) -> bool
                    {
                        return node.id == node_id;
                    });
                    iter->erased = 1;
                    for (auto it = links_.begin(); it != links_.end();)
                    {
                        if (link_on_node(*it, node_id))
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

        if (event.type == SDL_QUIT
            || event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)
        {
            need_dialog_ = 1;
        }

        if (need_dialog_ == 1)
        {
            //ImGui::OpenPopup(u8"退出");
            try_exit();
        }
        if (need_dialog_ == 2 || !begin_file_.empty() && first_run_)
        {
            std::string file;
            if (!begin_file_.empty() && first_run_)
            {
                file = begin_file_;
            }
            else
            {
                file = openfile();
            }
            //std::string file = "squeezenet_v1.1.param";
            if (!file.empty())
            {
                nodes_.clear();
                loader_ = create_loader(file);    //此处有内存泄漏,不管了
                loader_->fileToNodes(file, nodes_);
                current_file_ = file;
                ImVec2 pos;
                pos.x = 100, pos.y = 100;
                // restore position
                int count = 0;
                for (auto& node : nodes_)
                {
                    node.id = count * Node::MAX_PIN;
                    node.text_id = count * Node::MAX_PIN + Node::HALF_MAX_PIN;
                    node.prev_pin = node.prevs.size();
                    node.next_pin = node.nexts.size();
                    count++;
                    if (node.position_x != -1)
                    {
                        pos = ImVec2(node.position_x, node.position_y);
                    }
                    ImNodes::SetNodeGridSpacePos(node.id, pos);
                    pos.x += 150;
                    pos.y += 0;
                }
                // restore link
                links_.clear();
                for (auto& node : nodes_)
                {
                    for (auto node1 : node.nexts)
                    {
                        if (check_can_link(node.id, node1->text_id))
                        {
                            add_link(node.id, node1->text_id);
                        }
                    }
                }
            }
            saved_ = true;
        }

        //refresh_pos_link();

        ImGui::End();
        first_run_ = 0;
    }
    void setBeginFile(const std::string& file)
    {
        begin_file_ = file;
    }
    };

static ColorNodeEditor color_editor;
} // namespace

void NodeEditorInitialize(int argc, char* argv[])
{
    ImGui::StyleColorsLight();
    ImNodes::StyleColorsLight();
    ImNodesIO& ion = ImNodes::GetIO();
    ion.LinkDetachWithModifierClick.Modifier = &ImGui::GetIO().KeyCtrl;
    ion.EmulateThreeButtonMouse.Modifier = &ImGui::GetIO().KeyAlt;
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
    if (argc >= 2)
    {
        ex1::color_editor.setBeginFile(argv[1]);
    }
}

void NodeEditorShow() { ex1::color_editor.show(); }

void NodeEditorSetEvent(void* e) { ex1::color_editor.event = *(SDL_Event*)e; }

void NodeEditorShutdown() {}
} // namespace example
