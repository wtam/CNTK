#include "reduce_rules.h"
#include "check.hpp"
#include "cntk_includes.h"
#include "node.h"

using namespace std;

bool ReduceRule::CanBeApplied(const list<shared_ptr<Node>>& nodes) const
{
    if (rule_.size() > nodes.size())
    {
        return false; // Too short sequence.
    }

    // Reduce rule is applicable if nodes from beginning of the list pass rule filters in corresponding order,
    // i.e. the 1st node passes the 1st filter, the 2nd node passes the 2nd filter... up to the number of filters.
    bool can_be_applied = true;
    auto node = nodes.cbegin();
    for (size_t i = 0; i < rule_.size(); ++i, ++node)
    {
        CHECK((*node) != nullptr);
        if (!rule_[i].HasPassedFilter((*node)->GetTags()))
        {
            can_be_applied = false;
            break;
        }
    }
    return can_be_applied;
}