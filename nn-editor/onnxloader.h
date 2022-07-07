#pragma once
#include "FileLoader.h"

class onnxLoader : public FileLoader
{
public:
    onnxLoader();
    virtual void fileToNodes(const std::string& filename, std::deque<Node>& nodes) override;
    virtual void nodesToFile(const std::deque<Node>& nodes, const std::string& filename) override;
    //virtual std::vector<std::string> efftiveKeys(const std::string& type) override;
    virtual void refreshNodeValues(Node&) override;
private:
    //std::map<std::string, std::map<int, std::string>> int_to_string_;
    //std::map<std::string, std::map<std::string, int>> string_to_int_;
};

