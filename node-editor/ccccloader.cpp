#include "loader.h"
#include "INIReader.h"
#include "convert.h"

class ccccLoader : public NodeLoader
{
    INIReaderNormal ini_;
public:
    virtual void loadFile(const std::string& filename) override
    {
        ini_.loadFile(filename);
    }
    virtual void saveFile(const std::string& filename) override
    {
        ini_.saveFile(filename);
    }
    virtual std::vector<std::string> getAllLayers() override
    {
        std::string prefix = "layer_";
        auto sections = ini_.getAllSections();
        std::sort(sections.begin(), sections.end(), [this](const std::string& l, const std::string& r)
        {
            return ini_.getSectionNo(l) < ini_.getSectionNo(r);
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
        ini_.setKey(layer, pro, value);
    }
    virtual std::string getLayerPro(const std::string& layer, const std::string& pro) override
    {
        return ini_.getString(layer, pro);
    }
    virtual void setNext(const std::string& layer, const std::vector<std::string>& nexts) override {}
    virtual std::vector<std::string> getNext(const std::string& layer) override
    {
        return convert::splitString(getLayerPro(layer, "next"));
    }
    virtual void clear() override
    {
        //for (auto& section : ini_.getAllSections())
        //{
        //    if (section.find("layer_") == 0)
        //    {
        //        ini_.eraseSection(section);
        //    }
        //}
        ini_ = INIReaderNormal();
    }
    virtual void eraseLayer(const std::string& layer) override
    {
        ini_.eraseSection(layer);
    }
    virtual void eraseAllLayers() override
    {
        for (auto& section : ini_.getAllSections())
        {
            if (section.find("layer_") == 0)
            {
                ini_.eraseSection(section);
            }
        }
    }

};

NodeLoader* cccc_loader()
{
    return new ccccLoader();
}