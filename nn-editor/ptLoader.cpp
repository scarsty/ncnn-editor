#include "ptLoader.h"
#include <torch/script.h>
#include <iostream>

ptLoader::ptLoader()
{
}

void ptLoader::fileToNodes(const std::string& filename, std::deque<Node>& nodes)
{
    std::cout << filename << '\n';
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

        //   std::cout << v->debugName().c_str() << '\n';

        //}
        //std::cout << i++ << '\n';
    }
}

void ptLoader::nodesToFile(const std::deque<Node>& nodes, const std::string& filename)
{

}

void ptLoader::refreshNodeValues(Node& node)
{

}
