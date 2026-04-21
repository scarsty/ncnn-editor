#include "onnxloader.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <set>
#include <unordered_set>
#ifndef NETEDIT_HAS_ONNX
#include <cstdint>
#include <cstring>
#include <vector>
#endif

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

#ifndef NETEDIT_HAS_ONNX
// Minimal protobuf binary reader for ONNX parsing (no onnx library needed).
// Field numbers from onnx-ml.proto:
//   ModelProto:     graph=7
//   GraphProto:     node=1, initializer=5, input=11, output=12
//   NodeProto:      input=1, output=2, name=3, op_type=4, attribute=5
//   TensorProto:    dims=1, name=8
//   ValueInfoProto: name=1
//   AttributeProto: name=1, f=2(I32), i=3(VARINT), s=4(LEN),
//                   floats=7, ints=8, strings=9, type=20(VARINT)
//   AttributeType:  FLOAT=1,INT=2,STRING=3,TENSOR=4,GRAPH=5,FLOATS=6,INTS=7,STRINGS=8
namespace pb
{
    enum : int { VARINT = 0, I64 = 1, LEN = 2, I32 = 5 };

    struct Reader
    {
        const uint8_t* p;
        const uint8_t* end;

        bool good() const { return p < end; }

        uint64_t read_varint()
        {
            uint64_t r = 0;
            int shift = 0;
            while (p < end)
            {
                uint8_t b = *p++;
                r |= static_cast<uint64_t>(b & 0x7Fu) << shift;
                if (!(b & 0x80u)) { break; }
                shift += 7;
            }
            return r;
        }

        void read_tag(int& fn, int& wt)
        {
            uint64_t tag = read_varint();
            fn = static_cast<int>(tag >> 3);
            wt = static_cast<int>(tag & 7u);
        }

        void skip(int wt)
        {
            switch (wt)
            {
            case VARINT: read_varint(); break;
            case I64:    if (p + 8 <= end) p += 8; else p = end; break;
            case LEN:    { size_t n = static_cast<size_t>(read_varint()); if (p + n <= end) p += n; else p = end; break; }
            case I32:    if (p + 4 <= end) p += 4; else p = end; break;
            default:     p = end; break;
            }
        }

        std::string read_str()
        {
            size_t n = static_cast<size_t>(read_varint());
            if (p + n > end) { p = end; return {}; }
            std::string s(reinterpret_cast<const char*>(p), n);
            p += n;
            return s;
        }

        Reader sub()
        {
            size_t n = static_cast<size_t>(read_varint());
            if (p + n > end) { p = end; return { end, end }; }
            Reader r{ p, p + n };
            p += n;
            return r;
        }

        float read_f32()
        {
            if (p + 4 > end) { p = end; return 0.0f; }
            float v;
            std::memcpy(&v, p, 4);
            p += 4;
            return v;
        }
    };

    std::pair<std::string, std::string> parse_attr(Reader r)
    {
        std::string name;
        int type = 0;
        float f = 0.0f;
        int64_t i = 0;
        std::string s;
        int floats_count = 0, ints_count = 0, strings_count = 0;

        while (r.good())
        {
            int fn, wt;
            r.read_tag(fn, wt);
            if      (fn == 1  && wt == LEN)    { name = r.read_str(); }
            else if (fn == 20 && wt == VARINT)  { type = static_cast<int>(r.read_varint()); }
            else if (fn == 2  && wt == I32)     { f = r.read_f32(); }
            else if (fn == 3  && wt == VARINT)  { i = static_cast<int64_t>(r.read_varint()); }
            else if (fn == 4  && wt == LEN)     { s = r.read_str(); }
            else if (fn == 7  && wt == LEN)     { Reader pk = r.sub(); while (pk.good()) { pk.read_f32(); ++floats_count; } }
            else if (fn == 7  && wt == I32)     { r.read_f32(); ++floats_count; }
            else if (fn == 8  && wt == LEN)     { Reader pk = r.sub(); while (pk.good()) { pk.read_varint(); ++ints_count; } }
            else if (fn == 8  && wt == VARINT)  { r.read_varint(); ++ints_count; }
            else if (fn == 9  && wt == LEN)     { r.read_str(); ++strings_count; }
            else                                { r.skip(wt); }
        }

        std::string val;
        switch (type)
        {
        case 1:  val = std::to_string(f); break;
        case 2:  val = std::to_string(i); break;
        case 3:  val = s; break;
        case 4:  val = "tensor"; break;
        case 5:  val = "graph"; break;
        case 6:  val = std::string("[") + std::to_string(floats_count) + "]"; break;
        case 7:  val = std::string("[") + std::to_string(ints_count)   + "]"; break;
        case 8:  val = std::string("[") + std::to_string(strings_count) + "]"; break;
        default: val = "?"; break;
        }
        return { name, val };
    }
} // namespace pb
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
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs.good()) { return; }
    std::vector<uint8_t> buf(
        (std::istreambuf_iterator<char>(ifs)),
        std::istreambuf_iterator<char>());
    if (buf.empty()) { return; }

    const uint8_t* buf_end = buf.data() + buf.size();
    pb::Reader model{ buf.data(), buf_end };

    // Locate GraphProto (field 7 in ModelProto)
    pb::Reader graph_r{ buf_end, buf_end };
    while (model.good())
    {
        int fn, wt;
        model.read_tag(fn, wt);
        if (fn == 7 && wt == pb::LEN)
            graph_r = model.sub();
        else
            model.skip(wt);
    }
    if (!graph_r.good()) { return; }

    struct InitRaw { std::string name; int dims_size = 0; };
    struct NodeRaw
    {
        std::vector<std::string> inputs, outputs;
        std::string name, op_type;
        std::vector<std::pair<std::string, std::string>> attrs;
    };
    std::unordered_set<std::string> init_names;
    std::vector<InitRaw> initializers;
    std::vector<std::string> graph_inputs, graph_outputs;
    std::vector<NodeRaw> raw_nodes;

    pb::Reader gr = graph_r;
    while (gr.good())
    {
        int fn, wt;
        gr.read_tag(fn, wt);
        if (fn == 1 && wt == pb::LEN)           // NodeProto
        {
            pb::Reader nr = gr.sub();
            NodeRaw raw;
            int attr_idx = 0;
            while (nr.good())
            {
                int nfn, nwt;
                nr.read_tag(nfn, nwt);
                if      (nfn == 1 && nwt == pb::LEN) { raw.inputs.push_back(nr.read_str()); }
                else if (nfn == 2 && nwt == pb::LEN) { raw.outputs.push_back(nr.read_str()); }
                else if (nfn == 3 && nwt == pb::LEN) { raw.name = nr.read_str(); }
                else if (nfn == 4 && nwt == pb::LEN) { raw.op_type = nr.read_str(); }
                else if (nfn == 5 && nwt == pb::LEN)
                {
                    auto [aname, aval] = pb::parse_attr(nr.sub());
                    if (aname.empty())
                    {
                        auto mit = int_to_string_.find(raw.op_type);
                        if (mit != int_to_string_.end())
                        {
                            auto ait = mit->second.find(attr_idx);
                            aname = (ait != mit->second.end()) ? ait->second : std::to_string(attr_idx);
                        }
                        else { aname = std::to_string(attr_idx); }
                    }
                    raw.attrs.emplace_back(std::move(aname), std::move(aval));
                    ++attr_idx;
                }
                else { nr.skip(nwt); }
            }
            raw_nodes.push_back(std::move(raw));
        }
        else if (fn == 5 && wt == pb::LEN)      // TensorProto (initializer)
        {
            pb::Reader tr = gr.sub();
            InitRaw init;
            while (tr.good())
            {
                int tfn, twt;
                tr.read_tag(tfn, twt);
                if      (tfn == 1 && twt == pb::VARINT) { tr.read_varint(); ++init.dims_size; }
                else if (tfn == 1 && twt == pb::LEN)
                {
                    pb::Reader pk = tr.sub();
                    while (pk.good()) { pk.read_varint(); ++init.dims_size; }
                }
                else if (tfn == 8 && twt == pb::LEN) { init.name = tr.read_str(); }
                else { tr.skip(twt); }
            }
            init_names.insert(init.name);
            initializers.push_back(std::move(init));
        }
        else if (fn == 11 && wt == pb::LEN)     // ValueInfoProto (graph input)
        {
            pb::Reader vr = gr.sub();
            while (vr.good())
            {
                int vfn, vwt;
                vr.read_tag(vfn, vwt);
                if (vfn == 1 && vwt == pb::LEN) { graph_inputs.push_back(vr.read_str()); }
                else { vr.skip(vwt); }
            }
        }
        else if (fn == 12 && wt == pb::LEN)     // ValueInfoProto (graph output)
        {
            pb::Reader vr = gr.sub();
            while (vr.good())
            {
                int vfn, vwt;
                vr.read_tag(vfn, vwt);
                if (vfn == 1 && vwt == pb::LEN) { graph_outputs.push_back(vr.read_str()); }
                else { vr.skip(vwt); }
            }
        }
        else { gr.skip(wt); }
    }

    int auto_node_name = 0;
    int auto_blob_name = 0;

    for (const auto& init : initializers)
    {
        Node node;
        node.type = "Initializer";
        node.title = init.name;
        node.out.push_back(init.name);
        node.text = std::string("dims=") + std::to_string(init.dims_size);
        node.nexts.resize(1);
        nodes.push_back(std::move(node));
    }

    for (const auto& inp : graph_inputs)
    {
        if (init_names.count(inp)) { continue; }
        Node node;
        node.type = "Input";
        node.title = inp;
        node.out.push_back(inp);
        node.nexts.resize(1);
        nodes.push_back(std::move(node));
    }

    for (auto& raw : raw_nodes)
    {
        Node node;
        node.type = raw.op_type;
        node.title = raw.name.empty()
            ? (raw.op_type + "_" + std::to_string(auto_node_name++))
            : raw.name;
        for (auto& s : raw.inputs)  { node.in.push_back(sanitize_blob_name(s, auto_blob_name++)); }
        for (auto& s : raw.outputs) { node.out.push_back(sanitize_blob_name(s, auto_blob_name++)); }
        for (auto& [aname, aval] : raw.attrs)
        {
            node.values[aname] = aval;
            if (!node.text.empty()) { node.text += " "; }
            node.text += aname + "=" + aval;
        }
        node.prevs.resize(node.in.size());
        node.nexts.resize(node.out.size());
        nodes.push_back(std::move(node));
    }

    for (const auto& out : graph_outputs)
    {
        Node node;
        node.type = "Output";
        node.title = out;
        node.in.push_back(out);
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
