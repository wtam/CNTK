// All load transformers.
#include "alexnet_color_transformer.hpp"
#include "channelwise_scale_shift_transformer.hpp"
#include "color_jitter_transformer.hpp"
#include "crop_transformer.hpp"
#include "horizontal_flip_transformer.hpp"
#include "mean_transformer.hpp"
#include "minresize_transformer.hpp"
#include "resize_crop_transformer.hpp"
#include "rotation_transformer.hpp"
// All save transformers.
#include "fixresize_transformer_save.hpp"
#include "minresize_transformer_save.hpp"
#include "pixelmap_transformer_save.hpp"

#include "check.hpp"

#include "ds_load_params.hpp"
#include "ds_save_params.hpp"

#include <algorithm>
#include <string>
#include <unordered_map>

using namespace std;

// Defines transformer factory method signature.
typedef unique_ptr<ITransformer> (*TransformerFactory)(const TransformParameter& param);
typedef unique_ptr<ITransformerSave>(*TransformerFactorySave)(const TransformParameterSave& param);

// Helper method to enforce static methods required by transformer.
template <typename T, typename TFactory>
static pair<string, TFactory> MakeFactoryEntry()
{
  string type = T::GetType();
  transform(type.begin(), type.end(), type.begin(), ::tolower);
  return{ type, T::Create };
}

template <typename T, typename TParam, typename TFactory>
static unique_ptr<T> CreateTransformerBase(const TParam& param, unordered_map<string, TFactory>& map)
{
  string type = param.type();
  transform(type.begin(), type.end(), type.begin(), ::tolower);
  auto transformer_factory = map.find(type);
  if (transformer_factory != map.end())
  {
    return transformer_factory->second(param);
  }
  CHECK(false, "Invalid transform %s.", type.c_str());
  return nullptr;
}

// Maps transform name to its factory method.
unordered_map<string, TransformerFactory> transfomers_factory_map =
{
  MakeFactoryEntry<MinResizeTransformer, TransformerFactory>(),
  MakeFactoryEntry<CropTransformer, TransformerFactory>(),
  MakeFactoryEntry<MeanTransformer, TransformerFactory>(),
  MakeFactoryEntry<ChannelwiseScaleShiftTransformer, TransformerFactory>(),
  MakeFactoryEntry<HorizontalFlipTransformer, TransformerFactory>(),
  MakeFactoryEntry<AlexNetColorTransformer, TransformerFactory>(),
  MakeFactoryEntry<ColorJitterTransformer, TransformerFactory>(),
  MakeFactoryEntry<ResizeCropTransformer, TransformerFactory>(),
  MakeFactoryEntry<RotationTransformer, TransformerFactory>()
};

unique_ptr<ITransformer> CreateTransformer(const TransformParameter& param)
{
  return CreateTransformerBase<ITransformer, TransformParameter>(param, transfomers_factory_map);
}

// Maps save transform name to its factory method.
unordered_map<string, TransformerFactorySave> transfomers_factory_map_save =
{
  MakeFactoryEntry<FixedResizeTransformerSave, TransformerFactorySave>(),
  MakeFactoryEntry<MinResizeTransformerSave, TransformerFactorySave>(),
  MakeFactoryEntry<PixelMapTransformerSave, TransformerFactorySave>()
};

unique_ptr<ITransformerSave> CreateTransformerSave(const TransformParameterSave& param)
{
  return CreateTransformerBase<ITransformerSave, TransformParameterSave>(param, transfomers_factory_map_save);
}