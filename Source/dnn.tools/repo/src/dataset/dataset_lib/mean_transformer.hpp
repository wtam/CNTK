#pragma once

#include "base_transformer.hpp"

#include <memory>

// Subtracts mean image from channelset.
class MeanTransformer : public BaseTransformer<MeanTransformer>
{
public:
  static const char* GetTypeString() { return "mean"; };

  MeanTransformer(const TransformParameter& param);

  virtual void GetTransformedSizeImpl(int width, int height, int& new_width, int& new_height) override;

  virtual void TransformImpl(std::vector<TransformableChannelset*>& channelsets) override;

private:
  std::unique_ptr<float[]> data_mean_;
  int channels_;
  int dim_;
};
