#pragma once

#include "base_transformer.hpp"

#include <opencv2/core/core.hpp>

#include <random>

class ColorJitterTransformer : public BaseTransformer<ColorJitterTransformer>
{
public:
  static const char* GetTypeString() { return "color_jitter"; }
  ColorJitterTransformer(const TransformParameter& param);
  virtual void GetTransformedSizeImpl(int width, int height, int& newWidth, int& newHeight) override;
  virtual int GetRequiredWorkspaceMemoryImpl(int width, int height, int channels) override;
  virtual void TransformImpl(std::vector<TransformableChannelset*>& channelset) override;

private:
  cv::Mat Grayscale(TransformableChannelset* channelset);
  void Brightness(TransformableChannelset* channelset, double alpha);
  void Contrast(TransformableChannelset* channelset, double alpha);
  void Saturation(TransformableChannelset* channelset, double alpha);
  void Bound(TransformableChannelset* channelset);

  // Grayscale conversion coefficients (fixed).
  static const double c_blue;
  static const double c_green;
  static const double c_red;

  float brightness_;
  float contrast_;
  float saturation_;

  bool has_upper_bound_;
  bool has_lower_bound_;
  float upper_bound_;
  float lower_bound_;

  std::mt19937 gen_;
  std::uniform_real_distribution<double> uniform_gen_;
};