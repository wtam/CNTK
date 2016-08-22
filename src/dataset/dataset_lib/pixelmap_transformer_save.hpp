#pragma once

#include "base_transformer_save.hpp"

#include <unordered_map>

namespace cv
{
  class Mat;
}

// Save transformer that maps each RGB pixel to GS value. Mapping is provided
// using external file.
class PixelMapTransformerSave : public BaseTransformerSave<PixelMapTransformerSave>
{
public:
  PixelMapTransformerSave(const TransformParameterSave& param);

  static const char* GetTypeString() { return "PixelMap"; }

  virtual void Transform(const cv::Mat* in_image, cv::Mat* out_image) override;

private:
  // Defines RGB -> GS mapping.
  std::unordered_map<unsigned int, unsigned char> rgb_to_gs_map_;
};