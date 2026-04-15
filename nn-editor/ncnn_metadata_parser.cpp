#include "ncnn_metadata_parser.h"

#include <cctype>

#include "filefunc.h"

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
}

bool load_ncnn_metadata_from_json(
    const std::vector<std::string>& candidates,
    std::map<std::string, std::map<int, std::string>>& int_to_string,
    std::map<std::string, std::map<std::string, int>>& string_to_int)
{
    for (const auto& candidate : candidates)
    {
        const std::string content = filefunc::readFileToString(candidate);
        if (content.empty())
        {
            continue;
        }
        if (parse_metadata_fallback(content, int_to_string, string_to_int))
        {
            return true;
        }
    }
    return false;
}
