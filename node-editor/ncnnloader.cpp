#include "ncnnloader.h"
#include "convert.h"

void ncnnLoader::fileToNodes(const std::string& filename, std::vector<Node>& nodes)
{
    nodes.clear();
    auto str = convert::readStringFromFile(filename);
    auto lines = convert::splitString(str, "\n");
    int layer_count = 0, blob_count = 0;
    if (lines.size() >= 2)
    {
        auto v = convert::findNumbers<int>(lines[1]);
        layer_count = v[0];
        blob_count = v[1];
    }

    for (int i = 2; i < lines.size(); i++)
    {
        auto v = convert::splitString(lines[i], " ");
        if (v.size() >= 2)
        {
            Node node;
            node.title = v[1];
            node.type = v[0];

            int input_count = atoi(v[2].c_str());
            int output_count = atoi(v[3].c_str());
            

            for (int i_in = 0; i_in < input_count; i_in++)
            {
                node.in.push_back(v[4+i_in]);
            }
            for (int i_out = 0; i_out < output_count; i_out++)
            {
                node.out.push_back(v[4 + input_count + i_out]);
            }
            //int input_blobs = atoi(v[4].c_str());
            //int output_blobs = atoi(v[5].c_str());
            nodes.push_back(std::move(node));
        }
    }

    for (auto& node1 : nodes)
    {
        for (auto& node2 : nodes)
        {
            for (auto& from : node1.out)
            {
                for (auto& to : node2.in)
                {
                    if (from == to)
                    {
                        node1.nexts.push_back(&node2);
                    }
                }
            }
        }
    }
}

void ncnnLoader::nodesToFile(const std::vector<Node>& nodes, const std::string& filename)
{
}

std::vector<std::string> ncnnLoader::efftiveKeys(const std::string& type)
{
    if (type == "Convolution")
    {
        return { "channel", "window", "stride", "padding" };
    }
    else if (type == "Pooling")
    {
        return { "pooltype", "window", "stride", "padding" };
    }
    else if (type == "InnerProduct")
    {
        return { "node" };
    }
    else if (type == "input")
    {
        return { "data" };
    }
    return {};
}
