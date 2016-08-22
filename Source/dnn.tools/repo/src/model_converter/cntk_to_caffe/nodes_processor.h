#pragma once

#include <list>
#include <memory>

class Node;

// Interface for reducing list of nodes created by iterating through CNTK graph in postfix manner.
class NodesProcessor
{
public:
    // Checks if processor can be applied to the start of the sequence.
    virtual bool CanBeApplied(const std::list<std::shared_ptr<Node>>& sequence) = 0;
    // Replaces one or more nodes from the list start with one product node.
    virtual void Apply(std::list<std::shared_ptr<Node>>& sequence) = 0;
    virtual ~NodesProcessor() = default;
};