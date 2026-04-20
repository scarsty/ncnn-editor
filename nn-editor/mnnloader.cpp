#include "mnnloader.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <set>

#include "filefunc.h"
#include "strfunc.h"

namespace
{
size_t find_key_colon(const std::string& obj, const std::string& key)
{
    const std::string needle = std::string("\"") + key + "\"";
    size_t p = obj.find(needle);
    if (p == std::string::npos)
    {
        return std::string::npos;
    }
    p = obj.find(':', p + needle.size());
    if (p == std::string::npos)
    {
        return std::string::npos;
    }
    return p + 1;
}

size_t skip_ws(const std::string& s, size_t i)
{
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n'))
    {
        ++i;
    }
    return i;
}

bool parse_quoted(const std::string& s, size_t& i, std::string& out)
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

size_t find_match(const std::string& s, size_t pos, char open_ch, char close_ch)
{
    if (pos >= s.size() || s[pos] != open_ch)
    {
        return std::string::npos;
    }
    int depth = 0;
    bool in_str = false;
    for (size_t i = pos; i < s.size(); ++i)
    {
        char ch = s[i];
        if (in_str)
        {
            if (ch == '\\')
            {
                ++i;
                continue;
            }
            if (ch == '"')
            {
                in_str = false;
            }
            continue;
        }
        if (ch == '"')
        {
            in_str = true;
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

std::vector<std::string> parse_string_array(const std::string& s, size_t lb)
{
    std::vector<std::string> out;
    size_t rb = find_match(s, lb, '[', ']');
    if (rb == std::string::npos)
    {
        return out;
    }
    size_t i = lb + 1;
    while (i < rb)
    {
        i = skip_ws(s, i);
        if (i >= rb)
        {
            break;
        }
        std::string val;
        if (parse_quoted(s, i, val))
        {
            out.push_back(val);
            continue;
        }
        ++i;
    }
    return out;
}

std::vector<int> parse_int_array(const std::string& s, size_t lb)
{
    std::vector<int> out;
    size_t rb = find_match(s, lb, '[', ']');
    if (rb == std::string::npos)
    {
        return out;
    }
    size_t i = lb + 1;
    while (i < rb)
    {
        i = skip_ws(s, i);
        if (i >= rb)
        {
            break;
        }
        char* end = nullptr;
        long v = std::strtol(s.c_str() + i, &end, 10);
        if (end != nullptr && end > s.c_str() + i)
        {
            out.push_back(static_cast<int>(v));
            i = static_cast<size_t>(end - s.c_str());
            continue;
        }
        ++i;
    }
    return out;
}

std::string get_object_string_value(const std::string& obj, const std::string& key)
{
    size_t p = find_key_colon(obj, key);
    if (p == std::string::npos)
    {
        return "";
    }
    p = skip_ws(obj, p);
    std::string val;
    if (parse_quoted(obj, p, val))
    {
        return val;
    }
    return "";
}

bool get_object_int_value(const std::string& obj, const std::string& key, int& out)
{
    size_t p = find_key_colon(obj, key);
    if (p == std::string::npos)
    {
        return false;
    }
    p = skip_ws(obj, p);
    char* end = nullptr;
    long v = std::strtol(obj.c_str() + p, &end, 10);
    if (end == nullptr || end == obj.c_str() + p)
    {
        return false;
    }
    out = static_cast<int>(v);
    return true;
}

std::vector<std::string> metadata_candidates()
{
#ifdef __EMSCRIPTEN__
    return { "/mnn-metadata.json", "mnn-metadata.json", "./mnn-metadata.json" };
#else
#ifdef __APPLE__
    return { FileLoader::mainPath() + "/../Resources/mnn-metadata.json", FileLoader::mainPath() + "/mnn-metadata.json", "mnn-metadata.json" };
#else
    return { FileLoader::mainPath() + "/mnn-metadata.json", "mnn-metadata.json", "./mnn-metadata.json" };
#endif
#endif
}

void parse_mnn_operator_metadata(const std::string& content, std::map<int, std::string>& operator_to_name)
{
    size_t i = 0;
    while (true)
    {
        i = content.find('{', i);
        if (i == std::string::npos)
        {
            break;
        }
        size_t end = find_match(content, i, '{', '}');
        if (end == std::string::npos)
        {
            break;
        }

        const std::string obj = content.substr(i, end - i + 1);
        const std::string name = get_object_string_value(obj, "name");
        int op = 0;
        if (!name.empty() && get_object_int_value(obj, "operator", op))
        {
            operator_to_name[op] = name;
        }

        i = end + 1;
    }
}

std::vector<int> get_object_int_array(const std::string& obj, const std::string& key)
{
    const std::string needle = std::string("\"") + key + "\"";
    size_t p = obj.find(needle);
    if (p == std::string::npos)
    {
        return {};
    }
    p = obj.find('[', p + needle.size());
    if (p == std::string::npos)
    {
        return {};
    }
    return parse_int_array(obj, p);
}

std::string json_escape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char ch : s)
    {
        if (ch == '"' || ch == '\\')
        {
            out.push_back('\\');
        }
        out.push_back(ch);
    }
    return out;
}

std::string choose_output_name(const Node& node, int idx)
{
    if (idx < static_cast<int>(node.out.size()) && !node.out[idx].empty())
    {
        return node.out[idx];
    }
    if (!node.title.empty())
    {
        return node.title + "_out" + std::to_string(idx);
    }
    return "tensor_out_" + std::to_string(idx);
}
}

mnnLoader::mnnLoader()
{
    for (const auto& candidate : metadata_candidates())
    {
        const std::string content = filefunc::readFileToString(candidate);
        if (content.empty())
        {
            continue;
        }
        parse_mnn_operator_metadata(content, operator_to_name_);
        if (!operator_to_name_.empty())
        {
            break;
        }
    }
}

void mnnLoader::fileToNodes(const std::string& filename, std::deque<Node>& nodes)
{
    nodes.clear();
    std::string content = filefunc::readFileToString(filename);
    if (content.empty())
    {
        return;
    }

    // This loader targets MNN JSON graph format (commonly exported/intermediate).
    // For opaque binary .mnn, we keep a placeholder node so users still get feedback.
    size_t tensor_name_key = content.find("\"tensorName\"");
    size_t oplists_key = content.find("\"oplists\"");
    if (tensor_name_key == std::string::npos || oplists_key == std::string::npos)
    {
        Node placeholder;
        placeholder.type = "MNNBinary";
        placeholder.title = filefunc::getFilenameWithoutPath(filename);
        placeholder.text = "Binary .mnn is not directly parseable without MNN schema runtime";
        nodes.push_back(std::move(placeholder));
        calPosition(nodes);
        return;
    }

    std::vector<std::string> tensor_names;
    {
        size_t lb = content.find('[', tensor_name_key);
        if (lb != std::string::npos)
        {
            tensor_names = parse_string_array(content, lb);
        }
    }

    {
        size_t lb = content.find('[', oplists_key);
        size_t rb = lb == std::string::npos ? std::string::npos : find_match(content, lb, '[', ']');
        if (lb == std::string::npos || rb == std::string::npos)
        {
            return;
        }

        size_t i = lb + 1;
        while (i < rb)
        {
            i = content.find('{', i);
            if (i == std::string::npos || i >= rb)
            {
                break;
            }
            size_t j = find_match(content, i, '{', '}');
            if (j == std::string::npos || j > rb)
            {
                break;
            }

            std::string obj = content.substr(i, j - i + 1);
            Node node;
            node.title = get_object_string_value(obj, "name");
            node.type = get_object_string_value(obj, "type");
            if (node.type.empty())
            {
                int type_id = 0;
                if (get_object_int_value(obj, "type", type_id))
                {
                    auto it = operator_to_name_.find(type_id);
                    node.type = (it == operator_to_name_.end()) ? std::to_string(type_id) : it->second;
                }
            }
            if (node.title.empty())
            {
                node.title = "mnn_op_" + std::to_string(nodes.size());
            }
            if (node.type.empty())
            {
                node.type = "Unknown";
            }

            std::vector<int> in_idx = get_object_int_array(obj, "inputIndexes");
            std::vector<int> out_idx = get_object_int_array(obj, "outputIndexes");

            for (int idx : in_idx)
            {
                if (idx >= 0 && idx < static_cast<int>(tensor_names.size()))
                {
                    node.in.push_back(tensor_names[idx]);
                }
                else
                {
                    node.in.push_back("tensor_" + std::to_string(idx));
                }
            }
            for (int idx : out_idx)
            {
                if (idx >= 0 && idx < static_cast<int>(tensor_names.size()))
                {
                    node.out.push_back(tensor_names[idx]);
                }
                else
                {
                    node.out.push_back("tensor_" + std::to_string(idx));
                }
            }

            node.prevs.resize(node.in.size());
            node.nexts.resize(node.out.size());
            nodes.push_back(std::move(node));
            i = j + 1;
        }
    }

    for (auto& src : nodes)
    {
        for (auto& dst : nodes)
        {
            for (int i_out = 0; i_out < static_cast<int>(src.out.size()); ++i_out)
            {
                for (int i_in = 0; i_in < static_cast<int>(dst.in.size()); ++i_in)
                {
                    if (src.out[i_out] == dst.in[i_in])
                    {
                        src.nexts[i_out] = &dst;
                        dst.prevs[i_in] = &src;
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

void mnnLoader::nodesToFile(const std::deque<Node>& nodes, const std::string& filename)
{
    std::set<std::string> tensor_set;
    std::vector<std::string> tensor_names;
    auto add_tensor = [&](const std::string& name)
    {
        if (name.empty() || tensor_set.count(name) > 0)
        {
            return;
        }
        tensor_set.insert(name);
        tensor_names.push_back(name);
    };

    for (const auto& node : nodes)
    {
        if (node.erased)
        {
            continue;
        }
        for (const auto& in : node.in)
        {
            add_tensor(in);
        }
        for (const auto& out : node.out)
        {
            add_tensor(out);
        }
    }

    std::map<std::string, int> tensor_index;
    for (int i = 0; i < static_cast<int>(tensor_names.size()); ++i)
    {
        tensor_index[tensor_names[i]] = i;
    }

    std::string json = "{\n  \"tensorName\": [\n";
    for (int i = 0; i < static_cast<int>(tensor_names.size()); ++i)
    {
        json += "    \"" + json_escape(tensor_names[i]) + "\"";
        json += (i + 1 < static_cast<int>(tensor_names.size())) ? ",\n" : "\n";
    }
    json += "  ],\n  \"oplists\": [\n";

    int op_idx = 0;
    for (const auto& node : nodes)
    {
        if (node.erased)
        {
            continue;
        }
        if (op_idx > 0)
        {
            json += ",\n";
        }
        json += "    {\n";
        json += "      \"name\": \"" + json_escape(node.title) + "\",\n";
        json += "      \"type\": \"" + json_escape(node.type) + "\",\n";

        json += "      \"inputIndexes\": [";
        for (int i = 0; i < static_cast<int>(node.in.size()); ++i)
        {
            auto it = tensor_index.find(node.in[i]);
            int idx = it == tensor_index.end() ? -1 : it->second;
            json += std::to_string(idx);
            if (i + 1 < static_cast<int>(node.in.size()))
            {
                json += ", ";
            }
        }
        json += "],\n";

        json += "      \"outputIndexes\": [";
        for (int i = 0; i < static_cast<int>(node.out.size()); ++i)
        {
            auto it = tensor_index.find(node.out[i]);
            int idx = it == tensor_index.end() ? -1 : it->second;
            json += std::to_string(idx);
            if (i + 1 < static_cast<int>(node.out.size()))
            {
                json += ", ";
            }
        }
        json += "]\n";
        json += "    }";
        ++op_idx;
    }

    json += "\n  ]\n}\n";
    filefunc::writeStringToFile(json, filename);
}
