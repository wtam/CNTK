#include "alexnet_color_transformer.hpp"
#include "check.hpp"

#include "ds_load_params.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <random>
#include <string>

using namespace std;

AlexNetColorTransformer::AlexNetColorTransformer(const TransformParameter& param)
    : BaseTransformer(param), gen_(mt19937(random_device()()))
{
  CHECK(param.type() == GetType(),
    "Invalid type in AlexNetColorTransformer %s.", param.type().c_str());

  displacement_ = normal_distribution<float>(0.0f,
    param.alexnet_color_param().stdev());

  // Eignevalues and eigenvectors representing PCA of all RGB pixel values in
  // ILSVRC 2012 training set. These serve as a basis for displacing input values.
  float evec[3][3] = {
    { -0.5675f, 0.7192f, 0.4009f },
    { -0.5808f, -0.0045f, -0.8140f },
    { -0.5836f, -0.6948f, 0.4203f } };
  float eval[3][3] = {
    { 0.2175f, 0, 0 },
    { 0, 0.0188f, 0 },
    { 0, 0, 0.0045f } };
  displacement_basis_ = cv::Mat(3, 3, CV_32FC1, evec) * cv::Mat(3, 3, CV_32FC1, eval) * 255;
}

void AlexNetColorTransformer::GetTransformedSizeImpl(int width, int height, int& newWidth, int& newHeight)
{
  newWidth = width;
  newHeight = height;
}

int AlexNetColorTransformer::GetRequiredWorkspaceMemoryImpl(int width, int height, int channels)
{
  return channels * height * width;
}

void AlexNetColorTransformer::TransformImpl(vector<TransformableChannelset*>& channelsets)
{
  // Currently we expect just one channelset.
  CHECK(channelsets.size() == 1,
    "AlexNetColorTransformer expects one channelset, provided: %u", channelsets.size());
  TransformableChannelset* channelset = channelsets[0];

  int height = channelset->Height();
  int width = channelset->Width();
  int channels = channelset->Channels();
  CHECK(channels == 3, "AlexNetColorTransformer expects 3 channels, provided: %d", channels);

  cv::Mat orig_img(height, width, CV_32FC(channels), channelset->GetFinalMemory());
  float displacement_coef[3] = {
    displacement_(gen_),
    displacement_(gen_),
    displacement_(gen_) };
  const cv::Mat displacement
    = displacement_basis_ * cv::Mat(channels, 1, CV_32FC1, displacement_coef);

  cv::Mat channel_imgs[3];
  int64_t offset = 0;
  for (int i = 0; i < 3; i++)
  {
    channel_imgs[i] = cv::Mat(height, width, CV_32FC1, channelset->GetWorkspaceMemory() + offset);
    offset += channel_imgs[i].total();
  }
  cv::split(orig_img, channel_imgs);
  channel_imgs[0] += displacement.at<float>(0);
  channel_imgs[1] += displacement.at<float>(1);
  channel_imgs[2] += displacement.at<float>(2);
  cv::merge(channel_imgs, 3, orig_img);
}
