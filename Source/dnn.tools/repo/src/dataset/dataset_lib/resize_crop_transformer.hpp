#pragma once

#include "base_transformer.hpp"

#include <random>

class ResizeCropTransformer : public BaseTransformer<ResizeCropTransformer>
{
public:
  static const char* GetTypeString() { return "resize_crop"; }
  ResizeCropTransformer(const TransformParameter& param);
  virtual void GetTransformedSizeImpl(int width, int height, int& newWidth, int& newHeight) override;
  virtual int GetRequiredWorkspaceMemoryImpl(int width, int height, int channels) override;
  virtual void TransformImpl(std::vector<TransformableChannelset*>& channelsets) override;

private:
  void SizesAfterMinResize(int height, int width, int& new_height, int& new_width);

  std::mt19937 gen_;
  std::uniform_real_distribution<double> area_fraction_gen_;
  std::uniform_real_distribution<double> aspect_ratio_gen_;
  std::bernoulli_distribution coin_;
  int crop_size_;
};