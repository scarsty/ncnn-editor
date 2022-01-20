#include "loader.h"

#include <algorithm>

#include "File.h"
#include "convert.h"

//#ifdef NETEDIT_LOADER_CCCC
#include "ccccloader.h"
//#endif // NETEDIT_LOADER_CCCC

#ifdef NETEDIT_LOADER_YAML_YOLO
#include "yamlyololoader.h"
#endif // NETEDIT_LOADER_YAML_YOLO

#ifdef NETEDIT_LOADER_NCNN
#include "ncnnloader.h"
#endif // NETEDIT_LOADER_NCNN

NodeLoader* create_loader(const std::string& filename, int index)
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
    auto ext = convert::toLowerCase(File::getFileExt(filename));
    if (ext == "ini")
    {
        return new ccccLoader();
    }
#ifdef NETEDIT_LOADER_YAML_YOLO
    if (ext == "yaml")
    {
        return new yamlyoloLoader();
    }
#endif // NETEDIT_LOADER_YAML_YOLO
#ifdef NETEDIT_LOADER_NCNN
    if (ext == "param")
    {
        auto str = convert::readStringFromFile(filename);
        int a = atoi(convert::findANumber(str).c_str());
        if (a == 7767517)
        {
            return new ncnnLoader();
        }
    }
#endif NETEDIT_LOADER_NCNN
    return new ccccLoader();
}

const char* file_filter()
{
#ifdef _WIN32
    return "CCCC Example\0*.ini\0yolort\0*.yaml\0ncnn & pnnx\0*.param\0";
#endif
}


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

//最后一个参数为假，仅计算是否存在连接，为真则是严格计算传导顺序
void NodeLoader::push_cal_stack(Node* layer, int direct, std::vector<Node*>& stack, bool turn)
{
    //lambda函数：层是否已经在向量中
    auto contains = [&](std::vector<Node*>& v, Node* l) -> bool
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