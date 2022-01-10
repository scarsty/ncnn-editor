#pragma once
#include "loader.h"
//#include "yaml-cpp/yaml.h"
#include <fstream>


class yamlyoloLoader : public NodeLoader
{
    //YAML::Node config_;
public:
    virtual void fileToNodes(const std::string& filename, std::vector<Node>& nodes) override;
    virtual void nodesToFile(const std::vector<Node>& nodes, const std::string& filename) override;
};

