#include "minresize_transformer.hpp"
#include "check.hpp"

#include "ds_load_params.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <algorithm>

using namespace std;

MinResizeTransformer::MinResizeTransformer(const TransformParameter& param) : BaseTransformer(param)
{
  CHECK(param.type() == GetType(), "Invalid type in MinResizeTransformer: %s", param.type().c_str());
  CHECK(param.has_min_resize_param(), "Min resize params missing in MinResizeTransformer.");
  min_resize_ = param.min_resize_param().min_size();
}

void MinResizeTransformer::GetScales(int width, int height, float& scale_x, float& scale_y, float& scale)
{
  scale_x = 1.0f * min_resize_ / width;
  scale_y = 1.0f * min_resize_ / height;
  scale = max(scale_x, scale_y);
}

void MinResizeTransformer::GetTransformedSizeImpl(int width, int height, int& new_width, int& new_height)
{
  float scale_x, scale_y, scale;
  GetScales(width, height, scale_x, scale_y, scale);
  if (scale > 1.0f)
  {
    new_width = (scale_x >= scale_y) ? min_resize_ : static_cast<int>(width * scale);
    new_height = (scale_y >= scale_x) ? min_resize_ : static_cast<int>(height * scale);
  }
  else
  {
    // No need to resize, both dimensions are greater than minimally required.
    new_width = width;
    new_height = height;
  }
}

int MinResizeTransformer::GetRequiredWorkspaceMemoryImpl(int width, int height, int channels)
{
  float scale_x, scale_y, scale;
  GetScales(width, height, scale_x, scale_y, scale);
  if (scale > 1.0f)
  {
    const int new_width = (scale_x >= scale_y) ? min_resize_ : static_cast<int>(width * scale);
    const int new_height = (scale_y >= scale_x) ? min_resize_ : static_cast<int>(height * scale);
    return channels * new_height * new_width;
  }
  else
  {
    return 0;
  }
}

void MinResizeTransformer::TransformImpl(vector<TransformableChannelset*>& channelsets)
{
  // Currently we expect just one channelset.
  CHECK(channelsets.size() == 1, "MinResizeTransformer expects one channelset, provided: %u", channelsets.size());
  TransformableChannelset* channelset = channelsets[0];

  int height = channelset->Height();
  int width = channelset->Width();
  int channels = channelset->Channels();

  float scale_x, scale_y, scale;
  GetScales(width, height, scale_x, scale_y, scale);
  if (scale > 1.0f)
  {
    // We need to rescale.
    int new_width = (scale_x >= scale_y) ? min_resize_ : static_cast<int>(width * scale);
    int new_height = (scale_y >= scale_x) ? min_resize_ : static_cast<int>(height * scale);

    cv::Mat orig_image(height, width, CV_32FC(channels), channelset->GetFinalMemory());
    cv::Mat resized_image(new_height, new_width, CV_32FC(channels), channelset->GetWorkspaceMemory());

    cv::resize(orig_image, resized_image, resized_image.size(), 0, 0, cv::INTER_NEAREST);

    channelset->Swap();

    channelset->SetWidth(new_width);
    channelset->SetHeight(new_height);
  }
}
