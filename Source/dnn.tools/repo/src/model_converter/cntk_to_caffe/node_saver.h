#pragma once

#include <memory>
#include <string>
class Node;

class NodeSaver
{
public:
    void Save(std::shared_ptr<Node> root_node, const std::string& file);
};