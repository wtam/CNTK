#include "channelwise_scale_shift_transformer.hpp"

#include "base_transformer.hpp"
#include "check.hpp"
#include "ds_load_params.hpp"

#include <opencv2/core/core.hpp>

using namespace std;

ChannelwiseScaleShiftTransformer::ChannelwiseScaleShiftTransformer(const TransformParameter& param)
    : BaseTransformer(param)
{
  CHECK(param.type() == GetType(), "Invalid type in ChannelwiseScaleShiftTransformer %s.", param.type().c_str());
  CHECK(param.channelwise_scale_shift_param().shift_size() == param.channelwise_scale_shift_param().scale_size(),
    "The number of shift values must be equal to the number of scale values");
  for (int i = 0; i < param.channelwise_scale_shift_param().shift_size(); ++i) {
    shifts_.push_back(param.channelwise_scale_shift_param().shift(i));
  }
  for (int i = 0; i < param.channelwise_scale_shift_param().scale_size(); ++i) {
    scales_.push_back(param.channelwise_scale_shift_param().scale(i));
  }
}

void ChannelwiseScaleShiftTransformer::GetTransformedSizeImpl(int width, int height, int& newWidth, int& newHeight)
{
  newWidth = width;
  newHeight = height;
}

int ChannelwiseScaleShiftTransformer::GetRequiredWorkspaceMemoryImpl(int /*width*/, int /*height*/, int /*channels*/)
{
  return 0;
}

void ChannelwiseScaleShiftTransformer::TransformImpl(std::vector<TransformableChannelset*>& channelsets)
{
  int total_channel_count = 0;
  for (const auto& channelset : channelsets) {
    total_channel_count += channelset->Channels();
  }
  CHECK(total_channel_count == static_cast<int>(shifts_.size()),
    "The number of shift values must be equal to the number of input channels");
  CHECK(total_channel_count == static_cast<int>(scales_.size()),
    "The number of scale values must be equal to the number of input channels");

  int coef_vector_offset = 0;

  for (const auto& channelset : channelsets) {
    const int height = channelset->Height();
    const int width = channelset->Width();
    const int channels = channelset->Channels();

    cv::Mat orig_img(height, width, CV_32FC(channels), channelset->GetFinalMemory());
    for (int h = 0; h < orig_img.rows; ++h) {
      float* row_ptr = orig_img.ptr<float>(h);
      for (int w = 0; w < orig_img.cols; ++w) {
        for (int c = 0; c < channels; ++c, ++row_ptr) {
          *row_ptr = scales_[coef_vector_offset + c] * (*row_ptr) + shifts_[coef_vector_offset + c];
        }
      }
    }

    coef_vector_offset += channels;
  }
}
