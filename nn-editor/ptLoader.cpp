#include "ptLoader.h"
#include <torch/script.h>
#include "fmt1.h"

ptLoader::ptLoader()
{
}

void ptLoader::fileToNodes(const std::string& filename, std::deque<Node>& nodes)
{
    fmt1::print("{}\n", filename);
    torch::jit::Module mod = torch::jit::load(filename);
    mod.eval();
    auto g = mod.get_method("forward").graph();
    int i = 0;
    g->print(std::cout);
    for (auto n:g->nodes())
    {
        //for (const auto& v : n->outputs())
        //{
        //    auto tensor_type = v->type()->cast<torch::jit::TensorType>();
        //    if (!tensor_type)
        //        continue;

        //   fmt1::print("{}\n", v->debugName().c_str());

        //}
        //fmt1::print("{}{}\n",i++, );
    }
}

void ptLoader::nodesToFile(const std::deque<Node>& nodes, const std::string& filename)
{

}

void ptLoader::refreshNodeValues(Node& node)
{

}
