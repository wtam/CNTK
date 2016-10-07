//-----------------------------------------------------------------------------
// The logic related to overriding load parameters taken from config file
// is implemented in this file.
// To add new overridable parameter:
//  1) Add new OverridableParamID in dataset_io.h. Enum must have same name as
//     name of parameter to override from proto file.
//  2) Add name checker. Search for block with DEFINE_NAME_CHECKER macros and add
//     DEFINE_NAME_CHECKER(name) where name is name of parameter to override.
//  3) Add parameter descriptor. Search for block with DEFINE_DESCRIPTOR and add
//     DEFINE_DESCRIPTOR(type, name) where type and name are exact parameter type
//     and name from proto file.
//-----------------------------------------------------------------------------

#include "dataset_load_runtime_overrides.hpp"

#include "check.hpp"
#include "ds_load_params.hpp"

#include <string>

using namespace google::protobuf;
using namespace std;

// Base class for all parameter setters. Parameter setters take care of converting string value to appropriate type and
// invoking suitable proto method to override parameter value.
class IOverridableParamSetter
{
public:
  virtual ~IOverridableParamSetter() = default;
  virtual void Set(DsLoadParameters& load_parameters, const FieldDescriptor* field_descriptor, const string& str) = 0;
};

// Parameter setter for uint32 type.
class OverridableparamSetterUInt32 : public IOverridableParamSetter
{
public:
  virtual void Set(DsLoadParameters& load_parameters, const FieldDescriptor* field_descriptor, const string& str) override
  {
    CHECK(field_descriptor->cpp_type() == FieldDescriptor::CppType::CPPTYPE_UINT32,
      "Invalid cpp type of parameter to override %s", field_descriptor->name().c_str());
    const Reflection* param_reflection = load_parameters.GetReflection();
    param_reflection->SetUInt32(&load_parameters, field_descriptor, stoul(str));
  }
};

// Parameter setter for string type.
class OverridableparamSetterString : public IOverridableParamSetter
{
public:
  virtual void Set(DsLoadParameters& load_parameters, const FieldDescriptor* field_descriptor, const string& str) override
  {
    CHECK(field_descriptor->cpp_type() == FieldDescriptor::CppType::CPPTYPE_STRING,
      "Invalid cpp type of parameter to override %s", field_descriptor->name().c_str());
    const Reflection* param_reflection = load_parameters.GetReflection();
    param_reflection->SetString(&load_parameters, field_descriptor, str);
  }
};

// Instantiate all parameter setters to be used below.
OverridableparamSetterUInt32 overridableparamSetter_uint32;
OverridableparamSetterString overridableparamSetter_string;

// Base class for all name checkers. As new overridable parameter is added it needs to be guarded against proto name
// change defining checker with DEFINE_NAME_CHECKER(name).
class INameChecker
{
public:
  virtual ~INameChecker() = default;
  virtual void Check() = 0;
};

// Descriptor for overridable parameter.
struct OverridableParamDescriptor
{
  OverridableParamDescriptor(OverridableParamID i, string n, IOverridableParamSetter* s, INameChecker* c) :
    id(i), name(n), setter(s), checker(c) {}

public:
  OverridableParamID id;            // External ID of the parameter.
  string name;                      // Name of the parameter (used to retrieve filed descriptor for setter).
  IOverridableParamSetter* setter;  // Setter to be used for overriding.
private:
  INameChecker* checker;            // Checker, not used but needs to be defined.
};

// One subtle problem we may have is change of the parameter name in proto file. This cannot be checked in compile time.
// However proto compiler generates method with the same name to access parameter value. This can be checked in compile
// time using meta programming and SFINAE. Macro below will generate compile time error if proto generated class does
// not have method with name equal to macro parameter (overridable parameter name).
// Unfortunately, decltype on different platforms has different behavior so due to this we need to copy paste macro
// (although we have difference in one character).
#ifdef _MSC_VER
#define DEFINE_NAME_CHECKER(Method)                                           \
template <typename T>                                                         \
class HasMethod##Method : public INameChecker {                               \
    template <typename C> static char test(decltype(C::Method));              \
    template <typename C> static long test(...);                              \
public:                                                                       \
    HasMethod##Method() {                                                     \
      static_assert(sizeof(test<T>(0)) == sizeof(char),                       \
        "Method DsLoadParameters::"#Method" not defined."); }                 \
    virtual void Check() override {};                                         \
};                                                                            \
HasMethod##Method<DsLoadParameters> checkerFor_##Method;
#else
#define DEFINE_NAME_CHECKER(Method)                                           \
template <typename T>                                                         \
class HasMethod##Method : public INameChecker {                               \
    template <typename C> static char test(decltype(&C::Method));              \
    template <typename C> static long test(...);                              \
public:                                                                       \
    HasMethod##Method() {                                                     \
      static_assert(sizeof(test<T>(0)) == sizeof(char),                       \
        "Method DsLoadParameters::"#Method" not defined."); }                 \
    virtual void Check() override {};                                         \
};                                                                            \
HasMethod##Method<DsLoadParameters> checkerFor_##Method;
#endif

// Helper macro to ensure synchronization between enumeration and proto message name. Also takes care of declaring
// proper setter  based on type.
// type - type of the parameter in proto file
// name - name of the parameter in proto file
#define DEFINE_DESCRIPTOR(type, name) \
OverridableParamDescriptor(OverridableParamID::name, #name, &overridableparamSetter_##type, &checkerFor_##name)

// Make sure all proto names are consistent with strings we use here.
DEFINE_NAME_CHECKER(source_path);
DEFINE_NAME_CHECKER(loader_index);
DEFINE_NAME_CHECKER(loaders_count);
// Array of all descriptors. Use DEFINE_DESCRIPTOR macro to add new overrides.
OverridableParamDescriptor overridable_param_descriptors[] =
{
  DEFINE_DESCRIPTOR(string, source_path),
  DEFINE_DESCRIPTOR(uint32, loader_index),
  DEFINE_DESCRIPTOR(uint32, loaders_count)
};

// Ensure we have descriptors for all overridable parameters.
static_assert(sizeof(overridable_param_descriptors) / sizeof(overridable_param_descriptors[0]) == static_cast<size_t>(OverridableParamID::count),
  "Mismatch between number of overridable parameters and its descriptors");

// Applies given set of runtime overrides on top of load parameters from config file.
void ApplyRuntimeOverrides(DsLoadParameters& load_parameters, vector<OverridableParam>* runtime_params)
{
  // If no runtime parameters are present just return.
  if (runtime_params == nullptr)
  {
    return;
  }

  // Go over array of provided overrides.
  for (size_t ip = 0; ip < runtime_params->size(); ip++)
  {
    OverridableParam param = (*runtime_params)[ip];
    bool override_descriptor_found = false;
    bool field_descriptor_found = false;
    // For each override find its descriptor.
    for (int id = 0; id < sizeof(overridable_param_descriptors) / sizeof(overridable_param_descriptors[0]); id++)
    {
      if (overridable_param_descriptors[id].id == param.id)
      {
        // Now find corresponding field descriptor.
        const Descriptor* param_descriptor = load_parameters.GetDescriptor();
        CHECK(param_descriptor != nullptr, "nullptr load parameters descriptor.");
        for (int i = 0; i < param_descriptor->field_count(); i++)
        {
          const FieldDescriptor* field_descriptor = param_descriptor->field(i);
          CHECK(field_descriptor != nullptr, "nullptr load parameters field descriptor.");
          if (field_descriptor->name() == overridable_param_descriptors[id].name)
          {
            // Override the value.
            CHECK(overridable_param_descriptors[id].setter != nullptr, "Overridable parameter setter is nullptr.");
            overridable_param_descriptors[id].setter->Set(load_parameters, field_descriptor, param.value);
            field_descriptor_found = true;
            break;
          }
        }
        override_descriptor_found = true;
        break;
      }
    }
    CHECK(override_descriptor_found, "Override descriptor not found for param id %d", param.id);
    CHECK(field_descriptor_found, "Field descriptor not found for param id %d", param.id);
  }
}