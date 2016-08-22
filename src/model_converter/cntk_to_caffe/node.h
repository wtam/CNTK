#pragma once

#include "cntk_includes.h"
#include "node_attributes.h"
#include "node_tag.h"
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

class Node
{
public:
    class CreationAttorney
    {
    private:
        CreationAttorney() = default;
        friend class NodeFactory;
    };

    Node(const CreationAttorney&, Microsoft::MSR::CNTK::ComputationNodeBasePtr head_node);

    const std::unordered_set<NodeTag>& GetTags() const { return tags_; }
    void SetTags(const std::unordered_set<NodeTag>& tags) { tags_ = tags; }

    const NodeAttributes& GetAttributes() const { return attributes_; }
    void SetAttributes(const NodeAttributes& attributes) { attributes_ = attributes; }
    void AddAttribute(const NodeAttribute& attribute, Microsoft::MSR::CNTK::ComputationNodeBasePtr value);

    Microsoft::MSR::CNTK::ComputationNodeBasePtr GetCntkHeadNode();

    void RemoveTopConnection(std::shared_ptr<Node> node);
    void AddTopConnection(std::shared_ptr<Node> node);
    void AddBottomConnection(std::shared_ptr<Node> node);
    const std::vector<std::shared_ptr<Node>>& GetTopConnections() const;
    const std::vector<std::shared_ptr<Node>>& GetBottomConnections() const;

    const std::wstring& GetName() const { return name_; }

private:
    // Inner product layer in CNTK can be represented as (+) (bias) ((*) (weights) (input)) in prefix notation.
    // cntk_head_node_ will point to (+) node from which it is possible to iterate to other nodes.
    // (bias) and (weights) CNTK nodes can be accessed through attributes_.
    Microsoft::MSR::CNTK::ComputationNodeBasePtr cntk_head_node_;
    std::unordered_set<NodeTag> tags_;
    NodeAttributes attributes_;
    std::vector<std::shared_ptr<Node>> top_connections_;
    std::vector<std::shared_ptr<Node>> bottom_connections_;
    std::wstring name_;
};
