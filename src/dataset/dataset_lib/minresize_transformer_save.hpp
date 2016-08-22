#pragma once

#include "base_transformer_save.hpp"

namespace cv
{
  class Mat;
}

// Save transformer which resizes minimal image dimension to predefined value
// preserving the aspect ratio.
class MinResizeTransformerSave : public BaseTransformerSave<MinResizeTransformerSave>
{
public:
  MinResizeTransformerSave(const TransformParameterSave& param);

  static const char* GetTypeString() { return "MinResize"; };

  virtual void Transform(const cv::Mat* in_image, cv::Mat* out_image) override;

private:
  // Output dimension of minimal input dimension.
  int min_resize_;
  // Interpolation to use during resize.
  int out_interpolation_;
};