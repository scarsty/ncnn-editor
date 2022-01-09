#pragma once
#include <string>
#include <vector>
#include <map>

struct Node
{
    std::string title;
    std::string type;
    std::string text;
    std::map<std::string, std::string> values;
    int id, text_id;
    int erased = 0;

    std::vector<Node*> prevs, nexts;
    std::vector<std::string> in, out;

    int position_x = -1, position_y = -1;

    Node()
    {
        values =
        {
            { "active", "" },
            { "window", "" },
            { "stride", "" },
            { "padding", "" },
            { "channel", "" },
            { "node", "" },
        };
    }
};