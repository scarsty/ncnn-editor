#include "tnnloader.h"

#include "FileLoader.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

#include "filefunc.h"
#include "strfunc.h"

namespace {
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
                           std::map<std::string, std::map<int, std::string> >& int_to_string,
                           std::map<std::string, std::map<std::string, int> >& string_to_int)
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
                             std::map<std::string, std::map<int, std::string> >& int_to_string,
                             std::map<std::string, std::map<std::string, int> >& string_to_int)
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

std::string trim_copy(std::string s)
{
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n'))
    {
        s.pop_back();
    }
    size_t start = 0;
    while (start < s.size() && (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n'))
    {
        ++start;
    }
    return s.substr(start);
}

std::vector<std::string> tokenize_tnn_line(const std::string& line)
{
    std::vector<std::string> tokens;
    std::string cur;
    for (char ch : line)
    {
        if (ch == '"' || ch == '\r')
        {
            continue;
        }
        if (ch == ' ' || ch == '\t' || ch == ',')
        {
            if (!cur.empty())
            {
                tokens.push_back(cur);
                cur.clear();
            }
            continue;
        }
        cur.push_back(ch);
    }
    if (!cur.empty())
    {
        tokens.push_back(cur);
    }
    return tokens;
}

bool parse_int(const std::string& s, int& out)
{
    if (s.empty())
    {
        return false;
    }
    char* end = nullptr;
    long v = std::strtol(s.c_str(), &end, 10);
    if (end == nullptr || *end != '\0')
    {
        return false;
    }
    out = static_cast<int>(v);
    return true;
}

std::string companion_tnnproto(const std::string& filename)
{
    const std::string ext = strfunc::toLowerCase(filefunc::getFileExt(filename));
    if (ext == "tnnproto")
    {
        return filename;
    }
    if (ext == "tnnmodel")
    {
        return filefunc::changeFileExt(filename, "tnnproto");
    }
    return filename;
}

const std::map<int, std::string>* find_attr_index_to_name_map(
    const std::map<std::string, std::map<int, std::string> >& int_to_string,
    const std::string& type)
{
    auto it = int_to_string.find(type);
    if (it != int_to_string.end())
    {
        return &it->second;
    }

    const std::string lower = strfunc::toLowerCase(type);
    for (const auto& kv : int_to_string)
    {
        if (strfunc::toLowerCase(kv.first) == lower)
        {
            return &kv.second;
        }
    }
    return nullptr;
}

const std::map<std::string, int>* find_attr_name_to_index_map(
    const std::map<std::string, std::map<std::string, int> >& string_to_int,
    const std::string& type)
{
    auto it = string_to_int.find(type);
    if (it != string_to_int.end())
    {
        return &it->second;
    }

    const std::string lower = strfunc::toLowerCase(type);
    for (const auto& kv : string_to_int)
    {
        if (strfunc::toLowerCase(kv.first) == lower)
        {
            return &kv.second;
        }
    }
    return nullptr;
}
} // namespace

tnnLoader::tnnLoader()
{
    for (const auto& candidate : metadataCandidates("tnn-metadata.json"))
    {
        const std::string content = filefunc::readFileToString(candidate);
        if (content.empty())
        {
            continue;
        }
        if (parse_metadata_fallback(content, int_to_string_, string_to_int_))
        {
            break;
        }
    }
}

void tnnLoader::fileToNodes(const std::string& filename, std::deque<Node>& nodes)
{
    nodes.clear();

    const std::string proto_file = companion_tnnproto(filename);
    std::string content = filefunc::readFileToString(proto_file);
    if (content.empty())
    {
        return;
    }

    content = strfunc::replaceAllSubString(content, "\r", "");
    std::vector<std::string> lines = strfunc::splitString(content, "\n");

    for (const std::string& raw : lines)
    {
        const std::string line = trim_copy(raw);
        if (line.empty())
        {
            continue;
        }
        if (line[0] == '#')
        {
            continue;
        }

        std::vector<std::string> tok = tokenize_tnn_line(line);
        if (tok.size() < 5)
        {
            continue;
        }

        int in_count = 0;
        int out_count = 0;
        if (!parse_int(tok[2], in_count) || !parse_int(tok[3], out_count))
        {
            continue;
        }
        if (in_count < 0 || out_count < 0)
        {
            continue;
        }
        if (static_cast<int>(tok.size()) < 4 + in_count + out_count)
        {
            continue;
        }

        Node node;
        node.type = tok[0];
        node.title = tok[1];

        for (int i = 0; i < in_count; ++i)
        {
            node.in.push_back(tok[4 + i]);
        }
        for (int i = 0; i < out_count; ++i)
        {
            node.out.push_back(tok[4 + in_count + i]);
        }

        for (int i = 4 + in_count + out_count; i < static_cast<int>(tok.size()); ++i)
        {
            node.text += tok[i];
            if (i + 1 < static_cast<int>(tok.size()))
            {
                node.text += " ";
            }
        }
        refreshNodeValues(node);

        node.prevs.resize(node.in.size());
        node.nexts.resize(node.out.size());
        nodes.push_back(std::move(node));
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

void tnnLoader::nodesToFile(const std::deque<Node>& nodes, const std::string& filename)
{
    std::string out;
    out += "\"1 0 0\"\n";
    out += std::to_string(nodes.size()) + " " + std::to_string(nodes.size() * 2) + "\n";

    for (const auto& node : nodes)
    {
        if (node.erased)
        {
            continue;
        }

        std::vector<std::string> ins = node.in;
        std::vector<std::string> outs = node.out;

        if (ins.empty())
        {
            for (const auto* prev : node.prevs)
            {
                if (prev != nullptr)
                {
                    ins.push_back(prev->title);
                }
            }
        }
        if (outs.empty())
        {
            for (int i = 0; i < static_cast<int>(node.nexts.size()); ++i)
            {
                outs.push_back(node.title + "_o" + std::to_string(i));
            }
            if (outs.empty())
            {
                outs.push_back(node.title);
            }
        }

        out += node.type + " " + node.title + " ";
        out += std::to_string(ins.size()) + " " + std::to_string(outs.size());

        for (const auto& in : ins)
        {
            out += " " + in;
        }
        for (const auto& o : outs)
        {
            out += " " + o;
        }

        std::vector<std::string> params;
        const auto* name_to_index = find_attr_name_to_index_map(string_to_int_, node.type);
        for (const auto& kv : node.values)
        {
            if (kv.second.empty())
            {
                continue;
            }

            if (name_to_index != nullptr)
            {
                auto it = name_to_index->find(kv.first);
                if (it != name_to_index->end())
                {
                    params.push_back(std::to_string(it->second) + "=" + kv.second);
                    continue;
                }
            }

            int numeric_key = 0;
            if (parse_int(kv.first, numeric_key))
            {
                params.push_back(std::to_string(numeric_key) + "=" + kv.second);
            }
            else
            {
                params.push_back(kv.first + "=" + kv.second);
            }
        }

        if (params.empty() && !node.text.empty())
        {
            params.push_back(node.text);
        }

        for (const auto& p : params)
        {
            out += " " + p;
        }
        out += "\n";
    }

    filefunc::writeStringToFile(out, filename);
}

void tnnLoader::refreshNodeValues(Node& n)
{
    n.values.clear();
    if (n.text.empty())
    {
        return;
    }

    const std::vector<std::string> tokens = strfunc::splitString(n.text, " ");
    const auto* index_to_name = find_attr_index_to_name_map(int_to_string_, n.type);
    int positional_index = 0;
    for (const auto& token : tokens)
    {
        if (token.empty())
        {
            continue;
        }

        const std::vector<std::string> kv = strfunc::splitString(token, "=");
        if (kv.size() >= 2)
        {
            int key_index = 0;
            if (parse_int(kv[0], key_index) && index_to_name != nullptr && index_to_name->count(key_index) > 0)
            {
                n.values[index_to_name->at(key_index)] = kv[1];
            }
            else
            {
                n.values[kv[0]] = kv[1];
            }
            continue;
        }

        if (index_to_name != nullptr && index_to_name->count(positional_index) > 0)
        {
            n.values[index_to_name->at(positional_index)] = token;
        }
        else
        {
            n.values[std::to_string(positional_index)] = token;
        }
        ++positional_index;
    }
}
