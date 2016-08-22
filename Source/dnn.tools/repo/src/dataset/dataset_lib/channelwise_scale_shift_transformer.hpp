#pragma once

#include "base_transformer.hpp"

#include <vector>

class ChannelwiseScaleShiftTransformer : public BaseTransformer<ChannelwiseScaleShiftTransformer>
{
public:
  static const char* GetTypeString() { return "channelwise_scale_shift"; }
  ChannelwiseScaleShiftTransformer(const TransformParameter& param);
  virtual void GetTransformedSizeImpl(int width, int height, int& newWidth, int& newHeight) override;
  virtual void TransformImpl(std::vector<TransformableChannelset*>& channelsets) override;

private:
  std::vector<float> scales_;
  std::vector<float> shifts_;
};