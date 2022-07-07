#include "ptLoader.h"
#include <torch/script.h>
#include "fmt1.h"

ptLoader::ptLoader()
{
}

void ptLoader::fileToNodes(const std::string& filename, std::deque<Node>& nodes)
{
    fmt1::print("{}", filename);
    torch::jit::Module mod = torch::jit::load(filename);
    mod.eval();

    //     mod.dump(true, false, false);
    //     mod.dump(true, true, true);

    auto g = mod.get_method("forward").graph();
}

void ptLoader::nodesToFile(const std::deque<Node>& nodes, const std::string& filename)
{

}

void ptLoader::refreshNodeValues(Node& node)
{

}
