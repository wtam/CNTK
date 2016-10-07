#include "color_jitter_transformer.hpp"
#include "check.hpp"

#include "ds_load_params.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <random>
#include <string>

using namespace std;

/*static*/ const double ColorJitterTransformer::c_blue = 0.114;
/*static*/ const double ColorJitterTransformer::c_green = 0.587;
/*static*/ const double ColorJitterTransformer::c_red = 0.299;

ColorJitterTransformer::ColorJitterTransformer(const TransformParameter& param)
    : BaseTransformer(param), uniform_gen_(uniform_real_distribution<>()),
    gen_(mt19937(random_device()()))
{
  CHECK(param.type() == GetType(),
    "Invalid type in ColorJitterTransformer %s.", param.type().c_str());

  brightness_ = param.color_jitter_param().brightness();
  contrast_ = param.color_jitter_param().contrast();
  saturation_ = param.color_jitter_param().saturation();
  has_upper_bound_ = param.color_jitter_param().has_upper_bound();
  if (has_upper_bound_) {
    upper_bound_ = param.color_jitter_param().upper_bound();
  }
  has_lower_bound_ = param.color_jitter_param().has_lower_bound();
  if (has_lower_bound_) {
    lower_bound_ = param.color_jitter_param().lower_bound();
  }

}

void ColorJitterTransformer::GetTransformedSizeImpl(int width, int height, int& newWidth, int& newHeight)
{
  newWidth = width;
  newHeight = height;
}

int ColorJitterTransformer::GetRequiredWorkspaceMemoryImpl(int width, int height, int channels)
{
  return (channels + 1) * height * width;
}

void ColorJitterTransformer::TransformImpl(vector<TransformableChannelset*>& channelsets)
{
  // Currently we expect just one channelset.
  CHECK(channelsets.size() == 1,
    "ColorJitterTransformer expects one channelset, provided: %u", channelsets.size());
  TransformableChannelset* channelset = channelsets[0];

  enum Transformation { BRIGHTNESS, CONTRAST, SATURATION };
  vector<Transformation> transformations = { BRIGHTNESS, CONTRAST, SATURATION };
  shuffle(transformations.begin(), transformations.end(), gen_);
  for (auto t : transformations) {
    switch (t) {
    case BRIGHTNESS:
      Brightness(channelset, 1 + brightness_ * (2 * uniform_gen_(gen_) - 1));
      break;
    case CONTRAST:
      Contrast(channelset, 1 + contrast_ * (2 * uniform_gen_(gen_) - 1));
      break;
    case SATURATION:
      Saturation(channelset, 1 + saturation_ * (2 * uniform_gen_(gen_) - 1));
      break;
    default:
      CHECK(false, "ColorJitterTransformer: unknown transformation type");
    }
  }
  Bound(channelset);
}

// Convert to grayscale, return result as a single-channel image in workspace memory.
cv::Mat ColorJitterTransformer::Grayscale(TransformableChannelset* channelset) {
  const int height = channelset->Height();
  const int width = channelset->Width();
  const int channels = channelset->Channels();
  cv::Mat orig_img(height, width, CV_32FC(channels), channelset->GetFinalMemory());
  cv::Mat gs_img(height, width, CV_32FC1, channelset->GetWorkspaceMemory());
  cv::cvtColor(orig_img, gs_img, CV_BGR2GRAY);
  return gs_img;
}

void ColorJitterTransformer::Brightness(TransformableChannelset* channelset, double alpha) {
  const int height = channelset->Height();
  const int width = channelset->Width();
  const int channels = channelset->Channels();
  cv::Mat img(height, width, CV_32FC(channels), channelset->GetFinalMemory());
  img *= alpha;
}

void ColorJitterTransformer::Contrast(TransformableChannelset* channelset, double alpha) {
  const double gs_mean = cv::mean(Grayscale(channelset))[0];
  const int height = channelset->Height();
  const int width = channelset->Width();
  const int channels = channelset->Channels();
  cv::Mat img(height, width, CV_32FC(channels), channelset->GetFinalMemory());
  img *= alpha;
  img += (1 - alpha) * gs_mean;
}

void ColorJitterTransformer::Saturation(TransformableChannelset* channelset, double alpha) {
  cv::Mat gs_img = Grayscale(channelset);
  const int height = channelset->Height();
  const int width = channelset->Width();
  const int channels = channelset->Channels();

  // Replicate grayscale image into multiple channels. Note: resulting image resides after gs_img in workspace memory.
  vector<cv::Mat> channel_imgs(channels, gs_img);
  cv::Mat gs_multichannel_img(height, width, CV_32FC(channels), channelset->GetWorkspaceMemory() + gs_img.total());
  cv::merge(channel_imgs, gs_multichannel_img);

  cv::Mat img(height, width, CV_32FC(channels), channelset->GetFinalMemory());
  img *= alpha;
  img += (1 - alpha) * gs_multichannel_img;
}

void ColorJitterTransformer::Bound(TransformableChannelset* channelset) {
  const int height = channelset->Height();
  const int width = channelset->Width();
  const int channels = channelset->Channels();
  cv::Mat img(height, width, CV_32FC(channels), channelset->GetFinalMemory());
  if (has_upper_bound_) {
    cv::Mat temp(height, width, CV_32FC(channels), channelset->GetWorkspaceMemory());
    temp = cv::min(img, upper_bound_);
    channelset->Swap();
  }
  if (has_lower_bound_) {
    cv::Mat temp(height, width, CV_32FC(channels), channelset->GetWorkspaceMemory());
    temp = cv::max(img, lower_bound_);
    channelset->Swap();
  }
}
