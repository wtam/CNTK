#include "crop_transformer.hpp"
#include "check.hpp"

#include "ds_load_params.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

using namespace std;

CropTransformer::CropTransformer(const TransformParameter& param) : BaseTransformer(param)
{
  CHECK(param.type() == GetType());
  CHECK(param.has_crop_param());

  crop_size_ = param.crop_param().crop_size();
  central_crop_ = param.crop_param().central_crop();
}

void CropTransformer::GetTransformedSizeImpl(int width, int height, int& new_width, int& new_height)
{
  // We expect we can crop.
  CHECK(width >= crop_size_ && height >= crop_size_);
  // After this transform dimensions are same to crop size.
  new_width = crop_size_;
  new_height = crop_size_;
}

void CropTransformer::TransformImpl(vector<TransformableChannelset*>& channelsets)
{
  CHECK(!channelsets.empty());

  const int height = channelsets[0]->Height();
  const int width = channelsets[0]->Width();

  // We require that all channelsets have the same spatial size.
  for (const auto& channelset : channelsets)
  {
    CHECK(channelset->Height() == height);
    CHECK(channelset->Width() == width);
  }

  // We expect we can crop.
  CHECK(width >= crop_size_ && height >= crop_size_, "Invalid channelset dimensions for crop");
  // Calculate max top left of crop rectangle.
  int offset_max_x = width - crop_size_;
  int offset_max_y = height - crop_size_;

  if (offset_max_x == 0 && offset_max_y == 0)
  {
    return;
  }

  int offset_x, offset_y;
  if (central_crop_) {
    // Take central crop.
    offset_x = offset_max_x / 2;
    offset_y = offset_max_y / 2;
  } else {
    // Take random crop.
    offset_x = (offset_max_x > 0) ? (rand() % offset_max_x) : 0;
    offset_y = (offset_max_y > 0) ? (rand() % offset_max_y) : 0;
  }

  for (const auto& channelset : channelsets)
  {
    // Calculate region of interest image.
    const int channels = channelset->Channels();
    cv::Mat orig_image(height, width, CV_32FC(channels), channelset->GetFinalMemory());
    cv::Mat cropped_image(crop_size_, crop_size_, CV_32FC(channels), channelset->GetWorkspaceMemory());

    cv::Rect roi(offset_x, offset_y, crop_size_, crop_size_);
    cv::Mat roi_mat(orig_image, roi);

    // Copy roi image to workspace memory.
    roi_mat.copyTo(cropped_image);

    // Make workspace memory final.
    channelset->Swap();

    channelset->SetWidth(crop_size_);
    channelset->SetHeight(crop_size_);
  }
}
