#pragma once

#include "base_transformer_save.hpp"

namespace cv
{
  class Mat;
}

// Save transformed which resizes input image to predefined dimensions.
class FixedResizeTransformerSave : public BaseTransformerSave<FixedResizeTransformerSave>
{
public:
  FixedResizeTransformerSave(const TransformParameterSave& param);

  static const char* GetTypeString() { return "FixedResize"; };

  virtual void Transform(const cv::Mat* in_image, cv::Mat* out_image) override;

private:
  // Height of the resized image.
  int height_;
  // Width of the resized image.
  int width_;
  // Interpolation to use in resizing.
  int out_interpolation_;
};