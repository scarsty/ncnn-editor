#pragma once
#include "nodeloader.h"
#include "yaml-cpp/yaml.h"
#include <fstream>


class yamlyoloLoader : public NodeLoader
{
    //YAML::Node config_;
public:
    virtual void fileToNodes(const std::string& filename, std::deque<Node>& nodes) override;
    virtual void nodesToFile(const std::deque<Node>& nodes, const std::string& filename) override;
    virtual std::vector<std::string> efftiveKeys(const std::string& type)
    {
        return { "repeat", "" };
    }
private:
    YAML::Node config_;
};

