#include "node_tag.h"

#pragma warning( disable: 4996 )
#include "ConvolutionalNodes.h"
#include "InputAndParamNodes.h"
#include "LinearAlgebraNodes.h"
#include "NonlinearityNodes.h"
#include "TrainingNodes.h"
#pragma warning( default: 4996 )

#include <stdexcept>

namespace cntk = Microsoft::MSR::CNTK;

NodeTag GetTag(const cntk::ComputationNodeBase& node)
{
    if (dynamic_cast<const cntk::AveragePoolingNode<float>*>(&node) != nullptr)
    {
        return NodeTag::AveragePooling;
    }
    else if (dynamic_cast<const cntk::BatchNormalizationNode<float>*>(&node) != nullptr)
    {
        return NodeTag::BatchNorm;
    }
    else if (dynamic_cast<const cntk::ConvolutionNode<float>*>(&node) != nullptr)
    {
        return NodeTag::Convolution;
    }
    else if (dynamic_cast<const cntk::ElementTimesNode<float>*>(&node) != nullptr)
    {
        return NodeTag::ElementTimes;
    }
    else if (dynamic_cast<const cntk::InputValue<float>*>(&node) != nullptr)
    {
        return NodeTag::InputValue;
    }
    else if (dynamic_cast<const cntk::LearnableParameter<float>*>(&node) != nullptr)
    {
        return NodeTag::LearnableParameter;
    }
    else if (dynamic_cast<const cntk::MaxPoolingNode<float>*>(&node) != nullptr)
    {
        return NodeTag::MaxPooling;
    }
    else if (dynamic_cast<const cntk::PlusNode<float>*>(&node) != nullptr)
    {
        return NodeTag::Plus;
    }
    else if (dynamic_cast<const cntk::PoolingNode<float>*>(&node) != nullptr)
    {
        return NodeTag::Pooling;
    }
    else if (dynamic_cast<const cntk::RectifiedLinearNode<float>*>(&node) != nullptr)
    {
        return NodeTag::ReLU;
    }
    else if (dynamic_cast<const cntk::SigmoidNode<float>*>(&node) != nullptr)
    {
        return NodeTag::Sigmoid;
    }
    else if (dynamic_cast<const cntk::TimesNode<float>*>(&node) != nullptr)
    {
        return NodeTag::Times;
    }
    else
    {
        throw std::runtime_error("Unknown CNTK node");
    }
}

std::string ToString(const NodeTag& tag)
{
    switch (tag)
    {
    case NodeTag::AveragePooling:
        return "AveragePooling";
    case NodeTag::BatchNorm:
        return "BatchNorm";
    case NodeTag::Convolution:
        return "Convolution";
    case NodeTag::ElementTimes:
        return "ElementTimes";
    case NodeTag::Eltwise:
        return "Eltwise";
    case NodeTag::InnerProduct:
        return "InnerProduct";
    case NodeTag::InputValue:
        return "InputValue";
    case NodeTag::LearnableParameter:
        return "LearnableParameter";
    case NodeTag::MaxPooling:
        return "MaxPooling";
    case NodeTag::Plus:
        return "Plus";
    case NodeTag::Pooling:
        return "Pooling";
    case NodeTag::ReLU:
        return "ReLU";
    case NodeTag::Scale:
        return "Scale";
    case NodeTag::ScaleParam:
        return "ScaleParam";
    case NodeTag::Sigmoid:
        return "Sigmoid";
    case NodeTag::Times:
        return "Times";
    case NodeTag::IsLayer:
        return "IsLayer";
    case NodeTag::HasBiasTerm:
        return "+Bias";
    default:
        throw std::runtime_error("Unknown node tag");
    }
}