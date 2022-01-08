#include "loader.h"

#include "File.h"
#include "ccccloader.h"
#include "yamlyololoader.h"
#include "ncnnloader.h"

NodeLoader* create_loader(const std::string& filename)
{
    if (File::getFileExt(filename) == "ini")
    {
        return new ccccLoader();
    }
    else if (File::getFileExt(filename) == "yaml")
    {
        return new yamlyoloLoader();
    }
    else if (File::getFileExt(filename) == "param")
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
