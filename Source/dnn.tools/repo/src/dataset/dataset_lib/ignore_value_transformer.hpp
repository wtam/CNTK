#pragma once

#include "base_transformer.hpp"

#include <vector>

// Given data and label channelsets, marks with ignore label all pixels with given data value.
// Data and label channelsets need to have one channel each, and they need to be specified in
// that order (first data, then label) in transformer configuration.

class IgnoreValueTransformer : public BaseTransformer<IgnoreValueTransformer>
{
public:
  static const char* GetTypeString() { return "ignore_value"; }
  IgnoreValueTransformer(const TransformParameter& param);
  virtual void GetTransformedSizeImpl(int width, int height, int& newWidth, int& newHeight) override;
  virtual int GetRequiredWorkspaceMemoryImpl(int width, int height, int channels) override;
  virtual void TransformImpl(std::vector<TransformableChannelset*>& channelsets) override;

private:
  int m_ignoreLabel;
  float m_value;
};