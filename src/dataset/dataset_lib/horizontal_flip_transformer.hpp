#pragma once

#include "base_transformer.hpp"

#include <random>
#include <vector>

// This transformer modifies all input channels in a way which is consistent
// with mirroring both scene and camera with respect to a vertical plane.
//
// At a minimum, this means horizontally flipping all input channels.
//
// In addition, any number of channels can be flagged for additional processing
// by setting flip_channel_values: true in configuration file. This indicates
// that these channels encode vectors in world space, one vector per pixel.
//
// If d channels are selected, they are viewed as d-dimensional points (one
// point per pixel), and mirrored across a random hyperplane in d-dimensional
// space. One random hyperplane is chosen per example (not per pixel/point).
//
// Typical applications involve channels encoding pose-independent normals:
// 1) Selected channel: x-components of normals (d == 1)
//    Effect: flipping sign of x-components of normals
// 2) Selected channel: y-components of normals (d == 1)
//    Effect: flipping sign of y-components of normals
// 3) Selected channel: x- and y-components of normals (d == 2)
//    Effect: mirroring normals across a random vertical plane
//
// Currently, only one set of channels can be selected for additional processing.
// (i.e. all selected channels are transformed jointly).
class HorizontalFlipTransformer : public BaseTransformer<HorizontalFlipTransformer>
{
public:
  static const char* GetTypeString() { return "horizontal_flip"; }
  HorizontalFlipTransformer(const TransformParameter& param);
  virtual void GetTransformedSizeImpl(int width, int height, int& newWidth, int& newHeight) override;
  virtual int GetRequiredWorkspaceMemoryImpl(int width, int height, int channels) override;
  virtual void TransformImpl(std::vector<TransformableChannelset*>& channelsets) override;

private:
  std::mt19937 gen_;
  std::bernoulli_distribution coin_;
  std::vector<bool> flip_channel_values_;
  std::vector<double> coef_;
  std::normal_distribution<double> normal_;
};