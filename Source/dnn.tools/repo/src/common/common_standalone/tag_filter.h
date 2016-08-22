#pragma once

#include <unordered_set>

// Performs filtering of set of tags. Set of tags will pass the filter if it contains all required tags and doesn't
// contain any forbidden tag from filter.
template <typename Tag>
class Filter
{
public:
    Filter(const std::unordered_set<Tag>& required_tags,
        const std::unordered_set<Tag>& forbidden_tags = {}) :
        required_tags_(required_tags),
        forbidden_tags_(forbidden_tags)
    {}

    Filter(std::unordered_set<Tag>&& required_tags,
        std::unordered_set<Tag>&& forbidden_tags = {}) :
        required_tags_(move(required_tags)),
        forbidden_tags_(move(forbidden_tags))
    {}

    // Set of tags will pass the filter if it contains all required tags and
    // doesn't contain any forbidden tag from filter.
    bool HasPassedFilter(const std::unordered_set<Tag>& tags) const
    {
        for (const Tag& required : required_tags_)
        {
            if (tags.find(required) == tags.end())
            {
                return false; // One of required tags is not present.
            }
        }
        for (const Tag& forbidden : forbidden_tags_)
        {
            if (tags.find(forbidden) != tags.end())
            {
                return false; // One of forbidden tags is present.
            }
        }
        return true;
    }

private:
    std::unordered_set<Tag> required_tags_;
    std::unordered_set<Tag> forbidden_tags_;
};
