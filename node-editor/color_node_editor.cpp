#include "node_editor.h"

#include <imnodes.h>
#include <imgui.h>
#include <ImGuiFileDialog.h>

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
        std::vector<std::pair<std::string, std::string>> values;
        int text_id;
    };
    struct Link
    {
        int from, to;
        Link(int f, int t) { from = f; to = t; }
    };

    NodeLoader* loader_ = nullptr;
    std::vector<UiNode> nodes_;
    std::vector<Link> links_;
    int root_node_id_;
    ImNodesMiniMapLocation minimap_location_;
    std::string current_file_;
    bool saved_ = true;
    int need_dialog_ = 0;    //1: when exist, 2: openfile to open
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
        need_dialog_ = 0;
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
#else
        std::string filePathName;
        ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", ".ini", ".");

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
    void refresh_loader()
    {
        if (check_same_name()) { return; }
        loader_->eraseAllLayers();
        std::map<int, std::string> m1;
        for (int i = 0; i < nodes_.size(); i++)
        {
            auto pos = ImNodes::GetNodeGridSpacePos(nodes_[i].id);
            //需重设置属性
            //auto str = fmt1::format("[{}]\n{}\neditor_position={}, {}", nodes_[i].title, nodes_[i].text, pos.x, pos.y);
            for (auto& kv : nodes_[i].values)
            {
                loader_->setLayerPro(nodes_[i].title, kv.first, kv.second);
            }
            loader_->setLayerPro(nodes_[i].title, "editor_position", fmt1::format("{},{}", pos.x, pos.y));
            m1[nodes_[i].id] = nodes_[i].title;
            m1[nodes_[i].text_id] = nodes_[i].title;
        }
        std::map<std::string, std::vector<std::string>> m2;
        for (const auto& link : links_)
        {
            m2[m1[link.from]].push_back(m1[link.to]);
        }
        for (auto& kv : m2)
        {
            //看起来没有处理多个连接
            loader_->setLayerNext(kv.first, kv.second);
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
            refresh_loader();
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
                    loader_->saveFile(current_file_);
                    saved_ = true;
                }
            }
            else
            {
                loader_->saveFile(current_file_);
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
                    need_dialog_ = 2;

                }
                if (ImGui::MenuItem(u8"保存"))
                {
                    try_save(true);
                }
                if (ImGui::MenuItem(u8"另存为..."))
                {
                    refresh_loader();
                    auto file = openfile();
                    if (!file.empty())
                    {
                        if (File::getFileExt(file) != "ini")
                        {
                            file = File::changeFileExt(file, "ini");
                        }
                        current_file_ = file;
                        loader_->saveFile(current_file_);
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
            const float node_width = 100;
            ImNodes::BeginNode(node.id);

            ImNodes::BeginNodeTitleBar();
            ImGui::PushItemWidth(node_width);
            ImGui::InputText("##hidelabel", &node.title);
            ImNodes::EndNodeTitleBar();

            ImNodes::BeginInputAttribute(node.text_id);
            //if (graph_.num_edges_from_node(node.text_id) == 0ull)

            for (auto& kv : node.values)
            {
                ImGui::TextUnformatted(kv.first.c_str());
                ImGui::SameLine();
                ImGui::PushItemWidth(50);
                ImGui::InputText(("##"+kv.first).c_str(), &kv.second);
                ImGui::PopItemWidth();
            }
            {
                /*ImGui::PushItemWidth(node_width);
                ImGui::InputTextMultiline("##hidelabel", &node.text, ImVec2(node_width, 50));
                ImGui::PopItemWidth();*/
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
        if (need_dialog_ == 2)
        {
            auto file = openfile();
            if (!file.empty())
            {
                nodes_.clear();
                if (loader_)
                {
                    loader_->clear();
                }
                loader_ = create_loader(file);    //此处有内存泄漏,不管了
                loader_->loadFile(file);
                current_file_ = file;
                ImVec2 pos;
                pos.x = 100, pos.y = 200;

                std::map<std::string, UiNode> dd;
                //ImNodes::BeginNodeEditor();
                // restore mod
                auto layers = loader_->getAllLayers();
                for (auto& layer : layers)
                {
                    auto& ui_node = createUiNode();
                    ui_node.type = UiNodeType::fc;
                    ui_node.title = layer;
                    for (auto& key : loader_->getLayerAllPro(layer))
                    {
                        if (key != "next" && key != "editor_position")
                        {
                            ui_node.values.push_back({ key, loader_->getLayerPro(layer, key) });
                        }
                    }
                    if (!ui_node.text.empty())
                    {
                        ui_node.text.pop_back();
                    }
                    std::vector<int> v = convert::findNumbers<int>(loader_->getLayerPro(layer, "editor_position"));
                    if (v.size() >= 2)
                    {
                        pos.x = v[0];
                        pos.y = v[1];
                    }
                    ImNodes::SetNodeGridSpacePos(ui_node.id, pos);
                    pos.x += 200;
                    pos.y += 20;
                    dd[layer] = ui_node;
                }
                // restore link
                links_.clear();
                for (auto& layer : loader_->getAllLayers())
                {
                    auto nexts = loader_->getLayerNext(layer);
                    for (auto sec1 : nexts)
                    {
                        if (check_can_link(dd[layer].id, dd[sec1].text_id))
                        {
                            links_.emplace_back(dd[layer].id, dd[sec1].text_id);
                        }
                    }
                }
            }
            saved_ = true;
        }


        {
            for (auto& ui_node : nodes_)
            {
                std::string type;
                for (auto& v : ui_node.values)
                {
                    if (v.first == "type")
                    {
                        type = v.second;
                        break;
                    }
                }
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
                    ui_node.type = UiNodeType::fc;
                }
                else if (type.find("conv") == 0)
                {
                    ui_node.type = UiNodeType::conv;
                }
                else if (type.find("pool") == 0)
                {
                    ui_node.type = UiNodeType::pool;
                }
            }
        }

        ImGui::End();
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
