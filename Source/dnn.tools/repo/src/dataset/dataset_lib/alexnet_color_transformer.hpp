#pragma once

#include "base_transformer.hpp"

#include <opencv2/core/core.hpp>

#include <random>

class AlexNetColorTransformer : public BaseTransformer<AlexNetColorTransformer>
{
public:
  static const char* GetTypeString() { return "alexnet_color"; }
  AlexNetColorTransformer(const TransformParameter& param);
  virtual void GetTransformedSizeImpl(int width, int height, int& newWidth, int& newHeight) override;
  virtual int GetRequiredWorkspaceMemoryImpl(int width, int height, int channels) override;
  virtual void TransformImpl(std::vector<TransformableChannelset*>& channelsets) override;

private:
  std::mt19937 gen_;
  std::normal_distribution<float> displacement_;
  cv::Mat displacement_basis_;
  float stdev_;
};