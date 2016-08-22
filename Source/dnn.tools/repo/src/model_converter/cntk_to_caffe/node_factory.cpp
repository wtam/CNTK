#include "node_factory.h"
#include "node.h"
#include "node_tag.h"
#include "check.hpp"
#include <stdexcept>

using namespace std;
namespace cntk = Microsoft::MSR::CNTK;

static int64_t GetId(const cntk::ComputationNodeBasePtr& cntk_node)
{
    return reinterpret_cast<int64_t>(cntk_node.get());
}

void NodeFactory::Clear()
{
    nodes_.clear();
}

NodeFactory::~NodeFactory() = default;

shared_ptr<Node> NodeFactory::CreateNode(cntk::ComputationNodeBasePtr cntk_node)
{
    CHECK(!HasNode(cntk_node));
    const int64_t id = GetId(cntk_node);
    nodes_[id] = make_shared<Node>(Node::CreationAttorney(), cntk_node);
    return nodes_[id];
}

shared_ptr<Node> NodeFactory::GetNode(const Microsoft::MSR::CNTK::ComputationNodeBasePtr& head_node)
{
    const int64_t id = GetId(head_node);
    return nodes_.at(id);
}

bool NodeFactory::HasNode(const cntk::ComputationNodeBasePtr& head_node)
{
    const int64_t id = GetId(head_node);
    return nodes_.find(id) != nodes_.end();
}