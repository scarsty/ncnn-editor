#pragma once
#include "node.h"

//实现此接口即可支持不同格式
//还没设计完
class NodeLoader
{
public:
    virtual ~NodeLoader() = default;
    virtual void fileToNodes(const std::string& filename, std::vector<Node>& nodes) {}
    virtual void nodesToFile(const std::vector<Node>& nodes, const std::string& filename) {}
    virtual std::vector<std::string> efftiveKeys(const std::string& type) { return {}; }
};

NodeLoader* create_loader(const std::string& filename);