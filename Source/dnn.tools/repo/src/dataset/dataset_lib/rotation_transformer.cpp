#include "rotation_transformer.hpp"

#include "base_transformer.hpp"
#include "check.hpp"
#include "ds_load_params.hpp"

#include <opencv2/core/core.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <stdint.h>

using namespace std;

RotationTransformer::RotationTransformer(const TransformParameter& param)
    : BaseTransformer(param), gen_(mt19937(random_device()()))
{
  CHECK(param.type() == GetType());
  coef_.resize(dim_);
  for (int i = 0; i < dim_; i++)
  {
    coef_[i].resize(dim_);
  }
}

void RotationTransformer::GetTransformedSizeImpl(int width, int height, int& newWidth, int& newHeight)
{
  newWidth = width;
  newHeight = height;
}

void ComputeLinearCombination(const vector<cv::Mat>& in, const vector<double>& coef, cv::Mat& out)
{
  CHECK(in.size() == coef.size());
  CHECK(!in.empty());
  CHECK(out.depth() == CV_32F);
  CHECK(in[0].depth() == CV_32F);
  out = coef[0] * in[0];
  for (int i = 1; i < coef.size(); ++i)
  {
    CHECK(in[i].depth() == CV_32F);
    out += coef[i] * in[i];
  }
}

void RotationTransformer::TransformImpl(vector<TransformableChannelset*>& channelsets)
{
  // Check that input channelsets contain correct number of channels.
  int total_channel_count = 0;
  for_each(channelsets.cbegin(), channelsets.cend(),
    [&total_channel_count](const TransformableChannelset* cs) { total_channel_count += cs->Channels(); });
  CHECK(total_channel_count == dim_, "Rotation transformer: invalid number of input channels");

  // Assign random rotation matrix to coef_.
  CHECK(dim_ == 2);
  const double angle = 4 * asin(1) * uniform_(gen_);
  coef_[0][0] = cos(angle);
  coef_[0][1] = -sin(angle);
  coef_[1][0] = -coef_[0][1];
  coef_[1][1] = coef_[0][0];

  // Split multi-channel channelsets.
  vector<cv::Mat> chs;
  for (const auto& channelset : channelsets)
  {
    const int height = channelset->Height();
    const int width = channelset->Width();
    const int channels = channelset->Channels();

    if (channels == 1)
    {
      chs.push_back(cv::Mat(height, width, CV_32FC1, channelset->GetFinalMemory()));
    }
    else
    {
      cv::Mat img(height, width, CV_32FC(channels), channelset->GetFinalMemory());
      vector<cv::Mat> img_chs(channels);
      int64_t offset = 0;
      for (int c = 0; c < channels; c++)
      {
        img_chs[c] = cv::Mat(height, width, CV_32FC1, channelset->GetWorkspaceMemory() + offset);
        offset += img_chs[c].total();
      }
      cv::split(img, img_chs);
      chs.insert(chs.end(), img_chs.begin(), img_chs.end());
      channelset->Swap();
    }
  }

  // Compute linear combinations and store the result in workspace memory.
  int curr_channel = 0;
  for (const auto& channelset : channelsets)
  {
    const int height = channelset->Height();
    const int width = channelset->Width();
    const int channels = channelset->Channels();

    if (channels == 1)
    {
      cv::Mat img(height, width, CV_32FC1, channelset->GetWorkspaceMemory());
      ComputeLinearCombination(chs, coef_[curr_channel++], img);
    }
    else
    {
      int64_t offset = 0;
      for (int c = 0; c < channels; c++, curr_channel++)
      {
        cv::Mat img(height, width, CV_32FC1, channelset->GetWorkspaceMemory() + offset);
        ComputeLinearCombination(chs, coef_[curr_channel], img);
        offset += img.total();
      }
    }
  }

  // Merge multi-channel channelsets and move result to final memory.
  for (const auto& channelset : channelsets)
  {
    const int height = channelset->Height();
    const int width = channelset->Width();
    const int channels = channelset->Channels();

    if (channels == 1)
    {
      channelset->Swap();
    }
    else
    {
      // Merge channels and move to final memory.
      cv::Mat img(height, width, CV_32FC(channels), channelset->GetFinalMemory());
      vector<cv::Mat> img_chs(channels);
      int64_t offset = 0;
      for (int c = 0; c < channels; c++)
      {
        img_chs[c] = cv::Mat(height, width, CV_32FC1, channelset->GetWorkspaceMemory() + offset);
        offset += img_chs[c].total();
      }
      cv::merge(img_chs, img);
    }
  }
}
