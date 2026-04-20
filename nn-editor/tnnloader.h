#pragma once

#include "FileLoader.h"
#include <map>

class tnnLoader : public FileLoader
{
public:
    tnnLoader();
    void fileToNodes(const std::string& filename, std::deque<Node>& nodes) override;
    void nodesToFile(const std::deque<Node>& nodes, const std::string& filename) override;
    void refreshNodeValues(Node& n) override;
private:
    std::map<std::string, std::map<int, std::string>> int_to_string_;
    std::map<std::string, std::map<std::string, int>> string_to_int_;
};
