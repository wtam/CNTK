#pragma once

#include "cntk_includes.h"
#include <memory>
#include <stack>
#include <unordered_set>

struct CntkPostfixIteratorState;

class CntkPostfixIterator
{
public:
    CntkPostfixIterator(const Microsoft::MSR::CNTK::ComputationNetworkPtr& net);
    ~CntkPostfixIterator();
    const Microsoft::MSR::CNTK::ComputationNodeBasePtr GetNext();
    bool HasNext() const;
private:
    std::unique_ptr<CntkPostfixIteratorState> CreateIteratorState(
        const Microsoft::MSR::CNTK::ComputationNodeBasePtr& node);
    std::stack<std::unique_ptr<CntkPostfixIteratorState>> state_;
    std::unordered_set<int64_t> visited_nodes_;
};