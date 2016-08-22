#pragma once

#include <map>
#include <string>

#include "cntk_includes.h"

enum class NodeAttribute
{
    Bias,
    Input,
    InvStdDev,
    Mean,
    Operation,
    Scale,
    Weights
};

typedef std::map<NodeAttribute, Microsoft::MSR::CNTK::ComputationNodeBasePtr> NodeAttributes;