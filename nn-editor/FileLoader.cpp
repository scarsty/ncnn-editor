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
#ifdef NETEDIT_LOADER_YAML_YOLO
#include "yamlyololoader.h"
#endif
#include "ncnnloader.h"
#ifdef NETEDIT_LOADER_ONNX
#include "onnxloader.h"
#endif
#ifdef NETEDIT_LOADER_TNN
#include "tnnloader.h"
#endif
#ifdef NETEDIT_LOADER_MNN
#include "mnnloader.h"
#endif

namespace
{
constexpr int kDefaultNodeWidth = 200;
constexpr int kNodeSpacing = 96;
constexpr int kLayerSpacing = 20;
constexpr int kComponentSpacing = 240;
constexpr int kTopMargin = 20;
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

// -----------------------------------------------------------------------
// Virtual-node infrastructure for long-edge normalisation
//
// Dagre (used by Netron) inserts dummy nodes at every intermediate rank
// for edges that span more than one layer.  This "normalise" step:
//   1. Lets the crossing-minimisation (order_layers) treat skip-connections
//      correctly, because each layer now only has rank-adjacent edges.
//   2. Lets the x-relaxation route skip connections as smooth paths instead
//      of jumps across many layers.
//
// We apply the same idea in C++: temporary Node objects are spliced into
// the graph before layout and removed afterwards.
// -----------------------------------------------------------------------

struct NormalizeRecord
{
    Node* from;      // original edge source
    Node* to;        // original edge target
    Node* first_vn;  // first virtual node in the inserted chain
};

struct VirtualNodePool
{
    std::vector<std::unique_ptr<Node>> storage;
    std::unordered_set<Node*>          set;

    Node* alloc(int rank)
    {
        auto p   = std::make_unique<Node>();
        Node* ptr = p.get();
        ptr->turn = rank;
        ptr->id   = -1;   // sentinel – identifies virtual nodes
        storage.push_back(std::move(p));
        set.insert(ptr);
        return ptr;
    }

    bool is_virtual(Node* n) const { return set.count(n) > 0; }
};

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
    // Keep layout geometry consistent with runtime rendering width.
    // nn_editor.cpp draws nodes with a fixed width (200), so using variable
    // estimated widths here causes apparent x misalignment in the UI.
    double width = static_cast<double>(kDefaultNodeWidth);

    // Match normal (collapsed) node size. Expanded inspector content is transient and
    // should not drive global layer spacing.
    double height = 72.0;
    height += static_cast<double>((std::max)(node.prev_pin, node.next_pin)) * 18.0;
    height = (std::max)(height, 96.0);

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

    // Push source nodes (no in-component predecessors) down to sit right above
    // their earliest consumer instead of all piling up at rank 0.
    // This prevents weight/constant nodes (e.g. MemoryData) from creating an
    // excessively wide top layer that distorts the whole layout.
    for (Node* node : order)
    {
        bool is_source = true;
        for (Node* prev : node->prevs)
        {
            if (prev != nullptr && component_nodes.count(prev) > 0)
            {
                is_source = false;
                break;
            }
        }
        if (!is_source)
        {
            continue;
        }

        int min_succ_rank = std::numeric_limits<int>::max();
        for (Node* next : node->nexts)
        {
            if (next != nullptr && component_nodes.count(next) > 0)
            {
                auto it = ranks.find(next);
                if (it != ranks.end())
                {
                    min_succ_rank = (std::min)(min_succ_rank, it->second);
                }
            }
        }

        if (min_succ_rank != std::numeric_limits<int>::max() && min_succ_rank - 1 > ranks[node])
        {
            ranks[node] = min_succ_rank - 1;
            node->turn = min_succ_rank - 1;
        }
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

    // Left-to-right compaction: prevent nodes from overlapping
    std::vector<double> compacted = desired;
    for (size_t index = 1; index < compacted.size(); ++index)
    {
        compacted[index] = (std::max)(compacted[index], compacted[index - 1] + pair_spacing(layer[index - 1], layer[index], metrics));
    }
    // Right-to-left compaction: allow nodes pushed right to come back left
    // when space allows, so clusters stay centered on their desired positions.
    for (size_t index = compacted.size() - 1; index > 0; --index)
    {
        compacted[index - 1] = (std::min)(compacted[index - 1], compacted[index] - pair_spacing(layer[index - 1], layer[index], metrics));
    }

    double desired_mean = std::accumulate(desired.begin(), desired.end(), 0.0) / static_cast<double>(desired.size());
    double compacted_mean = std::accumulate(compacted.begin(), compacted.end(), 0.0) / static_cast<double>(compacted.size());
    double shift = desired_mean - compacted_mean;

    for (size_t index = 0; index < layer.size(); ++index)
    {
        positions[layer[index]] = compacted[index] + shift;
    }
}

PositionMap assign_horizontal_positions(const std::vector<std::vector<Node*>>& layers, const NodeMetricsMap& metrics,
                                        const std::unordered_set<Node*>& virtual_nodes = {})
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

    for (int iteration = 0; iteration < 12; ++iteration)
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

    // -----------------------------------------------------------------------
    // Post-relaxation cleanup.
    //
    // Design rules (to prevent the three passes from fighting each other):
    //
    //  1. Fan-out children (direct children of a node with >=2 in-component
    //     nexts) must NOT be repositioned by chain straightening.  Their x is
    //     owned exclusively by the fan-out centering pass below.
    //
    //  2. The snap pass is intentionally omitted: it aligns the nearest child
    //     to the parent while leaving the other child(ren) in place, creating
    //     asymmetry that the centering pass then cannot fix cleanly.
    //
    //  3. Fan-out centering places children with EXACT node spacing (not a
    //     group shift), so no intra-group overlaps are introduced and
    //     resolve_layer_overlaps only needs to handle inter-group collisions.
    // -----------------------------------------------------------------------

    // Helper: is a node reachable inside this position map?
    auto in_positions = [&](Node* n) -> bool
    {
        return n != nullptr && positions.count(n) > 0;
    };

    // Helper: is this node a virtual routing node?
    auto is_vn = [&](Node* n) -> bool
    {
        return virtual_nodes.count(n) > 0;
    };

    // Count in-component REAL adjacents (prevs or nexts).
    // Virtual nodes are excluded so they don't create spurious fan-out branches
    // or break chains that pass through skip-connection routing.
    auto count_ic = [&](Node* n, bool use_prev) -> int
    {
        int cnt = 0;
        const auto& adj = use_prev ? n->prevs : n->nexts;
        for (Node* nb : adj)
        {
            if (in_positions(nb) && !is_vn(nb))
            {
                ++cnt;
            }
        }
        return cnt;
    };

    // Build rank index for direct-child lookup.
    std::unordered_map<Node*, size_t> rank_index;
    for (size_t rank = 0; rank < layers.size(); ++rank)
    {
        for (Node* node : layers[rank])
        {
            rank_index[node] = rank;
        }
    }

    // Identify fan-out children so chain straightening can skip them.
    // Only real (non-virtual) children are counted; virtual routing nodes are
    // skip-connection waypoints and must not trigger spurious fan-out logic.
    std::unordered_set<Node*> fanout_children;
    for (const auto& layer : layers)
    {
        for (Node* node : layer)
        {
            if (is_vn(node)) continue;          // virtual nodes cannot be fan-out parents
            if (count_ic(node, false) >= 2)
            {
                for (Node* next : node->nexts)
                {
                    if (in_positions(next) && !is_vn(next))
                    {
                        fanout_children.insert(next);
                    }
                }
            }
        }
    }

    // --- Pass 1: Chain straightening ---
    // Only applies to pure-chain nodes (1 in-component prev, 1 in-component
    // next) that are NOT fan-out children.  This keeps sequential paths
    // visually straight without disturbing fan-out groups.
    {
        std::unordered_map<Node*, int> chain_id;
        std::vector<std::vector<Node*>> chains;

        for (const auto& layer : layers)
        {
            for (Node* node : layer)
            {
                if (chain_id.count(node))
                {
                    continue;
                }

                // Virtual nodes are registered as singleton chains so they get
                // a chain_id but are never straightened with real nodes.
                if (is_vn(node))
                {
                    chain_id[node] = static_cast<int>(chains.size());
                    chains.push_back({ node });
                    continue;
                }

                // Fan-out children are registered as singleton "chains" so they
                // get a chain_id but are never straightened.
                if (fanout_children.count(node))
                {
                    chain_id[node] = static_cast<int>(chains.size());
                    chains.push_back({ node });
                    continue;
                }

                std::vector<Node*> chain = { node };
                chain_id[node] = static_cast<int>(chains.size());

                Node* cur = node;
                while (true)
                {
                    // Only follow REAL nexts when building chains.
                    if (count_ic(cur, false) != 1)
                    {
                        break;
                    }
                    Node* next = nullptr;
                    for (Node* nb : cur->nexts)
                    {
                        if (in_positions(nb) && !is_vn(nb))
                        {
                            next = nb;
                            break;
                        }
                    }
                    if (next == nullptr || chain_id.count(next))
                    {
                        break;
                    }
                    if (count_ic(next, true) != 1)
                    {
                        break;
                    }
                    // Don't pull fan-out children into a chain.
                    if (fanout_children.count(next))
                    {
                        break;
                    }
                    chain.push_back(next);
                    chain_id[next] = static_cast<int>(chains.size());
                    cur = next;
                }
                chains.push_back(std::move(chain));
            }
        }

        for (const auto& chain : chains)
        {
            // Skip singleton or virtual-node-only chains.
            if (chain.size() < 2 || is_vn(chain[0]))
            {
                continue;
            }
            // Compute the "preferred X" for this chain as the average X of
            // all EXTERNAL neighbours (nodes connected to the chain from outside).
            // Shift the whole chain as a rigid group toward that preferred X.
            // This keeps relative node spacing intact (no intra-layer overlap is
            // introduced) while still pulling sequential paths into alignment.
            double ext_sum = 0.0;
            int    ext_cnt = 0;
            for (Node* n : chain)
            {
                for (Node* nb : n->prevs)
                {
                    if (in_positions(nb) && !chain_id.count(nb))
                    { ext_sum += positions.at(nb); ++ext_cnt; }
                }
                for (Node* nb : n->nexts)
                {
                    if (in_positions(nb) && !chain_id.count(nb))
                    { ext_sum += positions.at(nb); ++ext_cnt; }
                }
            }
            if (ext_cnt > 0)
            {
                double chain_cx = 0.0;
                for (Node* n : chain) chain_cx += positions.at(n);
                chain_cx /= static_cast<double>(chain.size());
                double shift = (ext_sum / static_cast<double>(ext_cnt)) - chain_cx;
                for (Node* n : chain) positions[n] += shift;
            }
        }
    }

    // --- Overlap resolver (used after centering) ---
    auto resolve_layer_overlaps = [&](const std::vector<Node*>& layer)
    {
        if (layer.size() < 2)
        {
            return;
        }
        std::vector<Node*> sorted = layer;
        std::sort(sorted.begin(), sorted.end(), [&](Node* a, Node* b)
        {
            return positions.at(a) < positions.at(b);
        });
        for (size_t i = 1; i < sorted.size(); ++i)
        {
            double min_x = positions[sorted[i - 1]] + pair_spacing(sorted[i - 1], sorted[i], metrics);
            if (positions[sorted[i]] < min_x)
            {
                positions[sorted[i]] = min_x;
            }
        }
        for (size_t i = sorted.size() - 1; i > 0; --i)
        {
            double max_x = positions[sorted[i]] - pair_spacing(sorted[i - 1], sorted[i], metrics);
            if (positions[sorted[i - 1]] > max_x)
            {
                positions[sorted[i - 1]] = max_x;
            }
        }
    };

    // --- Pass 2: Fan-out centering ---
    // For each parent with >=2 direct REAL children in the next rank, place the
    // children with exact node spacing, symmetrically around the parent's x.
    // Virtual routing nodes are excluded: they are waypoints for skip connections,
    // not real fan-out branches, so they must not shift real node positions.
    for (size_t rank = 0; rank + 1 < layers.size(); ++rank)
    {
        for (Node* parent : layers[rank])
        {
            if (is_vn(parent)) continue;   // virtual nodes are never fan-out parents

            // Collect deduplicated direct REAL children in the immediate next rank.
            std::vector<Node*> children;
            for (Node* next : parent->nexts)
            {
                if (!in_positions(next) || is_vn(next))
                {
                    continue;
                }
                auto it = rank_index.find(next);
                if (it == rank_index.end() || it->second != rank + 1)
                {
                    continue;
                }
                if (std::find(children.begin(), children.end(), next) == children.end())
                {
                    children.push_back(next);
                }
            }

            if (children.size() < 2)
            {
                continue;
            }

            // Keep children in a stable left-to-right order.
            std::sort(children.begin(), children.end(), [&](Node* a, Node* b)
            {
                return positions.at(a) < positions.at(b);
            });

            // Compute total group width (node widths + spacing between them).
            double total = 0.0;
            for (size_t i = 0; i < children.size(); ++i)
            {
                total += 2.0 * half_width(children[i], metrics);
                if (i + 1 < children.size())
                {
                    total += static_cast<double>(kNodeSpacing);
                }
            }

            // Place children symmetrically centered at the parent's x.
            double cursor = positions.at(parent) - total * 0.5 + half_width(children[0], metrics);
            for (size_t i = 0; i < children.size(); ++i)
            {
                positions[children[i]] = cursor;
                if (i + 1 < children.size())
                {
                    cursor += half_width(children[i], metrics)
                              + half_width(children[i + 1], metrics)
                              + static_cast<double>(kNodeSpacing);
                }
            }
        }

        // Fix any inter-group overlaps in the child layer.
        resolve_layer_overlaps(layers[rank + 1]);
    }

    // --- Final pass: global overlap resolution ---
    // Chain-straightening (Pass 1) may have introduced new overlaps in layers
    // that are not direct children of any fan-out parent.  This final sweep
    // guarantees no real-node overlaps remain after all post-processing.
    for (const auto& layer : layers)
    {
        // Build a real-nodes-only sub-layer for overlap resolution.
        // Virtual nodes have zero width and should not block real nodes.
        std::vector<Node*> real_layer;
        real_layer.reserve(layer.size());
        for (Node* n : layer)
        {
            if (!is_vn(n)) real_layer.push_back(n);
        }
        resolve_layer_overlaps(real_layer);
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
        double layer_height = 96.0;
        for (Node* node : layers[rank])
        {
            layer_height = (std::max)(layer_height, metrics.at(node).height);
        }
        cursor += layer_height + static_cast<double>(kLayerSpacing);
    }
    return tops;
}

// -----------------------------------------------------------------------
// normalize_long_edges
//
// For every edge (from → to) whose rank span is > 1, insert a chain of
// virtual nodes at the intermediate ranks and replace the long edge with
// a sequence of unit-length edges.  The real graph is temporarily mutated;
// call denormalize_long_edges to restore it after layout.
// -----------------------------------------------------------------------
void normalize_long_edges(
    std::vector<Node*>&              component,
    RankMap&                         ranks,
    std::vector<std::vector<Node*>>& layers,
    NodeMetricsMap&                  metrics,
    VirtualNodePool&                 pool,
    std::vector<NormalizeRecord>&    records)
{
    // Snapshot current long edges (iterate a copy to avoid invalidation).
    std::vector<std::pair<Node*, Node*>> long_edges;
    {
        std::unordered_set<Node*> comp_set(component.begin(), component.end());
        for (Node* n : component)
        {
            for (Node* nx : n->nexts)
            {
                if (nx == nullptr || !comp_set.count(nx))
                {
                    continue;
                }
                if (ranks.count(nx) && ranks.at(nx) - ranks.at(n) > 1)
                {
                    long_edges.emplace_back(n, nx);
                }
            }
        }
    }

    for (auto& [from, to] : long_edges)
    {
        int r_from = ranks.at(from);
        int r_to   = ranks.at(to);

        // Sever the original long edge.
        {
            auto& fn = from->nexts;
            auto  it = std::find(fn.begin(), fn.end(), to);
            if (it != fn.end()) fn.erase(it);
        }
        {
            auto& tp = to->prevs;
            auto  it = std::find(tp.begin(), tp.end(), from);
            if (it != tp.end()) tp.erase(it);
        }

        // Insert a chain: from → vn(r_from+1) → … → vn(r_to-1) → to
        Node* prev     = from;
        Node* first_vn = nullptr;
        for (int r = r_from + 1; r < r_to; ++r)
        {
            Node* vn = pool.alloc(r);
            ranks[vn]  = r;

            prev->nexts.push_back(vn);
            vn->prevs.push_back(prev);

            component.push_back(vn);
            if (r < static_cast<int>(layers.size()))
            {
                layers[static_cast<size_t>(r)].push_back(vn);
            }

            // Virtual nodes occupy zero width / height so they act as
            // routing guides without displacing real nodes.
            metrics[vn] = { 0.0, 0.0 };

            if (!first_vn) first_vn = vn;
            prev = vn;
        }

        // Connect the last virtual node (or from) to the target.
        prev->nexts.push_back(to);
        to->prevs.push_back(prev);

        records.push_back({ from, to, first_vn });
    }
}

// -----------------------------------------------------------------------
// denormalize_long_edges
//
// Remove all virtual nodes and restore the original long edges.
// Must be called after the position assignment is complete.
// -----------------------------------------------------------------------
void denormalize_long_edges(
    std::vector<Node*>&              component,
    std::vector<std::vector<Node*>>& layers,
    RankMap&                         ranks,
    NodeMetricsMap&                  metrics,
    const VirtualNodePool&           pool,
    const std::vector<NormalizeRecord>& records)
{
    for (const auto& rec : records)
    {
        Node* from     = rec.from;
        Node* to       = rec.to;
        Node* first_vn = rec.first_vn;
        if (!first_vn) continue;

        // Disconnect from → first_vn.
        {
            auto& fn = from->nexts;
            auto  it = std::find(fn.begin(), fn.end(), first_vn);
            if (it != fn.end()) fn.erase(it);
        }

        // Walk the virtual chain to find the last vn (its next is 'to').
        Node* last_vn = first_vn;
        while (true)
        {
            Node* candidate = nullptr;
            for (Node* nx : last_vn->nexts)
            {
                if (pool.is_virtual(nx)) { candidate = nx; break; }
            }
            if (!candidate) break;
            last_vn = candidate;
        }

        // Disconnect last_vn → to.
        {
            auto& ln = last_vn->nexts;
            auto  it = std::find(ln.begin(), ln.end(), to);
            if (it != ln.end()) ln.erase(it);
        }
        {
            auto& tp = to->prevs;
            // Remove whichever virtual node points to 'to'.
            auto it = std::find_if(tp.begin(), tp.end(),
                [&](Node* n){ return pool.is_virtual(n); });
            if (it != tp.end()) tp.erase(it);
        }

        // Restore the original edge.
        from->nexts.push_back(to);
        to->prevs.push_back(from);
    }

    // Strip virtual nodes from every container.
    auto is_vn = [&](Node* n){ return pool.is_virtual(n); };
    component.erase(std::remove_if(component.begin(), component.end(), is_vn),
                    component.end());
    for (auto& layer : layers)
    {
        layer.erase(std::remove_if(layer.begin(), layer.end(), is_vn),
                    layer.end());
    }
    for (Node* vn : pool.set)
    {
        ranks.erase(vn);
        metrics.erase(vn);
    }
}

} // anonymous namespace

FileLoader* create_loader(const std::string& filename, int index)
{
    (void)index;
    auto ext = strfunc::toLowerCase(filefunc::getFileExt(filename));
    if (ext == "ini")
    {
        return new ccccLoader();
    }
    if (ext == "yaml")
    {
#ifdef NETEDIT_LOADER_YAML_YOLO
        return new yamlyoloLoader();
#else
        return new FileLoader();
#endif
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

#ifdef NETEDIT_LOADER_ONNX
    if (ext == "onnx")
    {
        return new onnxLoader();
    }
#endif

#ifdef NETEDIT_LOADER_TNN
    if (ext == "tnnproto" || ext == "tnnmodel")
    {
        return new tnnLoader();
    }
#endif

#ifdef NETEDIT_LOADER_MNN
    const std::string lower_name = strfunc::toLowerCase(filename);
    if (ext == "mnn" || (lower_name.size() >= 9 && lower_name.substr(lower_name.size() - 9) == ".mnn.json"))
    {
        return new mnnLoader();
    }
#endif

    return new FileLoader();
}

const char* file_filter()
{
#ifdef _WIN32
        return "Supported files\0*.ini;*.yaml;*.param;*.onnx;*.tnnproto;*.tnnmodel;*.mnn;*.mnn.json\0"
           "CCCC Example\0*.ini\0"
           "yolort\0*.yaml\0"
           "ncnn & pnnx\0*.param\0"
           "ONNX\0*.onnx\0"
           "TNN\0*.tnnproto;*.tnnmodel\0"
            "MNN\0*.mnn;*.mnn.json\0"
           "All files\0*.*\0";
#endif
    return nullptr;
}

std::vector<std::string> FileLoader::metadataCandidates(const std::string& metadata_filename)
{
#ifdef __EMSCRIPTEN__
    return { "/" + metadata_filename, metadata_filename, "./" + metadata_filename };
#else
#ifdef __APPLE__
    return {
        FileLoader::mainPath() + "/../Resources/" + metadata_filename,
        FileLoader::mainPath() + "/" + metadata_filename,
        metadata_filename
    };
#else
    return {
        FileLoader::mainPath() + "/" + metadata_filename,
        FileLoader::mainPath() + "/../" + metadata_filename,
        FileLoader::mainPath() + "/../../" + metadata_filename,
        FileLoader::mainPath() + "/../nn-editor/" + metadata_filename,
        FileLoader::mainPath() + "/../../nn-editor/" + metadata_filename,
        metadata_filename,
        "./" + metadata_filename,
        "nn-editor/" + metadata_filename,
        "./nn-editor/" + metadata_filename
    };
#endif
#endif
}

void FileLoader::calPosition(std::deque<Node>& nodes)
{
    if (nodes.empty())
    {
        return;
    }

    // ------------------------------------------------------------------
    // Rebuild nexts from prevs.
    //
    // Some loaders (e.g. the lightweight ONNX web loader) populate
    // nexts via index-based assignment:  src.nexts[i_out] = &dst
    // When one output blob is consumed by multiple downstream nodes, only
    // the LAST assignment survives — earlier consumers are silently lost.
    //
    // prevs are always fully populated (dst.prevs[i_in] = &src never
    // overwrites a different predecessor).  Rebuilding nexts from prevs
    // gives the layout algorithm the complete fan-out graph, fixing:
    //   1. build_topological_order – correct indegrees → correct order
    //   2. assign_ranks            – correct predecessor ranks
    //   3. order_layers / relax    – correct neighbour references
    // ------------------------------------------------------------------
    for (auto& n : nodes)
    {
        n.nexts.clear();
    }
    for (auto& n : nodes)
    {
        for (Node* prev : n.prevs)
        {
            if (prev == nullptr)
            {
                continue;
            }
            auto& pn = prev->nexts;
            if (std::find(pn.begin(), pn.end(), &n) == pn.end())
            {
                pn.push_back(&n);
            }
        }
    }

    // ------------------------------------------------------------------
    // Isolate "parameter" source nodes (ONNX Initializers: weights, biases,
    // BatchNorm statistics, etc.) from the main layout graph.
    //
    // These nodes have NO predecessors and their sole role is to feed
    // parameters into computation nodes.  A typical medium network has
    // 100–300 such nodes.  Including them in rank assignment causes layers
    // to contain dozens of Initializers alongside a single Conv/BN node;
    // the overlap resolver then spreads that layer to tens-of-thousands of
    // pixels, producing the characteristic "long line to the right" artefact.
    //
    // Instead we detach them before layout, run layout on computation nodes
    // only, then re-attach and snap each Initializer to its first consumer.
    // ------------------------------------------------------------------
    std::unordered_map<Node*, std::vector<Node*>> param_consumers;
    for (auto& n : nodes)
    {
        if (n.type == "Initializer" && n.prevs.empty())
        {
            param_consumers[&n] = n.nexts;
        }
    }
    for (auto& [p, consumers] : param_consumers)
    {
        for (Node* c : consumers)
        {
            auto& pv = c->prevs;
            pv.erase(std::remove(pv.begin(), pv.end(), p), pv.end());
        }
        p->nexts.clear();
    }

    double component_offset = static_cast<double>(kLeftMargin);
    for (auto& component : collect_components(nodes))
    {
        // ------------------------------------------------------------------
        // Skip isolated parameter (Initializer) singleton components.
        // After detachment they have no edges, so they form trivial
        // size-1 components.  Processing them here would advance
        // component_offset by hundreds of pixels per initializer, placing
        // the main computation graph thousands of pixels to the right.
        // We position them manually after the computation layout.
        // ------------------------------------------------------------------
        if (component.size() == 1 && param_consumers.count(component[0]) > 0)
        {
            continue;
        }

        std::vector<Node*> order  = build_topological_order(component);
        NodeMetricsMap     metrics = build_metrics(component);
        RankMap            ranks  = assign_ranks(component, order);
        std::vector<std::vector<Node*>> layers = build_layers(order, ranks);

        // ------------------------------------------------------------------
        // Normalise long-span edges by inserting virtual routing nodes.
        // This mirrors Netron/dagre's "normalize" step: every edge that skips
        // more than one rank is broken into a chain of unit-length edges
        // through zero-width virtual nodes.  The virtual nodes participate in
        // crossing minimisation and x-relaxation, then are removed before
        // positions are written back to real Node objects.
        // ------------------------------------------------------------------
        VirtualNodePool             vn_pool;
        std::vector<NormalizeRecord> vn_records;
        normalize_long_edges(component, ranks, layers, metrics, vn_pool, vn_records);

        order_layers(layers);
        PositionMap positions = assign_horizontal_positions(layers, metrics, vn_pool.set);

        // Remove virtual nodes; restore original prevs/nexts.
        denormalize_long_edges(component, layers, ranks, metrics, vn_pool, vn_records);

        std::vector<double> layer_tops = build_layer_tops(layers, metrics);

        double min_x    = component_min_x(component, positions, metrics);
        double offset_x = component_offset - min_x;
        for (Node* node : component)
        {
            int rank = ranks.at(node);
            node->position_x = static_cast<int>(std::lround(positions.at(node) + offset_x - half_width(node, metrics)));
            node->position_y = static_cast<int>(std::lround(layer_tops[static_cast<size_t>(rank)]));
        }

        component_offset += component_width(component, positions, metrics) + static_cast<double>(kComponentSpacing);
    }

    // ------------------------------------------------------------------
    // Re-attach param nodes and place them above their consumers.
    //
    // Each computation node (Conv, BN, etc.) may have 1-3 Initializer
    // inputs (weight, bias, running_mean, …).  Instead of stacking them
    // on top of the computation node (which causes garbled overlap), we
    // spread them in a horizontal row 200 px ABOVE the consumer.
    // That keeps the computation graph clean and the parameter nodes
    // visible above it.
    // ------------------------------------------------------------------

    // Restore the graph edges (needed for correct link drawing).
    for (auto& [p, consumers] : param_consumers)
    {
        p->nexts = consumers;
        for (Node* c : consumers)
        {
            c->prevs.push_back(p);
        }
    }

    // Group initializers by their primary consumer.
    std::unordered_map<Node*, std::vector<Node*>> consumer_to_inits;
    for (auto& [p, consumers] : param_consumers)
    {
        if (!consumers.empty() && consumers[0]->position_x != -1)
        {
            consumer_to_inits[consumers[0]].push_back(p);
        }
    }

    // Place each group horizontally centred above the consumer.
    constexpr int kInitYOffset = 200;   // pixels above consumer
    constexpr int kInitXStep   = kDefaultNodeWidth + kNodeSpacing / 2;
    for (auto& [consumer, inits] : consumer_to_inits)
    {
        int N = static_cast<int>(inits.size());
        double group_half = (N - 1) * 0.5 * kInitXStep;
        for (int k = 0; k < N; ++k)
        {
            inits[k]->position_x = static_cast<int>(std::lround(
                consumer->position_x - group_half + k * kInitXStep));
            inits[k]->position_y = consumer->position_y - kInitYOffset;
        }
    }

    // Orphaned initializers (no valid consumer): place far above the canvas.
    for (auto& [p, consumers] : param_consumers)
    {
        if (p->position_x == -1)
        {
            p->position_x = static_cast<int>(component_offset);
            p->position_y = kTopMargin;
            component_offset += kDefaultNodeWidth + kComponentSpacing;
        }
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

