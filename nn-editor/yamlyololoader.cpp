#include "yamlyololoader.h"

#include "fmt1.h"
#include "convert.h"

#include <iostream>

struct Layer
{
    std::vector<int> from;
    int repeat;
    std::string type;
    std::vector<std::string> paras;
};

static Node yaml2l(YAML::Node y)
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
    auto back = config["backbone"];
    for (size_t i = 0; i < back.size(); i++)
    {
        auto n = yaml2l(back[i]);
        n.title = "back_" + std::to_string(i);
        nodes.emplace_back(std::move(n));
    }
    auto head = config["head"];

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
{
    std::vector<Node*> nodes_turn;
    push_cal_stack((Node*)&nodes[0], 1, nodes_turn, true);

    for (int i = 0; i < nodes_turn.size(); i++)
    {
        nodes_turn[i]->turn = i;
    }

    auto get_node = [&nodes_turn](std::string end_flag, int& index)
    {
        YAML::Node vec = YAML::Load("[]");
        for (int i = index; i < nodes_turn.size(); i++)
        {
            auto n = nodes_turn[i];
            YAML::Node n1 = YAML::Load("[]");
            if (n->prevs.size() == 0)
            {
                n1[0] = -1;
            }
            else if (n->prevs.size() == 1)
            {
                n1[0] = -1;
                if (n->prevs[0]->turn != n->turn - 1)
                {
                    n1[0] = n->prevs[0]->turn;
                }
            }
            else
            {
                n1[0] = YAML::Load("[]");
                for (auto np : n->prevs)
                {
                    if (np->turn == n->turn - 1)
                    {
                        n1[0].push_back(-1);
                    }
                    else
                    {
                        n1[0].push_back(np->turn);
                    }
                }
            }
            n1[1] = n->values["repeat"];
            n1[2] = n->type;
            n1[3] = YAML::Load("[]");
            for (auto str : convert::splitString(n->text, ", "))
            {
                n1[3].push_back(str);
            }
            vec.push_back(n1);
            if (n->type == end_flag)
            {
                index = i + 1;
                break;
            }
        }        
        return vec;
    };
    int i = 0;
    config_["backbone"] = get_node("SPPF", i);
    config_["head"] = get_node("GUO DOU SHI ZOU LAO DE", i);


    std::cout << config_;
    std::ofstream fout(filename);
    fout << config_;

}
