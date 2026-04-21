#include "onnxloader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <unordered_set>

#include "filefunc.h"
#include "strfunc.h"

#ifdef NETEDIT_HAS_ONNX
#include <onnx/onnx_pb.h>
#ifndef ONNX_NAMESPACE
#define ONNX_NAMESPACE onnx
#endif
#endif

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

void parse_metadata_fallback(const std::string& content,
    std::map<std::string, std::map<int, std::string>>& int_to_string,
    std::map<std::string, std::map<std::string, int>>& string_to_int)
{
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
}

std::vector<std::string> metadata_candidates()
{
#ifdef __EMSCRIPTEN__
    return { "/onnx-metadata.json", "onnx-metadata.json", "./onnx-metadata.json" };
#else
#ifdef __APPLE__
    return { FileLoader::mainPath() + "/../Resources/onnx-metadata.json", FileLoader::mainPath() + "/onnx-metadata.json", "onnx-metadata.json" };
#else
    return { FileLoader::mainPath() + "/onnx-metadata.json", "onnx-metadata.json", "./onnx-metadata.json" };
#endif
#endif
}

std::string sanitize_blob_name(const std::string& s, int fallback_index)
{
    if (!s.empty())
    {
        return s;
    }
    return std::string("blob_") + std::to_string(fallback_index);
}

#ifdef NETEDIT_HAS_ONNX
std::string onnx_attr_value_to_text(const ONNX_NAMESPACE::AttributeProto& attr)
{
    switch (attr.type())
    {
    case ONNX_NAMESPACE::AttributeProto_AttributeType_FLOAT:
        return std::to_string(attr.f());
    case ONNX_NAMESPACE::AttributeProto_AttributeType_INT:
        return std::to_string(attr.i());
    case ONNX_NAMESPACE::AttributeProto_AttributeType_STRING:
        return attr.s();
    case ONNX_NAMESPACE::AttributeProto_AttributeType_FLOATS:
        return std::string("[") + std::to_string(attr.floats_size()) + "]";
    case ONNX_NAMESPACE::AttributeProto_AttributeType_INTS:
        return std::string("[") + std::to_string(attr.ints_size()) + "]";
    case ONNX_NAMESPACE::AttributeProto_AttributeType_STRINGS:
        return std::string("[") + std::to_string(attr.strings_size()) + "]";
    case ONNX_NAMESPACE::AttributeProto_AttributeType_TENSOR:
        return "tensor";
    default:
        return "?";
    }
}
#endif
}

onnxLoader::onnxLoader()
{
    for (const auto& candidate : metadata_candidates())
    {
        const std::string content = filefunc::readFileToString(candidate);
        if (content.empty())
        {
            continue;
        }
        parse_metadata_fallback(content, int_to_string_, string_to_int_);
        if (!int_to_string_.empty())
        {
            break;
        }
    }
}

void onnxLoader::fileToNodes(const std::string& filename, std::deque<Node>& nodes)
{
    nodes.clear();

#ifdef NETEDIT_HAS_ONNX
    ONNX_NAMESPACE::ModelProto model;
    {
        std::ifstream input(filename, std::ios::binary);
        if (!input.good() || !model.ParseFromIstream(&input))
        {
            return;
        }
    }

    const auto& graph = model.graph();

    std::unordered_set<std::string> initializer_names;
    for (const auto& init : graph.initializer())
    {
        initializer_names.insert(init.name());

        Node node;
        node.type = "Initializer";
        node.title = init.name();
        node.out.push_back(init.name());
        node.text = std::string("dims=") + std::to_string(init.dims_size());
        node.nexts.resize(1);
        nodes.push_back(std::move(node));
    }

    for (const auto& input : graph.input())
    {
        if (initializer_names.count(input.name()) > 0)
        {
            continue;
        }

        Node node;
        node.type = "Input";
        node.title = input.name();
        node.out.push_back(input.name());
        node.nexts.resize(1);
        nodes.push_back(std::move(node));
    }

    int auto_node_name = 0;
    int auto_blob_name = 0;
    for (const auto& op : graph.node())
    {
        Node node;
        node.type = op.op_type();
        node.title = op.name().empty() ? (op.op_type() + "_" + std::to_string(auto_node_name++)) : op.name();

        for (int i = 0; i < op.input_size(); ++i)
        {
            const std::string in_name = sanitize_blob_name(op.input(i), auto_blob_name++);
            node.in.push_back(in_name);
        }
        for (int i = 0; i < op.output_size(); ++i)
        {
            const std::string out_name = sanitize_blob_name(op.output(i), auto_blob_name++);
            node.out.push_back(out_name);
        }

        for (int i = 0; i < op.attribute_size(); ++i)
        {
            const auto& attr = op.attribute(i);
            std::string attr_name = attr.name();
            if (attr_name.empty())
            {
                const auto& attr_map = int_to_string_[node.type];
                if (attr_map.count(i) > 0)
                {
                    attr_name = attr_map.at(i);
                }
                else
                {
                    attr_name = std::to_string(i);
                }
            }
            node.values[attr_name] = onnx_attr_value_to_text(attr);
            if (!node.text.empty())
            {
                node.text += " ";
            }
            node.text += attr_name + "=" + onnx_attr_value_to_text(attr);
        }

        node.prevs.resize(node.in.size());
        node.nexts.resize(node.out.size());
        nodes.push_back(std::move(node));
    }

    for (const auto& output : graph.output())
    {
        Node node;
        node.type = "Output";
        node.title = output.name();
        node.in.push_back(output.name());
        node.prevs.resize(1);
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
#else
    (void)filename;
#endif
}

void onnxLoader::nodesToFile(const std::deque<Node>& nodes, const std::string& filename)
{
#ifdef NETEDIT_HAS_ONNX
    ONNX_NAMESPACE::ModelProto model;
    model.set_ir_version(8);
    auto* graph = model.mutable_graph();
    graph->set_name("nn_editor_graph");

    std::set<std::string> graph_inputs;
    std::set<std::string> graph_outputs;

    for (const auto& n : nodes)
    {
        if (n.erased)
        {
            continue;
        }

        auto* op = graph->add_node();
        op->set_name(n.title.empty() ? (n.type + "_" + std::to_string(op->input_size())) : n.title);
        op->set_op_type(n.type.empty() ? "Identity" : n.type);

        if (n.in.empty())
        {
            const std::string input_name = n.title + "_in";
            op->add_input(input_name);
            graph_inputs.insert(input_name);
        }
        else
        {
            for (const auto& in : n.in)
            {
                op->add_input(in);
            }
        }

        if (n.out.empty())
        {
            const std::string out_name = n.title + "_out";
            op->add_output(out_name);
            graph_outputs.insert(out_name);
        }
        else
        {
            for (const auto& out : n.out)
            {
                op->add_output(out);
                graph_outputs.insert(out);
            }
        }

        for (const auto& kv : n.values)
        {
            auto* attr = op->add_attribute();
            attr->set_name(kv.first);
            attr->set_type(ONNX_NAMESPACE::AttributeProto_AttributeType_STRING);
            attr->set_s(kv.second);
        }
    }

    // Inputs are tensors that appear in op inputs but never as op outputs.
    std::set<std::string> produced;
    std::set<std::string> consumed;
    for (int i = 0; i < graph->node_size(); ++i)
    {
        const auto& op = graph->node(i);
        for (const auto& in : op.input())
        {
            consumed.insert(in);
        }
        for (const auto& out : op.output())
        {
            produced.insert(out);
        }
    }

    for (const auto& in : consumed)
    {
        if (produced.count(in) == 0)
        {
            auto* vi = graph->add_input();
            vi->set_name(in);
        }
    }
    for (const auto& out : produced)
    {
        if (consumed.count(out) == 0)
        {
            auto* vi = graph->add_output();
            vi->set_name(out);
        }
    }

    std::ofstream os(filename, std::ios::binary);
    if (!os.good())
    {
        return;
    }
    model.SerializeToOstream(&os);
#else
    (void)nodes;
    (void)filename;
#endif
}
