#pragma once
#include "FileLoader.h"
class ptLoader :
    public FileLoader
{
public:
    ptLoader();
    virtual void fileToNodes(const std::string& filename, std::deque<Node>& nodes) override;
    virtual void nodesToFile(const std::deque<Node>& nodes, const std::string& filename) override;
    //virtual std::vector<std::string> efftiveKeys(const std::string& type) override;
    virtual void refreshNodeValues(Node&) override;
};

