#include "loader.h"

#include "File.h"
#include "convert.h"

#include "ccccloader.h"
#include "yamlyololoader.h"
#include "ncnnloader.h"

void NodeLoader::calPosition(std::deque<Node>& nodes)
{
    std::map<std::pair<int, int>, Node*> node_pos_map;
    int width = 250;

    auto avoid_overlap = [&node_pos_map, width](Node& n)->int
    {
        for (auto& kv : node_pos_map)
        {
            int x = kv.first.first;
            int y = kv.first.second;
            if (y == n.position_y && abs(x - n.position_x) < width)
            {
                if (n.position_x >= x)
                {
                    n.position_x = x + width;
                }
                else
                {
                    n.position_x = x - width;
                }
                break;
            }
        }
        node_pos_map[{n.position_x, n.position_y}] = &n;
        return (n.position_x - width / 2) * 1e9 + n.position_y;
    };

    for (auto& n : nodes)
    {
        if (n.position_x == -1 && n.position_y == -1)
        {
            if (n.prevs.empty())
            {
                n.position_x = width;
                n.position_y = 150;
            }
            else
            {
                n.position_x = n.prevs[0]->position_x;
                n.position_y = n.prevs[0]->position_y + 150;
            }
        }
        int count = 0;
        for (auto& n1 : n.nexts)
        {
            if (n1->position_x < 0)
            {
                n1->position_x = n.position_x + (count++) * width;
                if (n.nexts.size() > 1)
                {
                    n1->position_x -= width * (n.nexts.size() - 1.5);
                }
            }
            if (n1->position_y < 0)
            {
                n1->position_y = std::max(n1->position_y, n.position_y + 150);
            }
        }
    }
    for (auto& n : nodes)
    {
        std::map<int, Node*> x_map;
        for (auto& n1 : n.prevs)
        {
            if (x_map.count(n1->position_x))
            {
                n1->position_x += width;
            }
            x_map[n1->position_x] = n1;
            n.position_y = std::max(n.position_y, n1->position_y + 150);
        }
    }
    // avoid the same position
    for (auto& n : nodes)
    {
        avoid_overlap(n);
    }
}

NodeLoader* create_loader(const std::string& filename)
{
    auto ext = convert::toLowerCase(File::getFileExt(filename));
    if (ext == "ini")
    {
        return new ccccLoader();
    }
    else if (ext == "yaml")
    {
        return new yamlyoloLoader();
    }
    else if (ext == "param")
    {
        auto str = convert::readStringFromFile(filename);
        int a = atoi(convert::findANumber(str).c_str());
        if (a == 7767517)
        {
            return new ncnnLoader();
        }
    }
    return new ccccLoader();
}


