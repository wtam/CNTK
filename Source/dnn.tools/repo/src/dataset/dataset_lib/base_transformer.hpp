#pragma once

#include "transformer.hpp"

#include "ds_load_params.hpp"

#include <memory>
#include <string>
#include <vector>

// Base class for all transformers. Each derived class of type T must be derived from BaseTransform<T>.
template <typename TDerived>
class BaseTransformer : public ITransformer
{
public:
  static std::unique_ptr<ITransformer> Create(const TransformParameter& param)
  {
    return std::unique_ptr<ITransformer>(new TDerived(param));
  }

  static const char* GetType()
  {
    return TDerived::GetTypeString();
  }

  // Consolidates target handling for all derived classes.
  virtual void GetTransformedSize(const std::string& channelset_name, int width, int height, int& new_width, int& new_height) override final
  {
    if (std::find(targets_.begin(), targets_.end(), channelset_name) != targets_.end())
    {
      // This is our target, invoke derived class.
      GetTransformedSizeImpl(width, height, new_width, new_height);
    }
    else
    {
      // We do not transform this channelset.
      new_width = width;
      new_height = height;
    }
  }

  // Consolidates target handling for all derived classes.
  virtual int GetRequiredWorkspaceMemory(const std::string& channelset_name, int width, int height, int channels) override final
  {
    if (std::find(targets_.begin(), targets_.end(), channelset_name) != targets_.end())
    {
      // This is our target, invoke derived class.
      return GetRequiredWorkspaceMemoryImpl(width, height, channels);
    }
    else
    {
      // We do not transform this channelset.
      return 0;
    }
  }

  // Consolidates target handling for all derived classes.
  virtual void Transform(ITransformableChannelsetIterator& channelsets) override final
  {
    std::vector<TransformableChannelset*> filtered_channelsets;
    for (int itc = 0; itc < channelsets.GetTransformableChannelsetsCount(); itc++)
    {
      TransformableChannelset* channelset = channelsets.GetTransformableChannelset(itc);
      if (std::find(targets_.begin(), targets_.end(), *channelset->GetTarget()) != targets_.end())
      {
        filtered_channelsets.push_back(channelset);
      }
    }

    if (filtered_channelsets.size() > 0)
    {
      TransformImpl(filtered_channelsets);
    }
    // else: no transform needed, there are no our targets.
  }

protected:
  BaseTransformer(const TransformParameter& param)
  {
    for (int it = 0; it < param.target_size(); it++)
    {
      targets_.emplace_back(param.target(it));
    }
  }

private:
  // Actual implementation implemented for derived class.
  virtual void GetTransformedSizeImpl(int width, int height, int& new_width, int& new_height) = 0;

  // Actual implementation implemented for derived class.
  virtual int GetRequiredWorkspaceMemoryImpl(int width, int height, int channels) = 0;

  // Actual implementation implemented for derived class.
  virtual void TransformImpl(std::vector<TransformableChannelset*>& channelsets) = 0;

private:
  std::vector<std::string> targets_;
};
