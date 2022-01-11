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
    std::vector<std::string> in, out;

    //use to draw nodes
    int id, text_id;
    int position_x = -1, position_y = -1;
    int erased = 0;
    int selected = 0;

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