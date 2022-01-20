#pragma once
#include "node.h"

//实现此接口即可支持不同格式
//还没设计完
class NodeLoader
{
public:
    virtual ~NodeLoader() = default;
    virtual void fileToNodes(const std::string& filename, std::deque<Node>& nodes) {}
    virtual void nodesToFile(const std::deque<Node>& nodes, const std::string& filename) {}
    //virtual std::vector<std::string> efftiveKeys(const std::string& type) { return { "" }; }
    virtual void refreshNodeValues(Node&) {}
public:
    void calPosition(std::deque<Node>& nodes);
    void push_cal_stack(Node* layer, int direct, std::vector<Node*>& stack, bool turn);
    static std::string& mainPath()
    {
        static std::string path;
        return path;
    }
};

NodeLoader* create_loader(const std::string& filename, int filter_index = -1);
const char* file_filter();