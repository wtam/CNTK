#pragma once

#include "cntk_includes.h"
#include "ConvolutionEngine.h"
#include "ReshapingNodes.h"
#include "TensorShape.h"
#include <memory>
#include <vector>

namespace Microsoft { namespace MSR { namespace CNTK {

template <typename ConvolutionNodeBaseType>
class ConvolutionNodeBaseWrapper : public ConvolutionNodeBaseType
{
private:
    typedef ConvolutionNodeBaseType Base;
    static const std::wstring TypeName() { return Base::TypeName(); }
public:
    ConvolutionNodeBaseWrapper(DEVICEID_TYPE deviceId, const std::wstring& name);

    static std::shared_ptr<ConvolutionNodeBaseWrapper<Base>> CreateWrapper(DEVICEID_TYPE deviceId, const Base& node);

    TensorShape GetKernelShape() const;
    TensorShape GetStrideShape() const;
    PoolKind GetPoolKind() const;
    ImageLayoutKind GetImageLayoutKind() const;
    const std::vector<bool>& GetAutoPad() const;
    TensorShape GetLowerPad() const;
    TensorShape GetUpperPad() const;
    ConvolutionEngine<float>* GetConvEngine() const;
    bool IsTransposed() const;
};

template <typename PoolingNodeBaseType>
class PoolingNodeBaseWrapper : public PoolingNodeBaseType
{
private:
    typedef PoolingNodeBaseType Base;
    static const std::wstring TypeName() { return Base::TypeName(); }
public:
    PoolingNodeBaseWrapper(DEVICEID_TYPE deviceId, const std::wstring& name);
    static std::shared_ptr<PoolingNodeBaseWrapper<Base>> CreateWrapper(DEVICEID_TYPE deviceId, const Base& node);

    size_t GetWindowWidth() const;
    size_t GetWindowHeight() const;
    size_t GetHorizontalSubsample() const;
    size_t GetVerticalSubsample() const;
    ImageLayoutKind GetImageLayoutKind() const;
};

#ifdef CONVERT_CROP_NODE
template <class CropNodeBase>
class CropNodeWrapper : public CropNodeBase
{
private:
    typedef CropNodeBase Base;
    static const std::wstring TypeName() { return Base::TypeName(); }
public:
    static std::shared_ptr<CropNodeWrapper<Base>> CreateWrapper(DEVICEID_TYPE deviceId, const Base& node);
    CropNodeWrapper(DEVICEID_TYPE deviceId, const std::wstring& name);
    int GetOffsetX() const;
    int GetOffsetY() const;
};
#endif

}}}
