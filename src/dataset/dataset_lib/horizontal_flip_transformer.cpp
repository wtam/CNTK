#include "horizontal_flip_transformer.hpp"

#include "base_transformer.hpp"
#include "check.hpp"
#include "ds_load_params.hpp"

#include <opencv2/core/core.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

using namespace std;

HorizontalFlipTransformer::HorizontalFlipTransformer(const TransformParameter& param)
    : BaseTransformer(param), gen_(mt19937(random_device()()))
{
  CHECK(param.type() == GetType());

  flip_channel_values_.assign(param.target_size(), false);
  int flip_channel_values_count = 0;
  if (param.horizontal_flip_param().flip_channel_values_size() > 0)
  {
    for (int i = 0; i < param.horizontal_flip_param().flip_channel_values_size(); ++i)
    {
      flip_channel_values_[i] = param.horizontal_flip_param().flip_channel_values(i);
      if (flip_channel_values_[i])
      {
        flip_channel_values_count++;
      }
    }
  }

  coef_.resize(flip_channel_values_count);
}

void HorizontalFlipTransformer::GetTransformedSizeImpl(int width, int height, int& newWidth, int& newHeight)
{
  newWidth = width;
  newHeight = height;
}

// Add c-th channel of matrix A multiplied by scalar a to matrix B.
void Accumulate(const cv::Mat& A, int c, double a, cv::Mat& B)
{
  CHECK(A.rows == B.rows);
  CHECK(A.cols == B.cols);
  CHECK(c < A.channels());
  CHECK(B.channels() == 1);
  CHECK(A.depth() == CV_32F);
  CHECK(B.depth() == CV_32F);
  const float* src_ptr = A.ptr<float>(0) + c;
  float* dst_ptr = B.ptr<float>(0);
  for (int i = 0; i < A.rows * A.cols; i++, src_ptr += A.channels(), dst_ptr++)
  {
    *dst_ptr += static_cast<float>(*src_ptr * a);
  }
}

// Add matrix A multiplied by scalar a to c-th channel of matrix B.
void Accumulate(const cv::Mat& A, double a, cv::Mat& B, int c)
{
  CHECK(A.rows == B.rows);
  CHECK(A.cols == B.cols);
  CHECK(A.channels() == 1);
  CHECK(c < B.channels());
  CHECK(A.depth() == CV_32F);
  CHECK(B.depth() == CV_32F);
  const float* src_ptr = A.ptr<float>(0);
  float* dst_ptr = B.ptr<float>(0) + c;
  for (int i = 0; i < A.rows * A.cols; i++, src_ptr++, dst_ptr += B.channels())
  {
    *dst_ptr += static_cast<float>(*src_ptr * a);
  }
}

void HorizontalFlipTransformer::TransformImpl(vector<TransformableChannelset*>& channelsets)
{
  if (!coin_(gen_))
  {
    // We only want to flip with probability 0.5.
    return;
  }

  for (const auto& channelset : channelsets)
  {
    const int height = channelset->Height();
    const int width = channelset->Width();
    const int channels = channelset->Channels();

    cv::Mat orig_img(height, width, CV_32FC(channels), channelset->GetFinalMemory());
    cv::Mat flipped_img(height, width, CV_32FC(channels), channelset->GetWorkspaceMemory());
    cv::flip(orig_img, flipped_img, 1); // flipCode == 1 means horizontal flipping.
    channelset->Swap();
  }

  if (coef_.empty())
  {
    // No values to modify, we're done.
    return;
  }

  // Check that the total number of channels matches the number of flip_channel_values flags
  // given in configuration file.
  int total_channel_count = 0;
  for_each(channelsets.cbegin(), channelsets.cend(),
    [&total_channel_count](const TransformableChannelset* cs) { total_channel_count += cs->Channels(); });
  CHECK(flip_channel_values_.size() == total_channel_count);

  // The remaining code in this function computes the following:
  // 1) Select a random unit (row-)vector a = [a_1, ..., a_d], where d is the number
  //    of channels with flip_channel_values: true.
  // 2) For each spatial position (pixel):
  //    - Let x = [x_1, ..., x_d] be the values of current pixel in d selected channels.
  //    - Apply linear transformation given by
  //        x <-- x * (I - 2 * a^T * a)
  //     which effectively flips x accross the hyperplane perpendicular to a passing
  //     through the origin.

  // Generate a random unit vector a.
  generate_n(coef_.begin(), coef_.size(), [this]() { return normal_(gen_); });
  const double norm = sqrt(inner_product(coef_.cbegin(), coef_.cend(), coef_.cbegin(), 0.0));
  transform(coef_.cbegin(), coef_.cend(), coef_.begin(), [&norm](const double& x) { return x / norm; });

  // Compute weighted sum of channels of interest (z <-- x * a^T).
  cv::Mat sum;
  int current_channel = 0;
  int processed_channels = 0;
  for (const auto& channelset : channelsets)
  {
    const int height = channelset->Height();
    const int width = channelset->Width();
    const int channels = channelset->Channels();

    for (int c = 0; c < channels; c++, current_channel++)
    {
      if (flip_channel_values_[current_channel])
      {
        if (processed_channels == 0)
        {
          // Initialize sum, use this channelset's workspace memory.
          sum = cv::Mat(height, width, CV_32FC1, channelset->GetWorkspaceMemory());
          CHECK(sum.isContinuous());
          sum = cv::Scalar(0);
        }

        // Update sum.
        cv::Mat img(height, width, CV_32FC(channels), channelset->GetFinalMemory());
        Accumulate(img, c, coef_[processed_channels], sum);
        processed_channels++;
      }
    }
  }

  // Update channels of interest (x <-- x - 2 * z * a).
  current_channel = 0;
  processed_channels = 0;
  for (const auto& channelset : channelsets)
  {
    const int height = channelset->Height();
    const int width = channelset->Width();
    const int channels = channelset->Channels();

    for (int c = 0; c < channels; c++, current_channel++)
    {
      if (flip_channel_values_[current_channel])
      {
        cv::Mat img(height, width, CV_32FC(channels), channelset->GetFinalMemory());
        Accumulate(sum, -2 * coef_[processed_channels], img, c);
        processed_channels++;
      }
    }
  }
}
