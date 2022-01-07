#pragma once
#include "loader.h"
#include "INIReader.h"
#include "convert.h"

class ccccLoader : public NodeLoader
{
    INIReaderNormal config_;
public:
    virtual void loadFile(const std::string& filename) override
    {
        config_.loadFile(filename);
    }
    virtual void saveFile(const std::string& filename) override
    {
        config_.saveFile(filename);
    }
    virtual std::vector<std::string> getAllLayers() override
    {
        std::string prefix = "layer_";
        auto sections = config_.getAllSections();
        std::sort(sections.begin(), sections.end(), [this](const std::string& l, const std::string& r)
        {
            return config_.getSectionNo(l) < config_.getSectionNo(r);
        });
        std::vector<std::string> res;
        for (auto& s : sections)
        {
            if (s.find(prefix) == 0)
            {
                res.push_back(s);
            }
        }
        return res;
    }
    virtual void setLayerPro(const std::string& layer, const std::string& pro, const std::string& value) override
    {
        config_.setKey(layer, pro, value);
    }
    virtual std::string getLayerPro(const std::string& layer, const std::string& pro) override
    {
        return config_.getString(layer, pro);
    }
    virtual std::vector<std::string> getLayerAllPro(const std::string& layer) override
    {
        return config_.getAllKeys(layer);
    }
    virtual void setLayerNext(const std::string& layer, const std::vector<std::string>& nexts) override
    {
        std::string str;
        for (auto& l : nexts)
        {
            str += l + ",";
        }
        if (!str.empty())
        {
            str.pop_back();
        }
        config_.setKey(layer, "next", str);
    }
    virtual std::vector<std::string> getLayerNext(const std::string& layer) override
    {
        return convert::splitString(getLayerPro(layer, "next"));
    }
    virtual void clear() override
    {
        config_ = INIReaderNormal();
    }
    virtual void eraseLayer(const std::string& layer) override
    {
        config_.eraseSection(layer);
    }
    virtual void eraseAllLayers() override
    {
        for (auto& section : config_.getAllSections())
        {
            if (section.find("layer_") == 0)
            {
                config_.eraseSection(section);
            }
        }
    }

};

