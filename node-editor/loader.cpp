#include "loader.h"

#include "File.h"
#include "ccccloader.h"
#include "yamlyololoader.h"

NodeLoader* create_loader(const std::string& filename)
{
    if (File::getFileExt(filename) == "ini")
    {
        return new ccccLoader();
    }
    if (File::getFileExt(filename) == "yml")
    {
        return new yamlyoloLoader();
    }
    return nullptr;
}
