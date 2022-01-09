#pragma once
#include "loader.h"

class ccccLoader : public NodeLoader
{
public:
    virtual void fileToNodes(const std::string& filename, std::vector<Node>& nodes) override;
    virtual void nodesToFile(const std::vector<Node>& nodes, const std::string& filename) override;
    virtual std::vector<std::string> efftiveKeys(const std::string& type) override;
};

