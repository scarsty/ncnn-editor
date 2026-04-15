#include "node_editor.h"

#include <imnodes.h>
#include <imgui.h>
#include <ImGuiFileDialog.h>

#include "node.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <format>
#include <vector>
#include <string>
#include <imgui_stdlib.h>

#include "filefunc.h"
#include "strfunc.h"

#include "FileLoader.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

#ifdef __APPLE__
#include "MacOSCode.h"
#endif

namespace example
{
namespace ex1
{

#ifdef __EMSCRIPTEN__
EM_JS(void, WebOpenFileDialog, (), {
    if (!Module.__ncnnEditorFileInput) {
        const input = document.createElement('input');
        input.type = 'file';
        input.accept = '.ini,.param,.yaml';
        // Some browsers reject synthetic click on display:none file inputs.
        input.style.position = 'fixed';
        input.style.left = '-10000px';
        input.style.top = '-10000px';
        input.style.width = '1px';
        input.style.height = '1px';
        input.style.opacity = '0';
        input.addEventListener('change', async (event) => {
            const file = event.target.files && event.target.files[0];
            if (!file) {
                return;
            }
            const safeName = file.name.replace(/[^A-Za-z0-9_.-]/g, '_');
            const mountPath = '/uploads';
            try {
                FS.mkdirTree(mountPath);
            } catch (error) {
            }
            const targetPath = mountPath + '/' + safeName;
            const bytes = new Uint8Array(await file.arrayBuffer());
            FS.writeFile(targetPath, bytes);
            Module.ccall('NodeEditorSetPendingFile', null, ['string'], [targetPath]);
            event.target.value = "";
        });
        document.body.appendChild(input);
        Module.__ncnnEditorFileInput = input;
    }

    if (!Module.__ncnnEditorTryOpenPicker) {
        Module.__ncnnEditorTryOpenPicker = () => {
            const input = Module.__ncnnEditorFileInput;
            if (!input) {
                return;
            }
            if (typeof input.showPicker === 'function') {
                input.showPicker();
            } else {
                input.click();
            }
        };
    }

    if (!Module.__ncnnEditorOpenHookInstalled) {
        Module.__ncnnEditorOpenHookInstalled = true;
        Module.__ncnnEditorPendingOpen = false;
        // Fallback: if immediate open loses user activation, next click re-triggers picker.
        document.addEventListener('pointerdown', () => {
            if (!Module.__ncnnEditorPendingOpen) {
                return;
            }
            Module.__ncnnEditorPendingOpen = false;
            Module.__ncnnEditorTryOpenPicker();
        }, true);
    }

    Module.__ncnnEditorPendingOpen = true;
    try {
        Module.__ncnnEditorTryOpenPicker();
        Module.__ncnnEditorPendingOpen = false;
    } catch (error) {
    }
});

EM_JS(void, WebDownloadFile, (const char* path_ptr), {
    const path = UTF8ToString(path_ptr);
    const data = FS.readFile(path);
    const blob = new Blob([data], { type: 'application/octet-stream' });
    const link = document.createElement('a');
    link.href = URL.createObjectURL(blob);
    link.download = path.split('/').pop() || 'nn-editor-output.ini';
    document.body.appendChild(link);
    link.click();
    link.remove();
    setTimeout(() => URL.revokeObjectURL(link.href), 1000);
});
#endif

static float current_time_seconds = 0.f;
static bool emulate_three_button_mouse = false;

class ColorNodeEditor
{
private:
    struct Link
    {
        int from, to;
        Link(int f, int t) { from = f; to = t; }
        bool operator==(const Link& other) const { return from == other.from && to == other.to; }
    };

    struct NodeSnapshot
    {
        std::string title;
        std::string type;
        std::string text;
        OrderMap<std::string, std::string> values;
        int id = 0;
        int text_id = 0;
        int prev_pin = 0;
        int next_pin = 0;
        int position_x = -1;
        int position_y = -1;
        int erased = 0;
        std::vector<std::string> in;
        std::vector<std::string> out;
        int turn = 0;
    };

    struct EditorSnapshot
    {
        std::vector<NodeSnapshot> nodes;
        std::vector<Link> links;
        bool saved = true;
    };

    FileLoader* loader_ = nullptr;
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
    int pending_focus_node_id_ = -1;
    int pending_focus_frames_ = 0;
    bool pending_focus_top_anchor_ = false;
    bool dirty_this_frame_ = false;
    std::vector<EditorSnapshot> history_;
    size_t history_cursor_ = 0;

    int erase_select_ = 0;

    static bool snapshot_equals(const EditorSnapshot& lhs, const EditorSnapshot& rhs)
    {
        if (lhs.saved != rhs.saved || lhs.nodes.size() != rhs.nodes.size() || lhs.links.size() != rhs.links.size())
        {
            return false;
        }

        for (size_t i = 0; i < lhs.nodes.size(); ++i)
        {
            const NodeSnapshot& a = lhs.nodes[i];
            const NodeSnapshot& b = rhs.nodes[i];
            if (a.title != b.title || a.type != b.type || a.text != b.text || a.values != b.values ||
                a.id != b.id || a.text_id != b.text_id || a.prev_pin != b.prev_pin || a.next_pin != b.next_pin ||
                a.position_x != b.position_x || a.position_y != b.position_y || a.erased != b.erased ||
                a.in != b.in || a.out != b.out || a.turn != b.turn)
            {
                return false;
            }
        }

        for (size_t i = 0; i < lhs.links.size(); ++i)
        {
            if (!(lhs.links[i] == rhs.links[i]))
            {
                return false;
            }
        }

        return true;
    }

    void mark_dirty()
    {
        dirty_this_frame_ = true;
        saved_ = false;
    }

    void rebuild_connectivity_from_links()
    {
        std::map<int, Node*> by_id;
        for (auto& node : nodes_)
        {
            node.prevs.clear();
            node.nexts.clear();
            by_id[node.id] = &node;
        }

        for (const auto& link : links_)
        {
            const int from_node_id = (link.from / Node::MAX_PIN) * Node::MAX_PIN;
            const int to_node_id = (link.to / Node::MAX_PIN) * Node::MAX_PIN;
            auto from_it = by_id.find(from_node_id);
            auto to_it = by_id.find(to_node_id);
            if (from_it == by_id.end() || to_it == by_id.end())
            {
                continue;
            }
            from_it->second->nexts.push_back(to_it->second);
            to_it->second->prevs.push_back(from_it->second);
        }
    }

    void sync_node_positions_from_editor()
    {
        for (auto& node : nodes_)
        {
            const ImVec2 pos = ImNodes::GetNodeGridSpacePos(node.id);
            const int x = static_cast<int>(std::lround(pos.x));
            const int y = static_cast<int>(std::lround(pos.y));
            if (x != node.position_x || y != node.position_y)
            {
                node.position_x = x;
                node.position_y = y;
                mark_dirty();
            }
        }
    }

    EditorSnapshot capture_snapshot() const
    {
        EditorSnapshot snapshot;
        snapshot.saved = saved_;
        snapshot.links = links_;
        snapshot.nodes.reserve(nodes_.size());

        for (const auto& node : nodes_)
        {
            NodeSnapshot ns;
            ns.title = node.title;
            ns.type = node.type;
            ns.text = node.text;
            ns.values = node.values;
            ns.id = node.id;
            ns.text_id = node.text_id;
            ns.prev_pin = node.prev_pin;
            ns.next_pin = node.next_pin;
            ns.position_x = node.position_x;
            ns.position_y = node.position_y;
            ns.erased = node.erased;
            ns.in = node.in;
            ns.out = node.out;
            ns.turn = node.turn;
            snapshot.nodes.push_back(std::move(ns));
        }

        return snapshot;
    }

    void apply_snapshot(const EditorSnapshot& snapshot)
    {
        nodes_.clear();
        links_ = snapshot.links;
        for (const auto& ns : snapshot.nodes)
        {
            nodes_.emplace_back();
            Node& node = nodes_.back();
            node.title = ns.title;
            node.type = ns.type;
            node.text = ns.text;
            node.values = ns.values;
            node.id = ns.id;
            node.text_id = ns.text_id;
            node.prev_pin = ns.prev_pin;
            node.next_pin = ns.next_pin;
            node.position_x = ns.position_x;
            node.position_y = ns.position_y;
            node.erased = ns.erased;
            node.in = ns.in;
            node.out = ns.out;
            node.turn = ns.turn;
        }

        rebuild_connectivity_from_links();
        for (auto& node : nodes_)
        {
            ImVec2 pos(100.0f, 100.0f);
            if (node.position_x != -1)
            {
                pos = ImVec2(static_cast<float>(node.position_x), static_cast<float>(node.position_y));
            }
            ImNodes::SetNodeGridSpacePos(node.id, pos);
        }
        saved_ = snapshot.saved;
        dirty_this_frame_ = false;
    }

    void reset_history_with_current()
    {
        history_.clear();
        history_.push_back(capture_snapshot());
        history_cursor_ = 0;
        dirty_this_frame_ = false;
    }

    void commit_history_if_dirty()
    {
        if (!dirty_this_frame_)
        {
            return;
        }

        EditorSnapshot current = capture_snapshot();
        if (history_.empty())
        {
            history_.push_back(std::move(current));
            history_cursor_ = 0;
            dirty_this_frame_ = false;
            return;
        }

        if (history_cursor_ + 1 < history_.size())
        {
            history_.erase(history_.begin() + static_cast<std::ptrdiff_t>(history_cursor_ + 1), history_.end());
        }

        if (!snapshot_equals(history_.back(), current))
        {
            history_.push_back(std::move(current));
            history_cursor_ = history_.size() - 1;
        }
        dirty_this_frame_ = false;
    }

    bool can_undo() const { return !history_.empty() && history_cursor_ > 0; }
    bool can_redo() const { return !history_.empty() && history_cursor_ + 1 < history_.size(); }

    void undo()
    {
        if (!can_undo())
        {
            return;
        }
        history_cursor_--;
        apply_snapshot(history_[history_cursor_]);
    }

    void redo()
    {
        if (!can_redo())
        {
            return;
        }
        history_cursor_++;
        apply_snapshot(history_[history_cursor_]);
    }

    void draw_pin_row(bool is_input, int start_id, int pin_count, float node_width)
    {
        if (pin_count <= 0)
        {
            return;
        }

        const float pin_pitch = 18.0f;
        const float total_width = static_cast<float>(pin_count - 1) * pin_pitch;
        const float lead_space = (std::max)(0.0f, (node_width - total_width) * 0.5f - 6.0f);

        ImGui::Dummy(ImVec2(lead_space, 1.0f));
        ImGui::SameLine(0.0f, 0.0f);

        for (int i = 0; i < pin_count; ++i)
        {
            if (is_input)
            {
                ImNodes::BeginInputAttribute(start_id + i);
            }
            else
            {
                ImNodes::BeginOutputAttribute(start_id + i);
            }

            ImGui::Dummy(ImVec2(1.0f, 8.0f));

            if (is_input)
            {
                ImNodes::EndInputAttribute();
            }
            else
            {
                ImNodes::EndOutputAttribute();
            }

            if (i + 1 < pin_count)
            {
                ImGui::SameLine(0.0f, pin_pitch);
            }
        }
    }

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
            for (auto& l : links_)
            {
                if (l.from == from || l.to == to)
                {
                    return;
                }
            }
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
                auto str = std::format("There are {} \"{}\"!", kv.second, kv.first);
                ImGui::OpenPopup("Note");
                if (ImGui::BeginPopupModal("Exit", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
                {
                    ImGui::TextUnformatted(str.c_str());
                    if (ImGui::Button("OK"))
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
                auto file = openfile(file_filter(), &loader_);
                if (!file.empty())
                {
                    current_file_ = file;
                    loader_->nodesToFile(nodes_, current_file_);
#ifdef __EMSCRIPTEN__
                    WebDownloadFile(current_file_.c_str());
#endif
                    saved_ = true;
                }
            }
            else
            {
                loader_->nodesToFile(nodes_, current_file_);
#ifdef __EMSCRIPTEN__
                WebDownloadFile(current_file_.c_str());
#endif
                saved_ = true;
            }
        }
    }

    void try_exit()
    {
#ifdef __EMSCRIPTEN__
        need_dialog_ = 0;
        return;
#else
        if (saved_)
        {
            exit(0);
        }
        ImGui::OpenPopup("Exit");
        if (ImGui::BeginPopupModal("Exit", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextUnformatted("Do you want to save the change?");
            if (ImGui::Button("Yes"))
            {
                ImGui::CloseCurrentPopup();
                try_save();
                exit(0);
                need_dialog_ = 0;
            }
            ImGui::SameLine();
            if (ImGui::Button("No"))
            {
                ImGui::CloseCurrentPopup();
                exit(0);
                need_dialog_ = 0;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                ImGui::CloseCurrentPopup();
                need_dialog_ = 0;
            }
            ImGui::EndPopup();
        }
#endif
    }

    std::string openfile(const char* filter, FileLoader** loader_ptr = nullptr)
    {
    #ifdef __EMSCRIPTEN__
        (void)filter;
        (void)loader_ptr;
        need_dialog_ = 0;
        WebOpenFileDialog();
        return "";
    #else
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
        ofn.lpstrFilter = filter;
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL;
        ofn.Flags = OFN_PATHMUSTEXIST;
        if (loader_ptr)
        {
            *loader_ptr = create_loader("", ofn.nFilterIndex);
        }
        if (GetOpenFileNameA(&ofn))
        {
            return szFile;
        }
        else
        {
            return "";
        }
#else
#ifdef __APPLE__
        need_dialog_ = 0;
        std::string filePathName;
        char* n = MacOSCode::openFile();
        if(n)
        {
            filePathName =n;
        }
        delete n;
        return filePathName;
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
#endif
    #endif
    }

public:
    int is_exiting_ = 0;

    ColorNodeEditor() :
        minimap_location_(ImNodesMiniMapLocation_BottomRight)
    {
        loader_ = create_loader("");
        reset_history_with_current();
    }

    void show()
    {
        // Update timer context
        current_time_seconds ++;

        auto flags = ImGuiWindowFlags_MenuBar
                    | ImGuiWindowFlags_NoMove
                    | ImGuiWindowFlags_NoCollapse
                    | ImGuiWindowFlags_NoResize
                    | ImGuiWindowFlags_NoTitleBar;

        // The node editor window fills the whole display
        std::string window_title = "Neural Net Editor";
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

        ImGui::Begin(window_title.c_str(), NULL, flags);
        
        window_title.clear();

        //if (ImGui::)   //close window
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Open..."))
                {
                    need_dialog_ = 2;

                }
                if (ImGui::MenuItem("Save"))
                {
                    try_save(true);
                }
                if (ImGui::MenuItem("Save as..."))
                {
#ifdef __EMSCRIPTEN__
                    if (current_file_.empty())
                    {
                        current_file_ = "/downloads/nn-editor.ini";
                    }
                    try_save(true);
#else
                    refresh_pos_link();
                    auto file = openfile(file_filter(), &loader_);
                    if (!file.empty())
                    {
                        if (filefunc::getFileExt(file) != "ini")
                        {
                            file = filefunc::changeFileExt(file, "ini");
                        }
                        current_file_ = file;
                        loader_->nodesToFile(nodes_, current_file_);
                        saved_ = true;
                    }
#endif
                }
                if (ImGui::MenuItem("Exit"))
                {
                    //try_exit();
                    need_dialog_ = 1;
                    //ImGui::OpenPopup("退出");
                    //
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, can_undo()))
                {
                    undo();
                }
                if (ImGui::MenuItem("Redo", "Ctrl+Shift+Z", false, can_redo()))
                {
                    redo();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("MiniMap"))
            {
                const char* names[] = {
                    "`+ ",
                    " +`",
                    ".+ ",
                    " +.",
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

            if (ImGui::BeginMenu("Style"))
            {
                if (ImGui::MenuItem("Classic"))
                {
                    ImGui::StyleColorsClassic();
                    ImNodes::StyleColorsClassic();
                }
                if (ImGui::MenuItem("Dark"))
                {
                    ImGui::StyleColorsDark();
                    ImNodes::StyleColorsDark();
                }
                if (ImGui::MenuItem("Light"))
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
            ImGui::TextUnformatted("Delete -- Erase selected ops and links");
            ImGui::SameLine();
            if (ImGui::Button("Focus Graph") && !nodes_.empty())
            {
                pending_focus_node_id_ = nodes_.front().id;
                pending_focus_frames_ = 2;
                pending_focus_top_anchor_ = false;
            }
            //ImGui::NextColumn();
            std::string str = "No opened file";
            if (!current_file_.empty())
            {
                str = std::format("Current file: {}, ", current_file_);
                if (saved_)
                {
                    str += "Saved";
                }
                else
                {
                    str += "Unsaved";
                }
            }
            ImGui::TextUnformatted(str.c_str());

            if (!nodes_.empty())
            {
                const ImVec2 pan = ImNodes::EditorContextGetPanning();
                const ImVec2 first_pos = ImNodes::GetNodeGridSpacePos(nodes_.front().id);
                const std::string graph_debug = std::format(
                    "Graph: nodes={} links={} pan=({}, {}) first=({}, {})",
                    nodes_.size(),
                    links_.size(),
                    static_cast<int>(std::lround(pan.x)),
                    static_cast<int>(std::lround(pan.y)),
                    static_cast<int>(std::lround(first_pos.x)),
                    static_cast<int>(std::lround(first_pos.y)));
                ImGui::TextUnformatted(graph_debug.c_str());
            }
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

        const ImVec2 editor_canvas_size = ImGui::GetContentRegionAvail();
        ImNodes::BeginNodeEditor();

        // Wheel/trackpad panning for the node canvas.
        // MouseWheel: vertical; MouseWheelH: horizontal (touchpad two-finger).
        // Shift + vertical wheel is treated as horizontal for wider compatibility.
        {
            ImGuiIO& io = ImGui::GetIO();
            if (ImNodes::IsEditorHovered() && !ImGui::IsAnyItemActive())
            {
                float wheel_x = io.MouseWheelH;
                float wheel_y = io.MouseWheel;

                if (wheel_x == 0.0f && io.KeyShift && wheel_y != 0.0f)
                {
                    wheel_x = wheel_y;
                    wheel_y = 0.0f;
                }

                if (wheel_x != 0.0f || wheel_y != 0.0f)
                {
                    constexpr float kWheelPanStep = 56.0f;
                    ImVec2 pan = ImNodes::EditorContextGetPanning();
                    pan.x += wheel_x * kWheelPanStep;
                    pan.y += wheel_y * kWheelPanStep;
                    ImNodes::EditorContextResetPanning(pan);
                }
            }
        }

        // Handle new nodes
        // These are driven by the user, so we place this code before rendering the nodes
        {
            static ImVec2 right_click_pos0, right_click_pos1;
            bool right_clicked = false;

            if (ImGui::IsMouseClicked(1))
            {
                right_click_pos0 = ImGui::GetMousePos();
            }
            if (ImGui::IsMouseReleased(1))
            {
                right_click_pos1 = ImGui::GetMousePos();
                if (right_click_pos1.x == right_click_pos0.x && right_click_pos1.y == right_click_pos0.y)
                {
                    right_clicked = true;
                }
            }

            const bool open_popup = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
                ImNodes::IsEditorHovered() &&
                (right_clicked);

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
                    if (x >= 0 && y >= 0 && x < size.x && y < size.y)
                    {
                        node = &n;
                        break;
                    }
                }
                if (node)
                {
                    if (ImGui::MenuItem("Add input pin"))
                    {
                        node->prev_pin++;
                        mark_dirty();
                    }
                    if (ImGui::MenuItem("Add output pin"))
                    {
                        node->next_pin++;
                        mark_dirty();
                    }
                    if (ImGui::MenuItem("Clear unlinked pins"))
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

                        auto remove = [&have_link](int& begin_id, int& current_count, std::vector<Link*>& this_node)
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
                        mark_dirty();
                    }
                    if (ImGui::MenuItem("Erase"))
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
                        mark_dirty();
                    }
                }
                else
                {
                    if (ImGui::MenuItem("New Op"))
                    {
                        auto& ui_node = createNode();
                        ui_node.title = std::format("layer_{}", rand());
                        ui_node.type = "data";
                        ImNodes::SetNodeScreenSpacePos(ui_node.id, click_pos);
                        mark_dirty();
                    }
                    if (ImGui::MenuItem("Erase all selected"))
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

                auto type = strfunc::toLowerCase(node.type);
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
                    draw_pin_row(true, node.text_id, node.prev_pin, node_width);
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
                    if (ImGui::BeginTable("value", 2, 0, { node_width, 0 }))
                    {
                        ImGui::TableSetupColumn("value1", ImGuiTableColumnFlags_WidthFixed, 120);
                        ImGui::TableSetupColumn("value2", ImGuiTableColumnFlags_WidthFixed, 60);
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
                    }
                    ImGui::PushItemWidth(node_width);
                    ImGui::InputTextMultiline("##text", &node.text, ImVec2(0, 20));
                }

                {
                    draw_pin_row(false, node.id, node.next_pin, node_width);
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

        if (pending_focus_frames_ > 0 && pending_focus_node_id_ >= 0)
        {
            if (pending_focus_top_anchor_)
            {
                const ImVec2 node_pos = ImNodes::GetNodeGridSpacePos(pending_focus_node_id_);
                const ImVec2 node_size = ImNodes::GetNodeDimensions(pending_focus_node_id_);
                const float target_x = (editor_canvas_size.x - node_size.x) * 0.5f;
                const float target_y = 20.0f;
                ImNodes::EditorContextResetPanning(ImVec2(target_x - node_pos.x, target_y - node_pos.y));
            }
            else
            {
                ImNodes::EditorContextMoveToNode(pending_focus_node_id_);
            }
            pending_focus_frames_--;
            if (pending_focus_frames_ <= 0)
            {
                pending_focus_top_anchor_ = false;
            }
        }
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
                    mark_dirty();
                }
            }
        }

        // Handle deleted links

        {
            int link_id;
            if (ImNodes::IsLinkDestroyed(&link_id))
            {
                links_.erase(links_.begin() + link_id);
                mark_dirty();
            }
        }

        {
            const int num_selected = ImNodes::NumSelectedLinks();
            if (num_selected > 0 && (erase_select_  || (ImGui::IsKeyReleased(ImGuiKey_Delete) && !ImGui::IsAnyItemActive())))
            {
                static std::vector<int> selected_links;
                selected_links.resize(static_cast<size_t>(num_selected));
                ImNodes::GetSelectedLinks(selected_links.data());
                std::sort(selected_links.begin(), selected_links.end(), std::greater<int>());
                for (const int link_id : selected_links)
                {
                    links_.erase(links_.begin() + link_id);
                }
                ImNodes::ClearLinkSelection();
                mark_dirty();
            }
        }

        {
            const int num_selected = ImNodes::NumSelectedNodes();
            if (num_selected > 0 && (erase_select_  || (ImGui::IsKeyReleased(ImGuiKey_Delete) && !ImGui::IsAnyItemActive())))
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
                mark_dirty();
            }
        }

        if (ImGui::IsItemEdited())
        {
            mark_dirty();
        }

        sync_node_positions_from_editor();

        if (ImGui::IsKeyReleased(ImGuiKey_Z) && ImGui::GetIO().KeyCtrl)
        {
            if (ImGui::GetIO().KeyShift)
            {
                redo();
            }
            else
            {
                undo();
            }
        }

        if (ImGui::IsKeyReleased(ImGuiKey_S) && ImGui::GetIO().KeyCtrl)
        {
            try_save(true);
        }

        if (is_exiting_)
        {
            need_dialog_ = 1;
            is_exiting_ = 0;
        }

        if (need_dialog_ == 1)
        {
            //ImGui::OpenPopup("退出");
            try_exit();
        }
        if (need_dialog_ == 2 || (!begin_file_.empty() && first_run_))
        {
            std::string file;
            const bool queued_file = !begin_file_.empty();
            if (queued_file)
            {
                file = begin_file_;
            }
            else
            {
                file = openfile("All files\0*.*\0");
            }
            //std::string file = "squeezenet_v1.1.param";
            if (!file.empty())
            {
                nodes_.clear();
                delete loader_;
                loader_ = create_loader(file);
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
                    int i_next = 0;
                    for (auto node1 : node.nexts)
                    {
                        for (int i_prev = 0; i_prev < node1->prevs.size(); i_prev++)
                        {
                            if (node1->prevs[i_prev] == &node)
                            {
                                add_link(node.id + i_next, node1->text_id + i_prev, true);
                            }
                        }
                        i_next++;
                    }
                }

                if (!nodes_.empty())
                {
                    pending_focus_node_id_ = nodes_.front().id;
                    pending_focus_frames_ = 3;
                    pending_focus_top_anchor_ = true;
                }

                saved_ = true;
                reset_history_with_current();
            }
            if (queued_file)
            {
                begin_file_.clear();
                need_dialog_ = 0;
            }
        }

        commit_history_if_dirty();

        //refresh_pos_link();
        
        ImGui::End();
        first_run_ = 0;
    }
    void setBeginFile(const std::string& file)
    {
        begin_file_ = file;
    }

    void queueOpenFile(const std::string& file)
    {
        begin_file_ = file;
        need_dialog_ = 2;
    }
};

static ColorNodeEditor color_editor;
    } // namespace

void NodeEditorInitialize(int argc, char* argv[])
{
    ImGui::StyleColorsLight();
    ImGui::GetIO().IniFilename = nullptr;
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
#ifdef __EMSCRIPTEN__
    else
    {
        ex1::color_editor.setBeginFile("/models/squeezenet_v1.1.param");
    }
#endif
    FileLoader::mainPath() = filefunc::getFilePath(argv[0]);
}

void NodeEditorShow() { ex1::color_editor.show(); }

void NodeEditorSetExit(int e) { ex1::color_editor.is_exiting_ = e; }

void NodeEditorQueueOpenFile(const char* path)
{
    ex1::color_editor.queueOpenFile(path ? path : "");
}

void NodeEditorShutdown() {}
} // namespace example

#ifdef __EMSCRIPTEN__
extern "C"
{
EMSCRIPTEN_KEEPALIVE void NodeEditorSetPendingFile(const char* path)
{
    example::NodeEditorQueueOpenFile(path);
}
}
#endif
