#include "cntk_node_wrappers.h"
#include "check.hpp"
#include "ConvolutionalNodes.h"

using namespace std;

namespace Microsoft { namespace MSR { namespace CNTK {

template <typename Base>
ConvolutionNodeBaseWrapper<Base>::ConvolutionNodeBaseWrapper(
    DEVICEID_TYPE deviceId, const wstring& name)
    : Base(deviceId, name)
{}

template <typename Base>
shared_ptr<ConvolutionNodeBaseWrapper<Base>> ConvolutionNodeBaseWrapper<Base>::CreateWrapper(
    DEVICEID_TYPE deviceId, const Base& node)
{
    auto wrapper = make_shared<ConvolutionNodeBaseWrapper<Base>>(deviceId, node.GetName());
    const ConvolutionNodeBase<float>* base = dynamic_cast<const ConvolutionNodeBase<float>*>(&node);
    CHECK(base != nullptr);
    base->CopyTo(wrapper, node.GetName(), Microsoft::MSR::CNTK::CopyNodeFlags::copyNodeValue);
    return wrapper;
}

template <typename Base>
TensorShape ConvolutionNodeBaseWrapper<Base>::GetKernelShape() const { return m_kernelShape; }

template <typename Base>
TensorShape ConvolutionNodeBaseWrapper<Base>::GetStrideShape() const { return m_stride; }

template <typename Base>
PoolKind ConvolutionNodeBaseWrapper<Base>::GetPoolKind() const { return m_poolKind; }

template <typename Base>
ImageLayoutKind ConvolutionNodeBaseWrapper<Base>::GetImageLayoutKind() const { return m_imageLayout; }

template <typename Base>
const std::vector<bool>& ConvolutionNodeBaseWrapper<Base>::GetAutoPad() const { return m_autoPad; }

template <typename Base>
TensorShape ConvolutionNodeBaseWrapper<Base>::GetLowerPad() const { return m_lowerPad; }

template <typename Base>
TensorShape ConvolutionNodeBaseWrapper<Base>::GetUpperPad() const { return m_upperPad; }

template <typename Base>
ConvolutionEngine<float>* ConvolutionNodeBaseWrapper<Base>::GetConvEngine() const { return m_convEng.get(); }

template <typename Base>
PoolingNodeBaseWrapper<Base>::PoolingNodeBaseWrapper(DEVICEID_TYPE deviceId, const wstring& name)
    : Base(deviceId, name)
{}

template <typename Base>
shared_ptr<PoolingNodeBaseWrapper<Base>>
PoolingNodeBaseWrapper<Base>::CreateWrapper(DEVICEID_TYPE deviceId, const Base& node)
{
    auto wrapper = make_shared<PoolingNodeBaseWrapper<Base>>(deviceId, node.GetName());
    const PoolingNodeBase<float>* pool = dynamic_cast<const PoolingNodeBase<float>*>(&node);
    CHECK(nullptr != pool);
    pool->CopyTo(wrapper, node.GetName(), CopyNodeFlags::copyNodeValue);
    return wrapper;
}

template <typename Base>
size_t PoolingNodeBaseWrapper<Base>::GetWindowWidth() const { return m_windowWidth; }

template <typename Base>
size_t PoolingNodeBaseWrapper<Base>::GetWindowHeight() const { return m_windowHeight; }

template <typename Base>
size_t PoolingNodeBaseWrapper<Base>::GetHorizontalSubsample() const { return m_horizontalSubsample; }

template <typename Base>
size_t PoolingNodeBaseWrapper<Base>::GetVerticalSubsample() const { return m_verticalSubsample; }

template <typename Base>
ImageLayoutKind PoolingNodeBaseWrapper<Base>::GetImageLayoutKind() const { return m_imageLayoutKind; }


template class PoolingNodeBaseWrapper<MaxPoolingNode<float>>;
template class PoolingNodeBaseWrapper<AveragePoolingNode<float>>;
template class ConvolutionNodeBaseWrapper<ConvolutionNode<float>>;
template class ConvolutionNodeBaseWrapper<PoolingNode<float>>;

}}}