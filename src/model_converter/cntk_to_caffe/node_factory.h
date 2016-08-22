#pragma once

#include <cstdint>
#include <memory>
#include <map>
#include <unordered_set>
#include "cntk_includes.h"

class Node;
enum class NodeTag;

class NodeFactory
{
public:
    ~NodeFactory();
    void Clear();
    std::shared_ptr<Node> CreateNode(Microsoft::MSR::CNTK::ComputationNodeBasePtr cntk_node);
    std::shared_ptr<Node> GetNode(const Microsoft::MSR::CNTK::ComputationNodeBasePtr& head_node);
    bool HasNode(const Microsoft::MSR::CNTK::ComputationNodeBasePtr& head_node);
private:
    std::map<int64_t, std::shared_ptr<Node>> nodes_;
};