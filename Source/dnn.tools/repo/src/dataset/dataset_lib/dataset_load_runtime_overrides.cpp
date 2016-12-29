//-----------------------------------------------------------------------------
// The logic related to overriding load parameters taken from config file
// is implemented in this file.
// To add new overridable parameter:
//  1) Add new OverridableParamID in dataset_io.hpp. Enum must have same name as
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
#include <utility>

using namespace google::protobuf;
using namespace std;

// Base class for all parameter setters. Parameter setters take care of converting string value to appropriate type and
// invoking suitable proto method to override parameter value.
class IOverridableParamSetter
{
public:
  virtual ~IOverridableParamSetter() = default;
  virtual void Set(
      DsLoadParameters& load_parameters,
      const FieldDescriptor* field_descriptor,
      const vector<string>& strVec
      ) = 0;
};

// Template class that introduces types to set.
template <class ParamType>
class IOverridableParamSetterTyped : public IOverridableParamSetter
{
public:
    // Function pointer that describes setting methods.
    typedef void(Reflection::*FieldSetterMethod)(
        Message* message,
        const FieldDescriptor* field,
        ParamType value
        ) const;

    // Function pointer that describes conversion from string method.
    typedef ParamType (*ConvertFromStringMethod)(const string& value);

    // Constructor, just save setter methods provided.
    IOverridableParamSetterTyped(
        FieldSetterMethod add_method,
        FieldSetterMethod set_method,
        ConvertFromStringMethod convert_method
        ) : add_method_(add_method), set_method_(set_method), convert_method_(convert_method) {}

    virtual ~IOverridableParamSetterTyped() = default;

    // Performs setting of the parameter.
    virtual void Set(
        DsLoadParameters& load_parameters,
        const FieldDescriptor* field_descriptor,
        const vector<string>& strVec
        )
    {
        CHECK(!strVec.empty(), "No values provided for parameter override.");
        // Take reflection.
        const Reflection* param_reflection = load_parameters.GetReflection();
        // Check if field is repeated.
        if (field_descriptor->is_repeated())
        {
            // Field is repeated, clear current value and add provided values.
            param_reflection->ClearField(&load_parameters, field_descriptor);
            for (const string& strValue : strVec)
            {
                (param_reflection->*add_method_)(&load_parameters, field_descriptor, convert_method_(strValue));
            }
        }
        else
        {
            CHECK(strVec.size() == 1, "Multiple values provided for overriding non repeated parameter.");
            (param_reflection->*set_method_)(&load_parameters, field_descriptor, convert_method_(strVec[0]));
        }
    }

    // Setter method to be used for repeated fields.
    FieldSetterMethod add_method_;
    // Setter method to be used for non-repeated fields.
    FieldSetterMethod set_method_;
    // Convert method to be used for string conversion.
    ConvertFromStringMethod convert_method_;
};


// Converts string to uint32 value.
static uint32 GetUint32Value(const string& strVal)
{
    return stoul(strVal);
}

// Perform dummy conversion from string to string.
static const string& GetStringValue(const string& strVal)
{
    return strVal;
}

// Instantiate all parameter setters to be used below.
IOverridableParamSetterTyped<uint32> overridableParamSetter_uint32(
    &Reflection::AddUInt32,
    &Reflection::SetUInt32,
    &GetUint32Value
);
IOverridableParamSetterTyped<const string&> overridableParamSetter_string(
    &Reflection::AddString,
    &Reflection::SetString,
    &GetStringValue
);

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
#define DEFINE_NAME_CHECKER(Method)                                           \
template <typename T>                                                         \
class HasMethod##Method : public INameChecker {                               \
    template <typename C, typename = decltype(std::declval<C>().Method())>    \
    static char test(int);                                                    \
    template <typename C>                                                     \
    static long test(...);                                                    \
public:                                                                       \
    HasMethod##Method() {                                                     \
      static_assert(sizeof(test<T>(0)) == sizeof(char),                       \
        "Method DsLoadParameters::"#Method" not defined."); }                 \
    virtual void Check() override {};                                         \
};                                                                            \
HasMethod##Method<DsLoadParameters> checkerFor_##Method;

// Helper macro to ensure synchronization between enumeration and proto message name. Also takes care of declaring
// proper setter  based on type.
// type - type of the parameter in proto file
// name - name of the parameter in proto file
#define DEFINE_DESCRIPTOR(type, name) \
OverridableParamDescriptor(OverridableParamID::name, #name, &overridableParamSetter_##type, &checkerFor_##name)

// Make sure all proto names are consistent with strings we use here.
DEFINE_NAME_CHECKER(source_path);
DEFINE_NAME_CHECKER(loader_index);
DEFINE_NAME_CHECKER(loaders_count);
DEFINE_NAME_CHECKER(source_name);
// Array of all descriptors. Use DEFINE_DESCRIPTOR macro to add new overrides.
OverridableParamDescriptor overridable_param_descriptors[] =
{
  DEFINE_DESCRIPTOR(string, source_path),
  DEFINE_DESCRIPTOR(uint32, loader_index),
  DEFINE_DESCRIPTOR(uint32, loaders_count),
  DEFINE_DESCRIPTOR(string, source_name)
};

// Ensure we have descriptors for all overridable parameters.
static_assert(
  sizeof(overridable_param_descriptors) / sizeof(overridable_param_descriptors[0]) ==
    static_cast<size_t>(OverridableParamID::count),
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
            overridable_param_descriptors[id].setter->Set(load_parameters, field_descriptor, param.values);
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