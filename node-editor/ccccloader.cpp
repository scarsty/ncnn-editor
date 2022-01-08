#include "ccccloader.h"

void ccccLoader::fileToNodes(const std::string& filename, std::vector<Node>& nodes)
{
    INIReaderNormal ini;
    ini.loadFile(filename);
    nodes.clear();

    std::string prefix = "layer_";
    auto sections = ini.getAllSections();
    std::sort(sections.begin(), sections.end(), [&](const std::string& l, const std::string& r)
    {
        return ini.getSectionNo(l) < ini.getSectionNo(r);
    });

    int x = -1;
    int y = -1;
    int id = 0;
    for (auto& s : sections)
    {
        if (s.find(prefix) == 0)
        {
            Node node;
            node.title = s;
            node.type = ini.getString(s, "type");
            for (auto& kv : node.values)
            {
                kv.second = ini.getString(s, kv.first);
            }
            std::vector<int> v = convert::findNumbers<int>(ini.getString(s, "editor_position"));
            if (v.size() >= 2)
            {
                x = v[0];
                y = v[1];
            }
            node.position_x = x;
            node.position_y = y;
            nodes.push_back(std::move(node));
        }
    }

    //create next
    for (auto& n : nodes)
    {
        std::vector<std::string> v = convert::splitString(ini.getString(n.title, "next"), ",");
        for (auto& s : v)
        {
            for (auto& n1 : nodes)
            {
                if (s == n1.title)
                {
                    n.nexts.push_back(&n1);
                }
            }
        }
    }
}

void ccccLoader::nodesToFile(const std::vector<Node>& nodes, const std::string& filename)
{
    INIReaderNormal ini;
    ini.loadFile(filename);
    for (auto& section : ini.getAllSections())
    {
        if (section.find("layer_") == 0)
        {
            ini.eraseSection(section);
        }
    }

    for (auto& node : nodes)
    {
        if (node.erased) { continue; }
        ini.setKey(node.title, "type", node.type);
        ini.setKey(node.title, "editor_position", std::to_string(node.position_x) + "," + std::to_string(node.position_y));
        std::string next_str;
        for (auto& n : node.nexts)
        {
            next_str += n->title + ",";
        }
        if (!node.nexts.empty())
        {
            next_str.pop_back();
        }
        ini.setKey(node.title, "next", next_str);
        for (auto& kv : node.values)
        {
            if (!kv.second.empty())
            {
                ini.setKey(node.title, kv.first, kv.second);
            }
        }
    }
    ini.saveFile(filename);
}

std::vector<std::string> ccccLoader::efftiveKeys(const std::string& type)
{
    if (type == "conv")
    {
        return { "active", "channel", "window", "stride", "padding" };
    }
    else if (type == "pool")
    {
        return { "active", "pooltype", "window", "stride", "padding" };
    }
    else if (type == "fc")
    {
        return { "active", "node" };
    }
    else if (type == "data")
    {
        return { "data" };
    }
    return {};
}
