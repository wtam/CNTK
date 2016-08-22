#pragma once

#include "base_transformer.hpp"

// Performs random cropping of the input channelset.
class CropTransformer : public BaseTransformer<CropTransformer>
{
public:
  static const char* GetTypeString() { return "crop"; }

  CropTransformer(const TransformParameter& param);

  virtual void GetTransformedSizeImpl(int width, int height, int& new_width, int& new_height) override;

  virtual void TransformImpl(std::vector<TransformableChannelset*>& channelsets) override;

protected:

  int crop_size_;
  bool central_crop_;
};