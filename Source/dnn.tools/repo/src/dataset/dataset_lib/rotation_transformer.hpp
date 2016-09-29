#pragma once

#include "base_transformer.hpp"

#include <random>
#include <vector>

// This transformer applies a random rotation to a given pair of channels
// interpreted as a 2D point set.
class RotationTransformer : public BaseTransformer<RotationTransformer>
{
public:
  static const char* GetTypeString() { return "rotation"; }
  RotationTransformer(const TransformParameter& param);
  virtual void GetTransformedSizeImpl(int width, int height, int& newWidth, int& newHeight) override;
  virtual int GetRequiredWorkspaceMemoryImpl(int width, int height, int channels) override;
  virtual void TransformImpl(std::vector<TransformableChannelset*>& channelsets) override;

private:
  static const int dim_ = 2;

  std::mt19937 gen_;
  std::uniform_real_distribution<double> uniform_;
  std::vector<std::vector<double> > coef_;
};