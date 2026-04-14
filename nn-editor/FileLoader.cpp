#include "FileLoader.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <numeric>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include "filefunc.h"
#include "strfunc.h"

#include "ccccloader.h"
#include "yamlyololoader.h"
#include "ncnnloader.h"

namespace
{
constexpr int kDefaultNodeWidth = 220;
constexpr int kMinNodeWidth = 200;
constexpr int kMaxNodeWidth = 420;
constexpr int kNodeSpacing = 96;
constexpr int kLayerSpacing = 84;
constexpr int kComponentSpacing = 240;
constexpr int kTopMargin = 180;
constexpr int kLeftMargin = 220;

using NodeSet = std::unordered_set<Node*>;
using RankMap = std::unordered_map<Node*, int>;
using PositionMap = std::unordered_map<Node*, double>;

struct NodeMetrics
{
    double width = kDefaultNodeWidth;
    double height = 120.0;
};

using NodeMetricsMap = std::unordered_map<Node*, NodeMetrics>;

double clamp_double(double value, double min_value, double max_value)
{
    return (std::max)(min_value, (std::min)(value, max_value));
}

size_t estimate_line_count(const std::string& text)
{
    if (text.empty())
    {
        return 0;
    }

    size_t lines = 1;
    for (char ch : text)
    {
        if (ch == '\n')
        {
            lines++;
        }
    }
    return lines;
}

double estimate_text_width(const std::string& text)
{
    if (text.empty())
    {
        return 0.0;
    }

    size_t longest = 0;
    size_t current = 0;
    for (char ch : text)
    {
        if (ch == '\n')
        {
            longest = (std::max)(longest, current);
            current = 0;
            continue;
        }
        current++;
    }
    longest = (std::max)(longest, current);
    return static_cast<double>(longest) * 7.5;
}

NodeMetrics estimate_node_metrics(const Node& node)
{
    double width = static_cast<double>(kDefaultNodeWidth);
    width = (std::max)(width, estimate_text_width(node.title) + 48.0);
    width = (std::max)(width, estimate_text_width(node.type) + 92.0);
    width = (std::max)(width, estimate_text_width(node.text) + 36.0);

    for (const auto& kv : node.values)
    {
        width = (std::max)(width, estimate_text_width(kv.first) + estimate_text_width(kv.second) + 80.0);
    }

    width += static_cast<double>((std::max)(node.prev_pin, node.next_pin)) * 6.0;
    width = clamp_double(width, kMinNodeWidth, kMaxNodeWidth);

    double height = 72.0;
    height += static_cast<double>((std::max)(node.prev_pin, node.next_pin)) * 18.0;
    if (!node.values.empty())
    {
        height += static_cast<double>(node.values.size()) * 24.0;
    }
    height += static_cast<double>(estimate_line_count(node.text)) * 22.0;
    height = (std::max)(height, 110.0);

    return { width, height };
}

NodeMetricsMap build_metrics(const std::vector<Node*>& nodes)
{
    NodeMetricsMap metrics;
    for (Node* node : nodes)
    {
        metrics[node] = estimate_node_metrics(*node);
    }
    return metrics;
}

double half_width(Node* node, const NodeMetricsMap& metrics)
{
    return metrics.at(node).width * 0.5;
}

double half_height(Node* node, const NodeMetricsMap& metrics)
{
    return metrics.at(node).height * 0.5;
}

double pair_spacing(Node* left, Node* right, const NodeMetricsMap& metrics)
{
    return half_width(left, metrics) + half_width(right, metrics) + static_cast<double>(kNodeSpacing);
}

std::vector<std::vector<Node*>> collect_components(std::deque<Node>& nodes)
{
    std::vector<std::vector<Node*>> components;
    NodeSet visited;

    for (auto& node : nodes)
    {
        if (visited.count(&node) > 0)
        {
            continue;
        }

        std::vector<Node*> component;
        std::queue<Node*> queue;
        queue.push(&node);
        visited.insert(&node);

        while (!queue.empty())
        {
            Node* current = queue.front();
            queue.pop();
            component.push_back(current);

            for (Node* next : current->nexts)
            {
                if (next != nullptr && visited.insert(next).second)
                {
                    queue.push(next);
                }
            }
            for (Node* prev : current->prevs)
            {
                if (prev != nullptr && visited.insert(prev).second)
                {
                    queue.push(prev);
                }
            }
        }

        components.push_back(std::move(component));
    }

    return components;
}

NodeSet make_node_set(const std::vector<Node*>& nodes)
{
    return NodeSet(nodes.begin(), nodes.end());
}

std::vector<Node*> build_topological_order(const std::vector<Node*>& component)
{
    NodeSet component_nodes = make_node_set(component);
    std::unordered_map<Node*, int> indegree;
    std::unordered_map<Node*, int> original_index;
    std::vector<Node*> ready;
    std::vector<Node*> order;

    for (int index = 0; index < static_cast<int>(component.size()); ++index)
    {
        Node* node = component[index];
        original_index[node] = index;
        indegree[node] = 0;
    }

    for (Node* node : component)
    {
        for (Node* next : node->nexts)
        {
            if (next != nullptr && component_nodes.count(next) > 0)
            {
                indegree[next]++;
            }
        }
    }

    for (Node* node : component)
    {
        if (indegree[node] == 0)
        {
            ready.push_back(node);
        }
    }

    std::stable_sort(ready.begin(), ready.end(), [&original_index](Node* lhs, Node* rhs)
    {
        return original_index[lhs] < original_index[rhs];
    });

    while (!ready.empty())
    {
        Node* node = ready.front();
        ready.erase(ready.begin());
        order.push_back(node);

        for (Node* next : node->nexts)
        {
            if (next == nullptr || component_nodes.count(next) == 0)
            {
                continue;
            }

            indegree[next]--;
            if (indegree[next] == 0)
            {
                ready.push_back(next);
                std::stable_sort(ready.begin(), ready.end(), [&original_index](Node* lhs, Node* rhs)
                {
                    return original_index[lhs] < original_index[rhs];
                });
            }
        }
    }

    if (order.size() == component.size())
    {
        return order;
    }

    for (Node* node : component)
    {
        if (std::find(order.begin(), order.end(), node) == order.end())
        {
            order.push_back(node);
        }
    }

    return order;
}

RankMap assign_ranks(const std::vector<Node*>& component, const std::vector<Node*>& order)
{
    NodeSet component_nodes = make_node_set(component);
    RankMap ranks;

    for (Node* node : order)
    {
        int rank = 0;
        for (Node* prev : node->prevs)
        {
            if (prev != nullptr && component_nodes.count(prev) > 0)
            {
                rank = (std::max)(rank, ranks[prev] + 1);
            }
        }
        ranks[node] = rank;
        node->turn = rank;
    }

    return ranks;
}

std::vector<std::vector<Node*>> build_layers(const std::vector<Node*>& order, const RankMap& ranks)
{
    int max_rank = 0;
    for (Node* node : order)
    {
        max_rank = (std::max)(max_rank, ranks.at(node));
    }

    std::vector<std::vector<Node*>> layers(static_cast<size_t>(max_rank + 1));
    for (Node* node : order)
    {
        layers[static_cast<size_t>(ranks.at(node))].push_back(node);
    }
    return layers;
}

std::unordered_map<Node*, size_t> make_order_index(const std::vector<Node*>& layer)
{
    std::unordered_map<Node*, size_t> index;
    for (size_t i = 0; i < layer.size(); ++i)
    {
        index[layer[i]] = i;
    }
    return index;
}

double barycenter(Node* node, const std::unordered_map<Node*, size_t>& adjacent_order, bool use_prevs, bool& has_value)
{
    const auto& adjacent = use_prevs ? node->prevs : node->nexts;
    double sum = 0.0;
    size_t count = 0;

    for (Node* neighbor : adjacent)
    {
        auto it = adjacent_order.find(neighbor);
        if (neighbor != nullptr && it != adjacent_order.end())
        {
            sum += static_cast<double>(it->second);
            count++;
        }
    }

    has_value = count > 0;
    if (!has_value)
    {
        return 0.0;
    }
    return sum / static_cast<double>(count);
}

void reorder_layer(std::vector<Node*>& layer, const std::unordered_map<Node*, size_t>& adjacent_order, bool use_prevs)
{
    struct RankedNode
    {
        Node* node;
        double center;
        size_t original;
        bool has_center;
    };

    std::vector<RankedNode> ranked;
    ranked.reserve(layer.size());
    for (size_t index = 0; index < layer.size(); ++index)
    {
        bool has_center = false;
        ranked.push_back({ layer[index], barycenter(layer[index], adjacent_order, use_prevs, has_center), index, has_center });
    }

    std::stable_sort(ranked.begin(), ranked.end(), [](const RankedNode& lhs, const RankedNode& rhs)
    {
        if (lhs.has_center != rhs.has_center)
        {
            return lhs.has_center > rhs.has_center;
        }
        if (lhs.has_center && rhs.has_center && lhs.center != rhs.center)
        {
            return lhs.center < rhs.center;
        }
        return lhs.original < rhs.original;
    });

    for (size_t index = 0; index < ranked.size(); ++index)
    {
        layer[index] = ranked[index].node;
    }
}

void order_layers(std::vector<std::vector<Node*>>& layers)
{
    if (layers.size() < 2)
    {
        return;
    }

    for (int iteration = 0; iteration < 6; ++iteration)
    {
        for (size_t rank = 1; rank < layers.size(); ++rank)
        {
            reorder_layer(layers[rank], make_order_index(layers[rank - 1]), true);
        }
        for (size_t rank = layers.size() - 1; rank > 0; --rank)
        {
            reorder_layer(layers[rank - 1], make_order_index(layers[rank]), false);
        }
    }
}

double preferred_x(Node* node, const PositionMap& positions, bool use_prevs)
{
    const auto& adjacent = use_prevs ? node->prevs : node->nexts;
    double sum = 0.0;
    int count = 0;

    for (Node* neighbor : adjacent)
    {
        auto it = positions.find(neighbor);
        if (neighbor != nullptr && it != positions.end())
        {
            sum += it->second;
            count++;
        }
    }

    if (count == 0)
    {
        auto it = positions.find(node);
        return it == positions.end() ? 0.0 : it->second;
    }
    return sum / static_cast<double>(count);
}

void relax_layer(const std::vector<Node*>& layer, PositionMap& positions, const NodeMetricsMap& metrics, bool use_prevs)
{
    if (layer.empty())
    {
        return;
    }

    std::vector<double> desired(layer.size(), 0.0);
    for (size_t index = 0; index < layer.size(); ++index)
    {
        desired[index] = preferred_x(layer[index], positions, use_prevs);
    }

    std::vector<double> compacted = desired;
    for (size_t index = 1; index < compacted.size(); ++index)
    {
        compacted[index] = (std::max)(compacted[index], compacted[index - 1] + pair_spacing(layer[index - 1], layer[index], metrics));
    }

    double desired_mean = std::accumulate(desired.begin(), desired.end(), 0.0) / static_cast<double>(desired.size());
    double compacted_mean = std::accumulate(compacted.begin(), compacted.end(), 0.0) / static_cast<double>(compacted.size());
    double shift = desired_mean - compacted_mean;

    for (size_t index = 0; index < layer.size(); ++index)
    {
        positions[layer[index]] = compacted[index] + shift;
    }
}

PositionMap assign_horizontal_positions(const std::vector<std::vector<Node*>>& layers, const NodeMetricsMap& metrics)
{
    PositionMap positions;

    for (const auto& layer : layers)
    {
        if (layer.empty())
        {
            continue;
        }

        double total_width = 0.0;
        for (size_t index = 0; index < layer.size(); ++index)
        {
            total_width += metrics.at(layer[index]).width;
            if (index + 1 < layer.size())
            {
                total_width += static_cast<double>(kNodeSpacing);
            }
        }

        double cursor = -0.5 * total_width;
        for (Node* node : layer)
        {
            cursor += half_width(node, metrics);
            positions[node] = cursor;
            cursor += half_width(node, metrics) + static_cast<double>(kNodeSpacing);
        }
    }

    for (int iteration = 0; iteration < 6; ++iteration)
    {
        for (size_t rank = 1; rank < layers.size(); ++rank)
        {
            relax_layer(layers[rank], positions, metrics, true);
        }
        for (size_t rank = layers.size(); rank-- > 1;)
        {
            relax_layer(layers[rank - 1], positions, metrics, false);
        }
    }

    return positions;
}

double component_width(const std::vector<Node*>& component, const PositionMap& positions, const NodeMetricsMap& metrics)
{
    double min_x = std::numeric_limits<double>::max();
    double max_x = std::numeric_limits<double>::lowest();

    for (Node* node : component)
    {
        double center_x = positions.at(node);
        min_x = (std::min)(min_x, center_x - half_width(node, metrics));
        max_x = (std::max)(max_x, center_x + half_width(node, metrics));
    }

    return max_x - min_x;
}

double component_min_x(const std::vector<Node*>& component, const PositionMap& positions, const NodeMetricsMap& metrics)
{
    double min_x = std::numeric_limits<double>::max();
    for (Node* node : component)
    {
        min_x = (std::min)(min_x, positions.at(node) - half_width(node, metrics));
    }
    return min_x;
}

std::vector<double> build_layer_tops(const std::vector<std::vector<Node*>>& layers, const NodeMetricsMap& metrics)
{
    std::vector<double> tops(layers.size(), static_cast<double>(kTopMargin));
    double cursor = static_cast<double>(kTopMargin);
    for (size_t rank = 0; rank < layers.size(); ++rank)
    {
        tops[rank] = cursor;
        double layer_height = 110.0;
        for (Node* node : layers[rank])
        {
            layer_height = (std::max)(layer_height, metrics.at(node).height);
        }
        cursor += layer_height + static_cast<double>(kLayerSpacing);
    }
    return tops;
}
}

FileLoader* create_loader(const std::string& filename, int index)
{
    if (index > 0)
    {
        switch (index)
        {
        case 1:
            return new ccccLoader();
        case 2:
            return new yamlyoloLoader();
        case 3:
            return new ncnnLoader();
        default:
            break;
        }
    }
    auto ext = strfunc::toLowerCase(filefunc::getFileExt(filename));
    if (ext == "ini")
    {
        return new ccccLoader();
    }
    if (ext == "yaml")
    {
        return new yamlyoloLoader();
    }
    if (ext == "param")
    {
        auto str = filefunc::readFileToString(filename);
        int a = atoi(strfunc::findANumber(str).c_str());
        if (a == 7767517)
        {
            return new ncnnLoader();
        }
    }
    return new FileLoader();
}

const char* file_filter()
{
#ifdef _WIN32
    return "CCCC Example\0*.ini\0yolort\0*.yaml\0ncnn & pnnx\0*.param\0";
#endif
    return nullptr;
}

void FileLoader::calPosition(std::deque<Node>& nodes)
{
    if (nodes.empty())
    {
        return;
    }

    double component_offset = static_cast<double>(kLeftMargin);
    for (auto& component : collect_components(nodes))
    {
        std::vector<Node*> order = build_topological_order(component);
        NodeMetricsMap metrics = build_metrics(component);
        RankMap ranks = assign_ranks(component, order);
        std::vector<std::vector<Node*>> layers = build_layers(order, ranks);
        order_layers(layers);
        PositionMap positions = assign_horizontal_positions(layers, metrics);
        std::vector<double> layer_tops = build_layer_tops(layers, metrics);

        double min_x = component_min_x(component, positions, metrics);
        double offset_x = component_offset - min_x;
        for (Node* node : component)
        {
            int rank = ranks.at(node);
            node->position_x = static_cast<int>(std::lround(positions.at(node) + offset_x - half_width(node, metrics)));
            node->position_y = static_cast<int>(std::lround(layer_tops[static_cast<size_t>(rank)]));
        }

        component_offset += component_width(component, positions, metrics) + static_cast<double>(kComponentSpacing);
    }
}

//最后一个参数为假，仅计算是否存在连接，为真则是严格计算传导顺序
void FileLoader::push_cal_stack(Node* layer, int direct, std::vector<Node*>& stack, bool turn)
{
    //lambda函数：层是否已经在向量中
    auto contains = [](std::vector<Node*>& v, Node* l) -> bool
    {
        return std::find(v.begin(), v.end(), l) != v.end();
    };

    //层连接不能回环
    if (layer == nullptr || contains(stack, layer))
    {
        return;
    }
    std::vector<Node*> connect0, connect1;
    connect1 = layer->nexts;
    connect0 = layer->prevs;

    if (direct < 0)
    {
        std::swap(connect0, connect1);
    }
    //前面的层都被压入，才压入本层
    bool contain_all0 = true;
    for (auto& l : connect0)
    {
        if (!contains(stack, l))
        {
            contain_all0 = false;
            break;
        }
    }
    if (!turn || (!contains(stack, layer) && contain_all0))
    {
        stack.push_back(layer);
    }
    else
    {
        return;
    }
    for (auto& l : connect1)
    {
        push_cal_stack(l, direct, stack, turn);
    }


}

