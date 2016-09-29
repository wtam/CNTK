#pragma once

// Declares interface for data transformers.
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

class TransformParameter;

// Input to transformers logic. Contains image (final memory) and some workspace memory along with image dimensions.
// Transformer needs to transform input image and store result in final memory.
struct TransformableChannelset
{
  TransformableChannelset(const char* channelset_name, float* final_mem, float* work_mem)
    : final_memory_(final_mem), workspace_memory_(work_mem), target_(channelset_name)
  {
  }

  float* GetFinalMemory() { return final_memory_; }

  float* GetWorkspaceMemory() { return workspace_memory_; }

  int Width() const { return width_; }

  int Height() const { return height_; }

  int Channels() const { return channels_; }

  void SetWidth(int w) { width_ = w; }

  void SetHeight(int h) { height_ = h; }

  void SetChannels(int c) { channels_ = c; }

  void Swap() { std::swap(final_memory_, workspace_memory_); }

  const std::string* GetTarget() const { return &target_; }

private:
  float* final_memory_;
  float* workspace_memory_;
  int height_;
  int width_;
  int channels_;
  std::string target_;
};

// Defines interface for iterating over collection of transformable channels.
class ITransformableChannelsetIterator
{
public:
  // Returns number of transformable channelsets.
  virtual int GetTransformableChannelsetsCount() = 0;
  // Returns transformable channelset at the given index.
  virtual TransformableChannelset* GetTransformableChannelset(int index) = 0;
};

// Transformer interface.
class ITransformer
{
public:
  virtual ~ITransformer() {}

  // Must return transformed dimensions given the current dimensions.
  virtual void GetTransformedSize(const std::string& channelset_name, int width,
                                  int height, int& newWidth, int& newHeight) = 0;

  // Must return requred memory for given input size.
  virtual int GetRequiredWorkspaceMemory(const std::string& channelset_name, int width,
                                         int height, int channels) = 0;

  // Transforms the given channelsets.
  virtual void Transform(ITransformableChannelsetIterator& channelsets) = 0;
};

std::unique_ptr<ITransformer> CreateTransformer(const TransformParameter& param);