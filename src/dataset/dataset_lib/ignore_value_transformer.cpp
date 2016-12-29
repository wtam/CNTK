#include "ignore_value_transformer.hpp"

#include "check.hpp"
#include "ds_load_params.hpp"

#include <opencv2/core/core.hpp>

using namespace std;

IgnoreValueTransformer::IgnoreValueTransformer(const TransformParameter& param)
    : BaseTransformer(param)
{
  CHECK(param.type() == GetType(), "Invalid type in IgnoreValueTransformer %s.", param.type().c_str());
  CHECK(param.target_size() == 2, "Two targets requred, %d provided", param.target_size());

  m_ignoreLabel = param.ignore_value_param().ignore_label();
  m_value = param.ignore_value_param().value();
}

void IgnoreValueTransformer::GetTransformedSizeImpl(int width, int height, int& newWidth, int& newHeight)
{
  newWidth = width;
  newHeight = height;
}

int IgnoreValueTransformer::GetRequiredWorkspaceMemoryImpl(int /*width*/, int /*height*/, int /*channels*/)
{
  return 0;
}

void IgnoreValueTransformer::TransformImpl(std::vector<TransformableChannelset*>& channelsets)
{
  CHECK(channelsets.size() == 2, "Two input channelsets expected, %d provided", channelsets.size());

  const int height = channelsets[0]->Height();
  const int width = channelsets[0]->Width();
  CHECK(channelsets[0]->Channels() == 1,
    "One channel expected for target channelset 0, %d channels provided", channelsets[0]->Channels());
  CHECK(channelsets[1]->Channels() == 1,
    "One channel expected for target channelset 1, %d channels provided", channelsets[1]->Channels());
  CHECK(channelsets[1]->Height() == height,
    "Target channelsets have different height (%d for channelset 0, and %d for channelset 1)", height, channelsets[1]->Height());
  CHECK(channelsets[1]->Width() == width,
    "Target channelsets have different width (%d for channelset 0, and %d for channelset 1)", width, channelsets[1]->Width());

  const int count = height * width;
  const float* value_data = channelsets[0]->GetFinalMemory();
  float* label_data = channelsets[1]->GetFinalMemory();
  for (int i = 0; i < count; ++i)
  {
    if (value_data[i] == m_value)
    {
      label_data[i] = static_cast<float>(m_ignoreLabel);
    }
  }
}
