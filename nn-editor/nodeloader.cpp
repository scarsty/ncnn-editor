#include "nodeloader.h"

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

#include "onnxloader.h"

#define M_PI 3.14159265358979323846
extern "C"
{
#include "sfg.h"
}
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
#endif // NETEDIT_LOADER_NCNN
    if (ext == "onnx")
    {
        return new onnxloader();
    }
    return new ccccLoader();
}

const char* file_filter()
{
#ifdef _WIN32
    return "CCCC Example\0*.ini\0yolort\0*.yaml\0ncnn & pnnx\0*.param\0";
#endif
    return nullptr;
}


void NodeLoader::calPosition(std::deque<Node>& nodes)
{
    auto status = sfg_init();

    //graph g;

    int i = 1;
    for (auto& n : nodes)
    {
        n.turn = i;
        sfg_addnode(i, 200, 50);
        i++;
    }
    i = 1;
    for (auto& n : nodes)
    {
        for (auto& nn : n.nexts)
        {
            sfg_addedge(i++, n.turn, nn->turn, 0,0);
            //g.add_edge(n.turn, nn->turn);
        }
    }
    sfg_xspacing(50);
    sfg_yspacing(50);
    sfg_layout();
    for (        auto& n : nodes)
    {
        n.position_x = sfg_nodexpos(n.turn);
        n.position_y = sfg_nodeypos(n.turn);

    }
    sfg_deinit();







    return;
    //连接关系
    std::vector<Node*> nodes_turn;
    for (auto& n : nodes)
    {
        if (n.prevs.size() == 0)
        {
            push_cal_stack((Node*)&n, 1, nodes_turn, true);
            break;
        }
    }
    //层序
    for (auto& n : nodes)
    {
        if (n.prevs.size() > 0)
        {
            for (auto& np : n.prevs)
            {
                n.turn = (std::max)(n.turn, np->turn + 1);
            }
        }
    }
    //层序
    for (auto& n : nodes)
    {
        if (n.nexts.size() > 0 && n.prevs.empty())
        {
            for (auto& np : n.nexts)
            {
                n.turn = (std::max)(n.turn, np->turn - 1);
            }
        }
    }
    //每层个数
    std::map<int, int> turn_count, turn_count1;
    for (auto& n : nodes)
    {
        turn_count[n.turn]++;
    }
    int width = 400;
    for (auto& n : nodes)
    {
        n.position_y = 150 * (n.turn + 1);
        n.position_x = width * 2 - width * (turn_count[n.turn] / 2.0) + width * (turn_count1[n.turn]++);
    }
    //同一层的上下层打散
    for (auto& n : nodes)
    {
        //if (n.prevs.size() <= 1)
        {
            int count = n.nexts.size();
            int count1 = 0;
            for (auto& n1 : n.nexts)
            {
                if (n1->nexts.size() > 1)
                    n1->position_x = n.position_x + width / 2 - width * (count / 2.0) + width * (count1++);
            }
        }
        //if (n.nexts.size() <= 1)
        {
            int count = n.prevs.size();
            int count1 = 0;
            for (auto& n1 : n.prevs)
            {
                if (n1->prevs.size() > 1)
                    n1->position_x = n.position_x + width / 2 - width * (count / 2.0) + width * (count1++);
            }
        }
    }

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

