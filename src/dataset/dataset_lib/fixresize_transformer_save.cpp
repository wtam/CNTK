#include "fixresize_transformer_save.hpp"
#include "check.hpp"

#include "ds_save_params.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

FixedResizeTransformerSave::FixedResizeTransformerSave(const TransformParameterSave& param)
{
  CHECK(param.type() == FixedResizeTransformerSave::GetTypeString(),
    "Invalid param type in FixedResizeTransformerSave: %s", param.type().c_str());
  CHECK(param.has_fixed_resize_param(), "Missing fixed resize param.");
  height_ = param.fixed_resize_param().height();
  width_ = param.fixed_resize_param().width();
  if (param.fixed_resize_param().out_interpolation() == OutInterpolation::NN)
  {
    out_interpolation_ = cv::INTER_NEAREST;
  }
  else if (param.fixed_resize_param().out_interpolation() == OutInterpolation::LINEAR)
  {
    out_interpolation_ = cv::INTER_LINEAR;
  }
  else
  {
    CHECK(false, "Unexpected interpolation.");
  }
}

void FixedResizeTransformerSave::Transform(const cv::Mat* in_image, cv::Mat* out_image)
{
  out_image->create(height_, width_, in_image->type());
  cv::resize(*in_image, *out_image, out_image->size(), 0, 0, out_interpolation_);
}