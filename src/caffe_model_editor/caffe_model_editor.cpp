// Need to disable warnings from protobuf.
#define _SCL_SECURE_NO_WARNINGS

#include "caffe_model_editor.hpp"

#include "caffe_model_editor.pb.hpp"
#include "caffe.pb.hpp"
#include "check.hpp"
#include "proto_io.hpp"

#include <algorithm>
#include <memory>

using namespace std;
using namespace caffe;
using namespace caffe_model_editor;
using namespace google::protobuf;

// Base class for all field manipulators.
class FieldManipulatorBase
{
public:
    virtual ~FieldManipulatorBase() {}
    virtual void Set(
        const Reflection* reflection,
        Message* message,
        const FieldDescriptor* field,
        const string& value
        ) const = 0;
    virtual void SetRepeted(
        const Reflection* reflection,
        Message* message,
        const FieldDescriptor* field,
        int index,
        const string& value
        ) const = 0;
};

// Type specific field manipulator implementation.
template <class Type>
class FieldManipulator : public FieldManipulatorBase
{
public:
    typedef void(Reflection::*FieldManipulatorMethod)(
        Message* message,
        const FieldDescriptor* field,
        Type value
        ) const;
    typedef void(Reflection::*RepeatedFieldManipulatorMethod)(
        Message* message,
        const FieldDescriptor* field,
        int index,
        Type value
        ) const;

    FieldManipulator(
        FieldManipulatorMethod set_method,
        RepeatedFieldManipulatorMethod set_repeated_method,
        FieldManipulatorMethod add_method
        ) : set_method_(set_method), set_repeated_method_(set_repeated_method), add_method_(add_method) {}

    virtual void Set(
        const Reflection* reflection,
        Message* message,
        const FieldDescriptor* field,
        const string& value
        ) const override
    {
        if (field->is_repeated())
        {
            (reflection->*add_method_)(message, field, GetValue(field, value));
        }
        else
        {
            (reflection->*set_method_)(message, field, GetValue(field, value));
        }
    }

    virtual void SetRepeted(
        const Reflection* reflection,
        Message* message,
        const FieldDescriptor* field,
        int index,
        const string& value
        ) const override
    {
        (reflection->*set_repeated_method_)(message, field, index, GetValue(field, value));
    }

private:
    // Needs to be specialized for each supported type. All are provided below class declaration.
    virtual Type GetValue(const FieldDescriptor* field, const string& value) const;

    FieldManipulatorMethod set_method_;
    RepeatedFieldManipulatorMethod set_repeated_method_;
    FieldManipulatorMethod add_method_;
};

template <>
int32 FieldManipulator<int32>::GetValue(const FieldDescriptor* /*field*/, const string& value) const
{
    return stoi(value);
}

template <>
int64 FieldManipulator<int64>::GetValue(const FieldDescriptor* /*field*/, const string& value) const
{
    return stoll(value);
}

template <>
uint32 FieldManipulator<uint32>::GetValue(const FieldDescriptor* /*field*/, const string& value) const
{
    return stoul(value);
}

template <>
uint64 FieldManipulator<uint64>::GetValue(const FieldDescriptor* /*field*/, const string& value) const
{
    return stoull(value);
}

template <>
float FieldManipulator<float>::GetValue(const FieldDescriptor* /*field*/, const string& value) const
{
    return stof(value);
}

template <>
double FieldManipulator<double>::GetValue(const FieldDescriptor* /*field*/, const string& value) const
{
    return stod(value);
}

template <>
bool FieldManipulator<bool>::GetValue(const FieldDescriptor* /*field*/, const string& value) const
{
    if (value == "true")
    {
        return true;
    }
    else if (value == "false")
    {
        return false;
    }
    CHECK(false, "Invalid string %s for bool. Acceptable values are true and false.", value.c_str());
    return false;
}

template <>
const string& FieldManipulator<const string&>::GetValue(const FieldDescriptor* /*field*/, const string& value) const
{
    return value;
}

template <>
const EnumValueDescriptor* FieldManipulator<const EnumValueDescriptor*>::GetValue(
    const FieldDescriptor* field,
    const string& value
    ) const
{
    const EnumDescriptor* enum_descriptor = field->enum_type();
    const EnumValueDescriptor* enum_value_descriptor = enum_descriptor->FindValueByName(value);
    return enum_value_descriptor;
}

template <>
const Message* FieldManipulator<const Message*>::GetValue(
    const FieldDescriptor* /*field*/,
    const string& /*value*/
    ) const
{
    CHECK(false, "Setting message from string is not supported.");
    return nullptr;
}

// Lookup table for all field manipulators.
unique_ptr<const FieldManipulatorBase> g_fieldManipulators[FieldDescriptor::CppType::MAX_CPPTYPE] =
{
    { make_unique<FieldManipulator<int32>>(&Reflection::SetInt32, &Reflection::SetRepeatedInt32, &Reflection::AddInt32) },
    { make_unique<FieldManipulator<int64>>(&Reflection::SetInt64, &Reflection::SetRepeatedInt64, &Reflection::AddInt64) },
    { make_unique<FieldManipulator<uint32>>(&Reflection::SetUInt32, &Reflection::SetRepeatedUInt32, &Reflection::AddUInt32) },
    { make_unique<FieldManipulator<uint64>>(&Reflection::SetUInt64, &Reflection::SetRepeatedUInt64, &Reflection::AddUInt64) },
    { make_unique<FieldManipulator<double>>(&Reflection::SetDouble, &Reflection::SetRepeatedDouble, &Reflection::AddDouble) },
    { make_unique<FieldManipulator<float>>(&Reflection::SetFloat, &Reflection::SetRepeatedFloat, &Reflection::SetFloat) },
    { make_unique<FieldManipulator<bool>>(&Reflection::SetBool, &Reflection::SetRepeatedBool, &Reflection::AddBool) },
    { make_unique<FieldManipulator<const EnumValueDescriptor*>>(&Reflection::SetEnum, &Reflection::SetRepeatedEnum, &Reflection::AddEnum) },
    { make_unique<FieldManipulator<const string&>>(&Reflection::SetString, &Reflection::SetRepeatedString, &Reflection::AddString) },
    { make_unique<FieldManipulator<const Message*>>(nullptr, nullptr, nullptr) }
};

// Edit action which removes layer.
static void RemoveLayer(NetParameter& net_param, const CaffeModelEdit& edit)
{
    CHECK(edit.has_remove_layer_params(), "Missing remove_layer_params.");
    const RemoveLayerParameters& remove_layer_param = edit.remove_layer_params();
    for (int il = 0; il < net_param.layer_size(); il++)
    {
        caffe::LayerParameter* layer = net_param.mutable_layer(il);
        if (layer->name() == remove_layer_param.name())
        {
            // Remove current layer param.
            net_param.mutable_layer()->DeleteSubrange(il, 1);
            break;
        }
    }
}

// Edit action which adds new layer.
static void AddLayer(NetParameter& net_param, const CaffeModelEdit& edit)
{
    CHECK(edit.has_add_layer_params(), "Missing add_layer_params.");
    const AddLayerParameters& add_layer_params = edit.add_layer_params();

    caffe::LayerParameter* data_layer = net_param.mutable_layer()->Add();
    data_layer->set_name(add_layer_params.name());
    data_layer->set_type(add_layer_params.type());

    if (add_layer_params.add_type() == AddLayerParameters_AddType_PREPEND)
    {
        rotate(
            net_param.mutable_layer()->rbegin(),
            net_param.mutable_layer()->rbegin() + 1,
            net_param.mutable_layer()->rend()
            );
    }
}

// Edit action which edits existing layer.
static void EditLayer(NetParameter& net_param, const CaffeModelEdit& edit)
{
    CHECK(edit.has_edit_layer_params(), "Missing edit_layer_params.");
    const EditLayerParameters& edit_layer_params = edit.edit_layer_params();
    // Go over all layers.
    for (int il = 0; il < net_param.layer_size(); il++)
    {
        // Take current layer and check if it is our target.
        caffe::LayerParameter* layer = net_param.mutable_layer(il);
        if (layer->name() == edit_layer_params.layer_name())
        {
            // We found target layer, take its reflection and description.
            const Reflection* layer_reflection = layer->GetReflection();
            const Descriptor* layer_descriptor = layer->GetDescriptor();

            // Now loop over params to edit.
            for (int ipe = 0; ipe < edit_layer_params.param_setter_params_size(); ipe++)
            {
                ParamSetterParameters param_setter_params = edit_layer_params.param_setter_params(ipe);

                // Check if we are editing layer field or sub-message field and take appropriate descriptor to use.
                const Descriptor* target_descriptor = layer_descriptor;
                const Reflection* target_reflection = layer_reflection;
                Message* target_message = layer;
                if (param_setter_params.has_param_name())
                {
                    // We need to find layer sub-message. Go over the layer fields.
                    target_descriptor = nullptr;
                    for (int i = 0; i < layer_descriptor->field_count(); i++)
                    {
                        const FieldDescriptor* layer_param_descriptor = layer_descriptor->field(i);
                        if (layer_param_descriptor->name() == param_setter_params.param_name())
                        {
                            target_message = layer_reflection->MutableMessage(layer, layer_param_descriptor);
                            target_descriptor = target_message->GetDescriptor();
                            target_reflection = target_message->GetReflection();
                            break;
                        }
                    }
                    CHECK(
                        target_descriptor != nullptr,
                        "Cannot find layer param %s.", param_setter_params.param_name().c_str()
                        );
                }

                // Go over the fields of target descriptor and find field with desired name.
                for (int iF = 0; iF < target_descriptor->field_count(); iF++)
                {
                    const FieldDescriptor* field_descriptor = target_descriptor->field(iF);
                    if (param_setter_params.name() == field_descriptor->name())
                    {
                        // We found the target field, modify it.
                        if (param_setter_params.has_index())
                        {
                            g_fieldManipulators[field_descriptor->cpp_type() - 1]->SetRepeted(
                                target_reflection,
                                target_message,
                                field_descriptor,
                                param_setter_params.index(),
                                param_setter_params.value()
                                );
                        }
                        else
                        {
                            g_fieldManipulators[field_descriptor->cpp_type() - 1]->Set(
                                target_reflection,
                                target_message,
                                field_descriptor,
                                param_setter_params.value()
                                );
                        }
                        break;
                    }
                }
            }
        }
    }
}

// Method signature for edit methods.
typedef void(*EditMethod)(NetParameter& net_param, const CaffeModelEdit& edit);

// Lookup table to available edit actions.
EditMethod c_editMethods[EditAction::COUNT] =
{
    RemoveLayer,
    AddLayer,
    EditLayer
};

static void EditCaffeModel(NetParameter& net_param, const CaffeModelEdits& caffe_model_edits)
{
    // Go over provided edits and invoke them.
    for (int ie = 0; ie < caffe_model_edits.edit_size(); ie++)
    {
        const CaffeModelEdit& edit = caffe_model_edits.edit(ie);
        c_editMethods[edit.edit_action()](net_param, edit);
    }
}

// Performs editing of input model in accordance to edit instructions from config.
// NOTE: All editing operations are supported except directly editing messages (in this case editing will throw).
void EditCaffeModel(const string& input_model_path, const string& config_path, const string& output_model_path)
{
    // First load input Caffe model.
    NetParameter net_param;
    ReadProtoFromTextFile(input_model_path.c_str(), &net_param);

    // Load configuration file that defines editing operations.
    CaffeModelEdits caffe_model_edits;
    ReadProtoFromTextFile(config_path.c_str(), &caffe_model_edits);

    // Edit model based on edit configuration.
    EditCaffeModel(net_param, caffe_model_edits);

    // Save final model.
    WriteProtoToTextFile(net_param, output_model_path.c_str());
}