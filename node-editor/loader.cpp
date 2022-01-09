#include "loader.h"

#include "File.h"
#include "convert.h"

#include "ccccloader.h"
#include "yamlyololoader.h"
#include "ncnnloader.h"

NodeLoader* create_loader(const std::string& filename)
{
    auto ext = convert::toLowerCase(File::getFileExt(filename));
    if (ext == "ini")
    {
        return new ccccLoader();
    }
    else if (ext == "yaml")
    {
        return new yamlyoloLoader();
    }
    else if (ext == "param")
    {
        auto str = convert::readStringFromFile(filename);
        int a = atoi(convert::findANumber(str).c_str());
        if (a == 7767517)
        {
            return new ncnnLoader();
        }
    }
    return nullptr;
}
