#include "ptLoader.h"
#include <torch/script.h>

ptLoader::ptLoader()
{
}

void ptLoader::fileToNodes(const std::string& filename, std::deque<Node>& nodes)
{
    torch::jit::Module mod = torch::jit::load(filename);
}

void ptLoader::nodesToFile(const std::deque<Node>& nodes, const std::string& filename)
{

}

void ptLoader::refreshNodeValues(Node& node)
{

}
