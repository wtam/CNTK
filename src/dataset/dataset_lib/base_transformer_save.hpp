#pragma once

#include "transformer_save.hpp"

// Base class for all save transformers. Each derived class of type T must be derived from BaseTransformSave<T>.
template <typename TDerived>
class BaseTransformerSave : public ITransformerSave
{
public:
  static std::unique_ptr<ITransformerSave> Create(const TransformParameterSave& param)
  {
    return std::unique_ptr<ITransformerSave>(new TDerived(param));
  }

  static const char* GetType()
  {
    return TDerived::GetTypeString();
  }
};
