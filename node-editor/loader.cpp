#include "loader.h"

#include "File.h"
#include "ccccloader.h"

NodeLoader* create_loader(const std::string& filename)
{
    if (File::getFileExt(filename) == "ini")
    {
        return new ccccLoader();
    }
    return nullptr;
}
