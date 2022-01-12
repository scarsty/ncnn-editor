#pragma once
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <unordered_map>

struct Node
{
    std::string title;
    std::string type;
    std::string text;
    std::map<std::string, std::string> values;

    std::vector<Node*> prevs, nexts;

    //use to draw nodes
    int id, text_id;
    int position_x = -1, position_y = -1;
    int erased = 0;
    int selected = 0;

    // 辅助值
    std::vector<std::string> in, out;



    Node()
    {
        //values =
        //{
        //    { "active", "" },
        //    { "window", "" },
        //    { "stride", "" },
        //    { "padding", "" },
        //    { "channel", "" },
        //    { "node", "" },
        //};
    }
};


/*
    接口应实现从文件到std::deque<Node>的转换，包含title，type，可编辑值等
    text可以加入编辑列表中没有的值
    Node的初始坐标可以自行计算，或者也可以调用Loader中自带的一个算法
*/
