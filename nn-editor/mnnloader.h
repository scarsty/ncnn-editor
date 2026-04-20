#pragma once

#include "FileLoader.h"
#include <map>

class mnnLoader : public FileLoader
{
public:
    mnnLoader();
    void fileToNodes(const std::string& filename, std::deque<Node>& nodes) override;
    void nodesToFile(const std::deque<Node>& nodes, const std::string& filename) override;
    void refreshNodeValues(Node&) override {}
private:
    std::map<int, std::string> operator_to_name_;
};
