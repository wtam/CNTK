#include "minresize_transformer_save.hpp"
#include "check.hpp"

#include "ds_save_params.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <algorithm>

MinResizeTransformerSave::MinResizeTransformerSave(const TransformParameterSave& param)
{
  CHECK(param.type() == MinResizeTransformerSave::GetTypeString(),
    "Invalid type in MinResizeTransformerSave: %s", param.type().c_str());
  CHECK(param.has_min_resize_param(), "Min resize params missing.");
  min_resize_ = param.min_resize_param().size();
  if (param.min_resize_param().out_interpolation() == OutInterpolation::NN)
  {
    out_interpolation_ = cv::INTER_NEAREST;
  }
  else if (param.min_resize_param().out_interpolation() == OutInterpolation::LINEAR)
  {
    out_interpolation_ = cv::INTER_LINEAR;
  }
  else
  {
    CHECK(false, "Unexpected interpolation.");
  }
}

void MinResizeTransformerSave::Transform(const cv::Mat* in_image, cv::Mat* out_image)
{
  float scaleX = 1.0f * min_resize_ / in_image->cols;
  float scaleY = 1.0f * min_resize_ / in_image->rows;
  float scale = std::max(scaleX, scaleY);

  int new_width = (scaleX > scaleY) ? min_resize_ : static_cast<int>(in_image->cols * scale);
  int new_height = (scaleY > scaleX) ? min_resize_ : static_cast<int>(in_image->rows * scale);

  out_image->create(new_height, new_width, in_image->type());
  cv::resize(*in_image, *out_image, out_image->size(), 0, 0, out_interpolation_);
}