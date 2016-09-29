#include "mean_transformer.hpp"
#include "check.hpp"
#include "platform.hpp"

#include "ds_load_params.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <string>

using namespace std;

MeanTransformer::MeanTransformer(const TransformParameter& param) : BaseTransformer(param)
{
  CHECK(param.type() == GetType(), "Invalid type in MeanTransformer: %s", param.type().c_str());
  CHECK(param.has_mean_param(), "Missing mean params.");

  string mean_file_path = param.mean_param().mean_file_path();
  dim_ = param.mean_param().resize_to();

  // Load mean from the mean file.
  FILE* mean_file;
  CHECK(Platform::fopen_s(&mean_file, mean_file_path.c_str(), "rb") == 0, "Failed to open mean file %s", mean_file_path.c_str());
  CHECK(mean_file != nullptr, "Failed to open mean file %s", mean_file_path.c_str());

  int orig_mean_channels = 0;
  int orig_mean_height = 0;
  int orig_mean_width = 0;
  fread(&orig_mean_channels, sizeof(int), 1, mean_file);
  fread(&orig_mean_height, sizeof(int), 1, mean_file);
  fread(&orig_mean_width, sizeof(int), 1, mean_file);

  int orig_mean_size = orig_mean_channels * orig_mean_height * orig_mean_width;
  unique_ptr<float> orig_mean(new float[orig_mean_size]);

  fread(orig_mean.get(), sizeof(float), orig_mean_size, mean_file);

  fclose(mean_file);

  // Allocate our stored mean.
  channels_ = orig_mean_channels;
  data_mean_.reset(new float[channels_ * dim_ * dim_]);

  // Resize original mean to provided dimension.
  int orig_mean_channel_size = orig_mean_height * orig_mean_width;
  float scale_x = 1.0f * orig_mean_width / dim_;
  float scale_y = 1.0f * orig_mean_height / dim_;
  int dest_index = 0;
  for (int iy = 0; iy < dim_; iy++)
  {
    for (int ix = 0; ix < dim_; ix++)
    {
      int orig_mean_x = static_cast<int>(scale_x * ix);
      int orig_mean_y = static_cast<int>(scale_y * iy);
      for (int i = 0; i < channels_; i++)
      {
        data_mean_[dest_index] = orig_mean.get()[orig_mean_y * orig_mean_width + orig_mean_x + i * orig_mean_channel_size];
        dest_index++;
      }
    }
  }
}

void MeanTransformer::GetTransformedSizeImpl(int width, int height, int& new_width, int& new_height)
{
  // Check that input dimensions are as expected.
  CHECK(dim_ == width, "Unexpected width %d in mean transform (dim is %d)", width, dim_);
  CHECK(dim_ == height, "Unexpected height %d in mean transform (dim is %d)", height, dim_);
  new_width = width;
  new_height = height;
}

int MeanTransformer::GetRequiredWorkspaceMemoryImpl(int /*width*/, int /*height*/, int /*channels*/)
{
  return 0;
}

void MeanTransformer::TransformImpl(vector<TransformableChannelset*>& channelsets)
{
  // Currently we expect just one channelset.
  CHECK(channelsets.size() == 1, "MeanTransformer expects one channelset, provided %u", channelsets.size());
  TransformableChannelset* channelset = channelsets[0];

  int height = channelset->Height();
  int width = channelset->Width();
  int channels = channelset->Channels();
  CHECK(height == dim_ && width == dim_ && channels == channels_,
    "Invalid channelset dimensions in MeanTransformer. H=%d W=%d C=%d dim_=%d channels_=%d", height, width, channels, dim_, channels_);
  cv::Mat orig_image(height, width, CV_32FC(channels), channelset->GetFinalMemory());
  cv::Mat mean_image(dim_, dim_, CV_32FC(channels_), data_mean_.get());
  orig_image -= mean_image;
}
