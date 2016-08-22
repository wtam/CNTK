#pragma once

#include <string>

namespace Microsoft { namespace MSR { namespace CNTK {
class ComputationNodeBase;
}}}

enum class NodeTag
{
    AveragePooling,
    BatchNorm,
    Convolution,
    ElementTimes,
    Eltwise,
    InnerProduct,
    InputValue,
    LearnableParameter,
    MaxPooling,
    Plus,
    Pooling,
    ReLU,
    Scale,
    ScaleParam,
    Sigmoid,
    Times,
    IsLayer, // Indicating whether node can be converted to Caffe layer.
    HasBiasTerm
};

NodeTag GetTag(const Microsoft::MSR::CNTK::ComputationNodeBase& node);

std::string ToString(const NodeTag& tag);