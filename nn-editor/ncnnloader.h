#pragma once
#include "loader.h"

class ncnnLoader : public NodeLoader
{
public:
    ncnnLoader();
    virtual void fileToNodes(const std::string& filename, std::deque<Node>& nodes) override;
    virtual void nodesToFile(const std::deque<Node>& nodes, const std::string& filename) override;
    //virtual std::vector<std::string> efftiveKeys(const std::string& type) override;
    virtual void refreshNodeValues(Node&) override;
private:
    bool is_pnnx_ = false;
    std::map<std::string, std::map<std::string, std::string>> int_to_string_;
    std::map<std::string, std::map<std::string, std::string>> string_to_int_;
};

