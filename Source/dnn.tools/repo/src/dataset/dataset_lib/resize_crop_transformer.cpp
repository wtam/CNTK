#include "resize_crop_transformer.hpp"
#include "check.hpp"

#include "ds_load_params.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <random>
#include <string>

using namespace std;

ResizeCropTransformer::ResizeCropTransformer(const TransformParameter& param)
    : BaseTransformer(param), gen_(mt19937(random_device()()))
{
  CHECK(param.type() == GetType(),
    "Invalid type in ResizeCropTransformer %s.", param.type().c_str());

  crop_size_ = param.resize_crop_param().crop_size();
  const float min_area_fraction
    = param.resize_crop_param().min_area_fraction();
  const float max_area_fraction
    = param.resize_crop_param().max_area_fraction();
  const float min_aspect_ratio
    = param.resize_crop_param().min_aspect_ratio();
  const float max_aspect_ratio
    = param.resize_crop_param().max_aspect_ratio();

  area_fraction_gen_ = uniform_real_distribution<>(
    min_area_fraction, max_area_fraction);
  aspect_ratio_gen_ = uniform_real_distribution<>(
    min_aspect_ratio, max_aspect_ratio);
  coin_ = bernoulli_distribution(0.5);
}

void ResizeCropTransformer::GetTransformedSizeImpl(int /*width*/, int /*height*/, int& newWidth, int& newHeight)
{
  newWidth = crop_size_;
  newHeight = crop_size_;
}

int ResizeCropTransformer::GetRequiredWorkspaceMemoryImpl(int width, int height, int channels)
{
  // Size of the temporary image in case we fall back to simple scaling.
  int new_width, new_height;
  SizesAfterMinResize(height, width, new_height, new_width);

  // Return the maximum of the fallback and non-fallback case.
  return channels * max(height * width, new_height * new_width);
}

void ResizeCropTransformer::TransformImpl(vector<TransformableChannelset*>& channelsets)
{
  const int c_maxAttampts = 10;

  CHECK(channelsets.size() > 0, "ResizeCropTransformer expects at least one channelset");

  const int height = channelsets[0]->Height();
  const int width = channelsets[0]->Width();

  // We require that all channelsets have the same spatial size.
  for (int i = 1; i < static_cast<int>(channelsets.size()); ++i)
  {
    CHECK(channelsets[i]->Height() == height,
      "ResizeCropTransformer: all target channelsets need to have the same height");
    CHECK(channelsets[i]->Width() == width,
      "ResizeCropTransformer: all target channelsets need to have the same width");
  }

  for (int attempt = 0; attempt < c_maxAttampts; ++attempt) {
    const int area = height * width;
    const int targetArea = static_cast<int>(area * area_fraction_gen_(gen_) + 1);

    const double aspectRatio = aspect_ratio_gen_(gen_);
    int w = static_cast<int>(sqrt(targetArea * aspectRatio) + 0.5);
    int h = static_cast<int>(sqrt(targetArea / aspectRatio) + 0.5);

    if (coin_(gen_)) {
      swap(w, h);
    }

    if (h <= height && w <= width) {
      for (int i = 0; i < static_cast<int>(channelsets.size()); ++i) {
        TransformableChannelset* channelset = channelsets[i];
        const int channels = channelset->Channels();

        // Crop.
        cv::Mat origImage(height, width, CV_32FC(channels), channelset->GetFinalMemory());
        cv::Mat croppedImage(h, w, CV_32FC(channels), channelset->GetWorkspaceMemory());
        cv::Rect myROI(rand() % (width - w + 1), rand() % (height - h + 1), w, h);
        cv::Mat roiMat(origImage, myROI);
        roiMat.copyTo(croppedImage);

        // Resize.
        cv::Mat resizedImage(crop_size_, crop_size_, CV_32FC(channels), channelset->GetFinalMemory());
        cv::resize(croppedImage, resizedImage, resizedImage.size(), 0, 0, cv::INTER_CUBIC);
        channelset->SetWidth(crop_size_);
        channelset->SetHeight(crop_size_);
      }

      return;
    }
  }

  // Fallback.
  // Scale (preserving aspect ratio) so that the shorter side is crop_size_.
  int newWidth, newHeight;
  SizesAfterMinResize(height, width, newHeight, newWidth);

  for (int i = 0; i < static_cast<int>(channelsets.size()); ++i) {
    TransformableChannelset* channelset = channelsets[i];
    const int channels = channelset->Channels();

    cv::Mat origImage(height, width, CV_32FC(channels), channelset->GetFinalMemory());
    cv::Mat resizedImage(newHeight, newWidth, CV_32FC(channels), channelset->GetWorkspaceMemory());
    cv::resize(origImage, resizedImage, resizedImage.size(), 0, 0, cv::INTER_CUBIC);

    // Take central crop.
    cv::Mat croppedImage(crop_size_, crop_size_, CV_32FC(channels), channelset->GetFinalMemory());
    cv::Rect myROI((newWidth - crop_size_) / 2, (newHeight - crop_size_) / 2, crop_size_, crop_size_);
    cv::Mat roiMat(resizedImage, myROI);
    roiMat.copyTo(croppedImage);

    channelset->SetWidth(crop_size_);
    channelset->SetHeight(crop_size_);
  }
}

void ResizeCropTransformer::SizesAfterMinResize(int height, int width, int& new_height, int& new_width)
{
  if (width > height) {
    new_width = (width * crop_size_ + height - 1) / height;
    new_height = crop_size_;
  }
  else {
    new_width = crop_size_;
    new_height = (height * crop_size_ + width - 1) / width;
  }
}
