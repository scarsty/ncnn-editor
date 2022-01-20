#include "loader.h"

#include <algorithm>

#include "File.h"
#include "convert.h"

#include "ccccloader.h"
#include "yamlyololoader.h"
#include "ncnnloader.h"

void NodeLoader::calPosition(std::deque<Node>& nodes)
{
    int width = 250;

    for (auto& n : nodes)
    {
        if (n.position_x == -1 && n.position_y == -1)
        {
            if (n.prevs.empty())
            {
                n.position_x = width * 2;
                n.position_y = 150;
            }
            else
            {
                n.position_x = n.prevs[0]->position_x;
                n.position_y = n.prevs[0]->position_y + 150;
            }
        }
        int x_sum = 0, x_count = 0;
        for (auto& n1 : n.prevs)
        {
            if (n1->position_x > 0)
            {
                x_sum += n1->position_x;
                x_count++;
            }
        }
        if (x_count >= 2 || x_count == 1 && n.prevs[0]->nexts.size() == 1)
        {
            n.position_x = x_sum / x_count;
        }
        int count = 0;
        for (auto& n1 : n.nexts)
        {
            if (n1->position_x < 0)
            {
                n1->position_x = n.position_x + (count++) * width;
                if (n.nexts.size() > 1)
                {
                    n1->position_x -= width / 2 * (n.nexts.size() - 1);
                }
            }
            //if (n1->position_y < 0)
            {
                n1->position_y = std::max(n1->position_y, n.position_y + 150);
            }
        }
    }
    //同一下级的横坐标打散
    for (auto& n : nodes)
    {
        std::map<int, Node*> x_map;
        auto v = n.prevs;
        //有些情况上下级间不止一个连接, 不去重会过于偏右
        //std::sort(v.begin(), v.end());
        //v.erase(std::unique(v.begin(), v.end()), v.end());
        for (auto& n1 : v)
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
    std::map<std::pair<int, int>, Node*> node_pos_map;
    for (auto& n : nodes)
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


