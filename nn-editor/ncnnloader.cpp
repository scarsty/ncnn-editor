#include "ncnnloader.h"
#include "filefunc.h"
#include "strfunc.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <functional>
#include <vector>
#ifdef NETEDIT_HAS_YAML_CPP
#include "yaml-cpp/yaml.h"
#endif
#include <iostream>

namespace
{
size_t skip_ws(const std::string& s, size_t i)
{
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])))
    {
        ++i;
    }
    return i;
}

bool parse_quoted_string(const std::string& s, size_t& i, std::string& out)
{
    if (i >= s.size() || s[i] != '"')
    {
        return false;
    }
    ++i;
    out.clear();

    while (i < s.size())
    {
        char ch = s[i++];
        if (ch == '\\')
        {
            if (i < s.size())
            {
                out.push_back(s[i++]);
            }
            continue;
        }
        if (ch == '"')
        {
            return true;
        }
        out.push_back(ch);
    }
    return false;
}

size_t find_matching(const std::string& s, size_t open_pos, char open_ch, char close_ch)
{
    if (open_pos >= s.size() || s[open_pos] != open_ch)
    {
        return std::string::npos;
    }

    int depth = 0;
    bool in_string = false;
    for (size_t i = open_pos; i < s.size(); ++i)
    {
        char ch = s[i];
        if (in_string)
        {
            if (ch == '\\')
            {
                ++i;
                continue;
            }
            if (ch == '"')
            {
                in_string = false;
            }
            continue;
        }

        if (ch == '"')
        {
            in_string = true;
            continue;
        }
        if (ch == open_ch)
        {
            ++depth;
            continue;
        }
        if (ch == close_ch)
        {
            --depth;
            if (depth == 0)
            {
                return i;
            }
        }
    }
    return std::string::npos;
}

bool extract_top_level_name(const std::string& object_text, std::string& layer_name)
{
    bool in_string = false;
    int brace_depth = 0;
    int bracket_depth = 0;

    for (size_t i = 0; i < object_text.size(); ++i)
    {
        char ch = object_text[i];
        if (in_string)
        {
            if (ch == '\\')
            {
                ++i;
                continue;
            }
            if (ch == '"')
            {
                in_string = false;
            }
            continue;
        }

        if (ch == '"')
        {
            in_string = true;
            std::string key;
            size_t p = i;
            if (!parse_quoted_string(object_text, p, key))
            {
                continue;
            }
            p = skip_ws(object_text, p);
            if (brace_depth == 1 && bracket_depth == 0 && key == "name" && p < object_text.size() && object_text[p] == ':')
            {
                ++p;
                p = skip_ws(object_text, p);
                std::string value;
                if (parse_quoted_string(object_text, p, value))
                {
                    layer_name = value;
                    return true;
                }
            }
            i = p > 0 ? p - 1 : i;
            continue;
        }
        if (ch == '{')
        {
            ++brace_depth;
            continue;
        }
        if (ch == '}')
        {
            --brace_depth;
            continue;
        }
        if (ch == '[')
        {
            ++bracket_depth;
            continue;
        }
        if (ch == ']')
        {
            --bracket_depth;
            continue;
        }
    }

    return false;
}

void parse_attribute_names(const std::string& attrs_text, const std::string& layer_name,
    std::map<std::string, std::map<int, std::string>>& int_to_string,
    std::map<std::string, std::map<std::string, int>>& string_to_int)
{
    size_t i = 0;
    int attr_index = 0;
    while (i < attrs_text.size())
    {
        i = attrs_text.find('{', i);
        if (i == std::string::npos)
        {
            break;
        }

        size_t end = find_matching(attrs_text, i, '{', '}');
        if (end == std::string::npos)
        {
            break;
        }

        std::string attr_obj = attrs_text.substr(i, end - i + 1);
        std::string attr_name;
        if (extract_top_level_name(attr_obj, attr_name))
        {
            int_to_string[layer_name][attr_index] = attr_name;
            string_to_int[layer_name][attr_name] = attr_index;
        }
        ++attr_index;
        i = end + 1;
    }
}

bool parse_metadata_fallback(const std::string& content,
    std::map<std::string, std::map<int, std::string>>& int_to_string,
    std::map<std::string, std::map<std::string, int>>& string_to_int)
{
    bool any_layer = false;
    size_t i = 0;

    while (true)
    {
        i = content.find('{', i);
        if (i == std::string::npos)
        {
            break;
        }

        size_t end = find_matching(content, i, '{', '}');
        if (end == std::string::npos)
        {
            break;
        }

        std::string obj = content.substr(i, end - i + 1);
        std::string layer_name;
        if (extract_top_level_name(obj, layer_name) && !layer_name.empty())
        {
            any_layer = true;
            size_t attrs_pos = obj.find("\"attributes\"");
            if (attrs_pos != std::string::npos)
            {
                size_t lb = obj.find('[', attrs_pos);
                if (lb != std::string::npos)
                {
                    size_t rb = find_matching(obj, lb, '[', ']');
                    if (rb != std::string::npos)
                    {
                        parse_attribute_names(obj.substr(lb, rb - lb + 1), layer_name, int_to_string, string_to_int);
                    }
                }
            }
        }

        i = end + 1;
    }

    return any_layer;
}

std::vector<std::string> metadata_candidates()
{
#ifdef __EMSCRIPTEN__
    return { "/ncnn-metadata.json", "ncnn-metadata.json", "./ncnn-metadata.json" };
#else
#ifdef __APPLE__
    return { FileLoader::mainPath() + "/../Resources/ncnn-metadata.json", FileLoader::mainPath() + "/ncnn-metadata.json", "ncnn-metadata.json" };
#else
    return { FileLoader::mainPath() + "/ncnn-metadata.json", "ncnn-metadata.json", "./ncnn-metadata.json" };
#endif
#endif
}
}

ncnnLoader::ncnnLoader()
{
    bool loaded = false;

#ifdef NETEDIT_HAS_YAML_CPP
    for (const auto& candidate : metadata_candidates())
    {
        try
        {
            YAML::Node node = YAML::LoadFile(candidate);
            for (auto n : node)
            {
                if (n["attributes"].IsSequence())
                {
                    for (int i = 0; i < n["attributes"].size(); i++)
                    {
                        int_to_string_[n["name"].as<std::string>()][i] = n["attributes"][i]["name"].as<std::string>();
                        string_to_int_[n["name"].as<std::string>()][n["attributes"][i]["name"].as<std::string>()] = i;
                    }
                }
            }
            loaded = !int_to_string_.empty() || !string_to_int_.empty();
            if (loaded)
            {
                break;
            }
        }
        catch (...)
        {
        }
    }
#endif

    if (!loaded)
    {
        for (const auto& candidate : metadata_candidates())
        {
            const std::string content = filefunc::readFileToString(candidate);
            if (content.empty())
            {
                continue;
            }
            if (parse_metadata_fallback(content, int_to_string_, string_to_int_))
            {
                loaded = true;
                break;
            }
        }
    }

    if (!loaded)
    {
        std::cerr << "[ncnnLoader] Warning: failed to load ncnn-metadata.json; attribute names will be limited." << std::endl;
    }
}

void ncnnLoader::fileToNodes(const std::string& filename, std::deque<Node>& nodes)
{
    nodes.clear();
    is_pnnx_ = false;
    auto str = filefunc::readFileToString(filename);
    if (str.find("pnnx") != std::string::npos)
    {
        is_pnnx_ = true;
    }
    str = strfunc::replaceAllSubString(str, "\r", "");
    auto lines = strfunc::splitString(str, "\n");
    int layer_count = 0, blob_count = 0;
    if (lines.size() >= 2)
    {
        auto v = strfunc::findNumbers<int>(lines[1]);
        layer_count = v[0];
        blob_count = v[1];
    }

    for (int i = 2; i < lines.size(); i++)
    {
        auto v = strfunc::splitString(lines[i], " ");
        if (v.size() >= 2)
        {
            Node node;
            node.title = v[1];
            node.type = v[0];

            int input_count = atoi(v[2].c_str());
            int output_count = atoi(v[3].c_str());

            for (int i_in = 0; i_in < input_count; i_in++)
            {
                node.in.push_back(v[4 + i_in]);
            }
            for (int i_out = 0; i_out < output_count; i_out++)
            {
                node.out.push_back(v[4 + input_count + i_out]);
            }
            for (int i_rest = 4 + input_count + output_count; i_rest < v.size(); i_rest++)
            {
                node.text += v[i_rest] + " ";
            }
            if (!node.text.empty())
            {
                node.text.pop_back();
            }
            refreshNodeValues(node);
            node.prevs.resize(input_count);
            if (!is_pnnx_)
            {
                node.nexts.resize(output_count);
            }
            nodes.push_back(std::move(node));
        }
    }

    for (auto& node1 : nodes)
    {
        for (auto& node2 : nodes)
        {
            for (int i_from = 0; i_from < node1.out.size(); i_from++)
            {
                for (int i_to = 0; i_to < node2.in.size(); i_to++)
                {
                    if (node1.out[i_from] == node2.in[i_to])
                    {
                        if (is_pnnx_)
                        {
                            node1.nexts.push_back(&node2);
                        }
                        else
                        {
                            node1.nexts[i_from] = &node2;
                        }
                        node2.prevs[i_to] = &node1;
                    }
                }
            }
        }
    }
    for (auto& n : nodes)
    {
        n.prevs.erase(std::remove(n.prevs.begin(), n.prevs.end(), nullptr), n.prevs.end());
        n.nexts.erase(std::remove(n.nexts.begin(), n.nexts.end(), nullptr), n.nexts.end());
    }
    calPosition(nodes);
}

void ncnnLoader::nodesToFile(const std::deque<Node>& nodes, const std::string& filename)
{
    std::vector<Node*> nodes_turn;
    for (auto& n : nodes)
    {
        if (n.prevs.size() == 0)
        {
            push_cal_stack((Node*)&n, 1, nodes_turn, true);
            break;
        }
    }

    nodes_turn.back()->nexts.resize(1);

    std::vector<std::string> lines(2);
    lines[0] = "7767517";

    int blob_count = 0;
    for (auto& n : nodes_turn)
    {
        blob_count += n->nexts.size();
        auto l = std::format("{:<16} {:<16} {} {} ", n->type, n->title, n->prevs.size(), n->nexts.size());
        for (auto& n1 : n->prevs)
        {
            if (n1->nexts.size() == 1)
            {
                l += n1->title + " ";
            }
            else
            {
                l += n1->title + "_" + n->title + " ";
            }
        }
        if (n->nexts.size() <= 1)
        {
            l += n->title + " ";
        }
        else
        {
            for (auto& n1 : n->nexts)
            {
                l += n->title + "_" + n1->title + " ";
            }
        }
        std::vector<std::string> v;
        for (auto& kv : n->values)
        {
            if (!kv.second.empty())
            {
                if (string_to_int_[n->type].count(kv.first))
                {
                    v.push_back(std::to_string(string_to_int_[n->type][kv.first]) + "=" + kv.second + " ");
                }
                else
                {
                    v.push_back(kv.first + "=" + kv.second + " ");
                }
            }
        }
        std::sort(v.begin(), v.end());
        for (auto& kv : v)
        {
            l += kv;
        }
        l.pop_back();
        //l += strfunc::replaceAllSubString(n->text, "\n", " ");
        lines.push_back(std::move(l));
    }
    lines[1] = std::format("{} {}", nodes_turn.size(), blob_count);
    std::string str;
    for (auto& l : lines)
    {
        str += l + "\n";
    }
    std::cout << str;
    filefunc::writeStringToFile(str, filename);
}

void ncnnLoader::refreshNodeValues(Node& n)
{
    auto strs = strfunc::splitString(n.text, " ");
    if (!is_pnnx_)
    {
        for (auto& kv : int_to_string_[n.type])
        {
            if (!n.values.count(kv.second) && kv.second != "")
            {
                n.values[kv.second] = "";
                for (auto& str : strs)
                {
                    auto kv1 = strfunc::splitString(str, "=");
                    if (kv1.size() >= 2)
                    {
                        auto i = atoi(kv1[0].c_str());
                        if (int_to_string_[n.type][i] == kv.second)
                        {
                            n.values[kv.second] = kv1[1];
                        }
                    }

                }
            }
        }
        for (auto& str : strs)
        {
            auto kv = strfunc::splitString(str, "=");
            if (kv.size() >= 2 && int_to_string_[n.type].count(atoi(kv[0].c_str())) == 0)
            {
                n.values[kv[0]] = kv[1];
            }
        }
    }
    else
    {
        for (auto& str : strs)
        {
            auto kv = strfunc::splitString(str, "=");
            if (kv.size() >= 2)
            {
                n.values[kv[0]] = kv[1];
            }
        }
    }
    n.text = "";
}


