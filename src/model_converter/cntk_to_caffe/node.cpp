#include "node.h"
#include <algorithm>
#include <sstream>

namespace cntk = Microsoft::MSR::CNTK;
using namespace std;

Node::Node(const CreationAttorney&, cntk::ComputationNodeBasePtr head_node) :
cntk_head_node_(head_node)
{
    if (head_node != nullptr)
    {
        name_ = head_node->GetName();
    }
}

cntk::ComputationNodeBasePtr Node::GetCntkHeadNode()
{
    return cntk_head_node_;
}

void Node::AddAttribute(const NodeAttribute& attribute, cntk::ComputationNodeBasePtr value)
{
    attributes_[attribute] = value;
}

void Node::RemoveTopConnection(shared_ptr<Node> node)
{
    top_connections_.erase(find(top_connections_.begin(), top_connections_.end(), node));
}

void Node::AddTopConnection(shared_ptr<Node> node)
{
    top_connections_.emplace_back(node);
}

void Node::AddBottomConnection(shared_ptr<Node> node)
{
    bottom_connections_.emplace_back(node);
}

const vector<shared_ptr<Node>>& Node::GetTopConnections() const
{
    return top_connections_;
}

const vector<shared_ptr<Node>>& Node::GetBottomConnections() const
{
    return bottom_connections_;
}
