#include "cntk_postfix_iterator.h"
#include "check.hpp"
#include "cntk_includes.h"

namespace cntk = Microsoft::MSR::CNTK;
using namespace std;

struct CntkPostfixIteratorState
{
public:
    Microsoft::MSR::CNTK::ComputationNodeBasePtr node;
    int next_child = 0;
    CntkPostfixIteratorState(Microsoft::MSR::CNTK::ComputationNodeBasePtr n) :
        node(n), next_child(0)
    {}
    bool AreChildNodesVisited() const
    {
        return next_child >= static_cast<int>(node->GetInputs().size());
    }
    void MarkAllChildNodesAsVisited()
    {
        next_child = static_cast<int>(node->GetInputs().size());
    }
};

CntkPostfixIterator::CntkPostfixIterator(const cntk::ComputationNetworkPtr& net)
{
    const auto output_nodes = net->OutputNodes();
    // Currently, networks with multiple outputs are not supported.
    CHECK(output_nodes.size() == 1, "CntkPostfixIterator expects 1 output node (%d given)", output_nodes.size());
    auto state = make_unique<CntkPostfixIteratorState>(output_nodes.front());
    state_.push(move(state));
}

CntkPostfixIterator::~CntkPostfixIterator() {}

bool CntkPostfixIterator::HasNext() const
{
    return !state_.empty();
}

static int64_t GetNodeId(const cntk::ComputationNodeBasePtr& cntk_node)
{
    return reinterpret_cast<int64_t>(cntk_node.get());
}

unique_ptr<CntkPostfixIteratorState> CntkPostfixIterator::CreateIteratorState(const cntk::ComputationNodeBasePtr& node)
{
    auto iterator_state = make_unique<CntkPostfixIteratorState>(node);
    const int64_t node_id = GetNodeId(node);
    if (visited_nodes_.find(node_id) != visited_nodes_.end())
    {
        // Already visited.
        iterator_state->MarkAllChildNodesAsVisited();
    }
    return iterator_state;
}

const cntk::ComputationNodeBasePtr CntkPostfixIterator::GetNext()
{
    if (state_.empty())
    {
        return nullptr;
    }

    while (!state_.top()->AreChildNodesVisited())
    {
        const auto child_nodes = state_.top()->node->GetInputs();
        auto child = CreateIteratorState(child_nodes[state_.top()->next_child]);
        state_.top()->next_child++;
        state_.push(move(child));
    }

    CHECK(!state_.empty(), "Empty state in CntkPostfixIterator::GetNext");
    cntk::ComputationNodeBasePtr result = state_.top()->node;
    state_.pop();
    visited_nodes_.insert(GetNodeId(result));
    return result;
}