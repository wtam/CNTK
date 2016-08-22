#pragma once

#include "tag_filter.h"
#include "node_tag.h"
#include <list>
#include <memory>
#include <string>
#include <vector>
#include <unordered_set>

class Node;

// Contains rule for replacing/resolving array of nodes with a single node.
// Rule can be applied to reduce array of nodes if all nodes pass filters specified in rule ctor.
// E.g. reduce rule could contain information that list of nodes [ElementTimes, LearningParameter, InputValue] can be
// replaced with single InputValue node with scale parameter.
class ReduceRule
{
public:
    ReduceRule(std::vector<Filter<NodeTag>>&& rule,
        std::unordered_set<NodeTag>&& resolved_tags) :
        rule_(std::move(rule)), resolved_tags_(std::move(resolved_tags))
    {}
    ReduceRule(ReduceRule&& other) :
        rule_(move(other.rule_)), resolved_tags_(move(other.resolved_tags_))
    {}

    bool CanBeApplied(const std::list<std::shared_ptr<Node>>& nodes) const;
    const std::unordered_set<NodeTag>& GetResolvedTags() const { return resolved_tags_; }
private:
    std::vector<Filter<NodeTag>> rule_;
    std::unordered_set<NodeTag> resolved_tags_;
};
