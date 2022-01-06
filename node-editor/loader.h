#pragma once
#include <string>
#include <vector>

//实现此接口即可支持不同格式
//还没设计完
class NodeLoader
{
public:
    virtual void loadFile(const std::string& filename) = 0;
    virtual void saveFile(const std::string& filename) = 0;
    virtual std::vector<std::string> getAllLayers() = 0;
    virtual void setLayerPro(const std::string& layer, const std::string& pro, const std::string& value) = 0;
    virtual std::string getLayerPro(const std::string& layer, const std::string& pro) = 0;
    virtual void setNext(const std::string& layer, const std::vector<std::string>& nexts) = 0;
    virtual std::vector<std::string> getNext(const std::string& layer) = 0;
    virtual void clear() = 0;
    virtual void eraseLayer(const std::string& layer) = 0;
    virtual void eraseAllLayers() = 0;
};

NodeLoader* cccc_loader();