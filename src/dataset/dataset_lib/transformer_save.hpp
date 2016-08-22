#pragma once

#include <memory>

class TransformParameterSave;
namespace cv
{
  class Mat;
}

// Base class for all transformers used during dataset saving.
class ITransformerSave
{
public:
  virtual ~ITransformerSave() { }
  virtual void Transform(const cv::Mat* in_image, cv::Mat* out_image) = 0;
};

// Factory method.
std::unique_ptr<ITransformerSave> CreateTransformerSave(const TransformParameterSave& param);
