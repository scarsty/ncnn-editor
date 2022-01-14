#pragma once
#include "node.h"

//ʵ�ִ˽ӿڼ���֧�ֲ�ͬ��ʽ
//��û�����
class NodeLoader
{
public:
    virtual ~NodeLoader() = default;
    virtual void fileToNodes(const std::string& filename, std::deque<Node>& nodes) {}
    virtual void nodesToFile(const std::deque<Node>& nodes, const std::string& filename) {}
    //virtual std::vector<std::string> efftiveKeys(const std::string& type) { return { "" }; }
    virtual void refreshNodeValues(Node&) {}
public:
    void calPosition(std::deque<Node>& nodes);
    void push_cal_stack(Node* layer, int direct, std::vector<Node*>& stack, bool turn);
};

NodeLoader* create_loader(const std::string& filename);