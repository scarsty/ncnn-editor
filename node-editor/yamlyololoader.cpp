#include "yamlyololoader.h"
#include "yaml-cpp/yaml.h"
#include "fmt1.h"

struct Layer
{
    std::vector <int> from;
    int repeat;
    std::string type;
    std::vector<std::string> paras;
};

static Node yaml2l(YAML::Node& y)
{
    Node n;
    if (y[0].IsScalar())
    {
        n.in.push_back(y[0].as<std::string>());
    }
    else
    {
        n.in = y[0].as< std::vector<std::string>>();
    }
    n.values["repeat"] = y[1].as<std::string>();
    n.type = y[2].as<std::string>();
    n.text = fmt1::format("{}", y[3].as<std::vector<std::string>>());
    n.text = n.text.substr(1);
    n.text.pop_back();
    return n;
}

void yamlyoloLoader::fileToNodes(const std::string& filename, std::deque<Node>& nodes)
{
    YAML::Node config;
    config = YAML::LoadFile(filename);
    auto& back = config["backbone"];
    for (size_t i = 0; i < back.size(); i++)
    {
        auto n = yaml2l(back[i]);
        n.title = "back_" + std::to_string(i);
        nodes.emplace_back(std::move(n));
    }
    auto& head = config["head"];
    for (size_t i = 0; i < head.size(); i++)
    {
        auto n = yaml2l(head[i]);
        n.title = "head_" + std::to_string(i);
        nodes.emplace_back(std::move(n));
    }

    for (int i = 0; i < nodes.size(); i++)
    {
        auto& n = nodes[i];
        for (auto& from : n.in)
        {
            auto index = atoi(from.c_str());
            if (index == -1 && i > 0)
            {
                n.prevs.push_back(&nodes[i - 1]);
                nodes[i - 1].nexts.push_back(&n);
            }
            else if (index >= 0)
            {
                n.prevs.push_back(&nodes[index]);
                nodes[index].nexts.push_back(&n);
            }
        }
    }

    calPosition(nodes);
}

void yamlyoloLoader::nodesToFile(const std::deque<Node>& nodes, const std::string& filename)
{}
