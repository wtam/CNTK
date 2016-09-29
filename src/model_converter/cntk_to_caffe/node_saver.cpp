#define _SCL_SECURE_NO_WARNINGS
#pragma warning(push, 1)
#include "glog/logging.h"           // warning C4251: 'google::LogMessage::LogStream::streambuf_'
#include "caffe/util/io.hpp"        // conversion from 'uint64_t' to 'int', level 3 warning.
#include "caffe/proto/caffe.pb.h"   // warning C4996: 'std::_Copy_impl'
#pragma warning(pop)
#pragma warning( disable: 4996 )
#include "ConvolutionalNodes.h"     // warning C4996: 'sprintf', 'strerror', 'fwscanf': This function may be unsafe.
#pragma warning( default: 4996 )
#include "cntk_includes.h"
#include "cntk_node_wrappers.h"
#include "device_info.h"
#include "node_saver.h"
#include "node.h"
#include "boost/filesystem/path.hpp"
#include <fstream>
#include <iostream>
#include <locale>
#include <queue>
#include <unordered_map>
#undef _SCL_SECURE_NO_WARNINGS

using namespace std;
namespace cntk = Microsoft::MSR::CNTK;

static const bool c_write_diff = false;
typedef cntk::ConvolutionNodeBaseWrapper<cntk::ConvolutionNode<float>> ConvolutionNodeWrapper;
typedef shared_ptr<cntk::ConvolutionNodeBaseWrapper<cntk::ConvolutionNode<float>>> ConvolutionNodeWrapperPtr;

static const float* GetNodeParameters(const cntk::ComputationNodeBasePtr& cntk_node)
{
    const cntk::ComputationNode<float>* node = dynamic_cast<const cntk::ComputationNode<float>*>(cntk_node.get());
    CHECK(node != nullptr);
    return node->Value().Data();
}

static void AddBlob(caffe::LayerParameter& param,
    const float* data,
    const vector<int>& dimensions)
{
    caffe::BlobProto* blob = param.add_blobs();
    CHECK(blob != nullptr);
    blob->clear_shape();
    blob->clear_data();
    blob->clear_diff();

    int element_count = 1;
    for (int dimension : dimensions)
    {
        blob->mutable_shape()->add_dim(dimension);
        element_count *= dimension;
    }

    for (int index = 0; index < element_count; ++index)
    {
        blob->add_data(data[index]);
    }
}

static void SetInputLayerParams(shared_ptr<Node> layer,
    caffe::LayerParameter& param)
{
    const NodeAttributes& cntk_layer_params = layer->GetAttributes();
    param.set_type("Input");

    // Set shape.
    cntk::ComputationNodeBasePtr input = cntk_layer_params.at(NodeAttribute::Input);
    const cntk::TensorShape& cntk_shape = input->GetSampleLayout();
    param.mutable_input_param()->clear_shape();
    caffe::BlobShape* caffe_shape = param.mutable_input_param()->add_shape();
    caffe_shape->Clear();
    caffe_shape->add_dim(1);
    for (const size_t dim : cntk_shape.GetDims())
    {
        CHECK(dim > 0);
        caffe_shape->add_dim(dim);
    }

    // Set scaling.
    if ((cntk_layer_params.find(NodeAttribute::Scale) != cntk_layer_params.end()))
    {
        // TODO: add dimension check.
        cntk::ComputationNodeBasePtr scale = cntk_layer_params.at(NodeAttribute::Scale);
        const double scale_param = scale->Get00Element();
        param.mutable_transform_param()->set_scale(static_cast<float>(scale_param));
    }
}

static vector<float> Transpose(const float* data, int rows, int cols)
{
    vector<float> transposed(rows * cols, 0);
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            const int src_index = row * cols + col;
            const int dst_index = col * rows + row;
            transposed[dst_index] = data[src_index];
        }
    }
    return transposed;
}

static int GetParamCount(cntk::ComputationNodeBasePtr param_node)
{
    return param_node->GetSampleLayout().GetNumElements();
}

static void SetInnerProductLayerParams(
    shared_ptr<Node> layer,
    caffe::LayerParameter& param,
    bool save_weights)
{
    const NodeAttributes& cntk_layer_params = layer->GetAttributes();
    param.set_type("InnerProduct");
    cntk::ComputationNodeBasePtr weights = cntk_layer_params.at(NodeAttribute::Weights);
    const bool has_bias = cntk_layer_params.find(NodeAttribute::Bias) != cntk_layer_params.end();
    if (!has_bias)
    {
        caffe::InnerProductParameter* inner_product_params = param.mutable_inner_product_param();
        inner_product_params->set_bias_term(false);
    }

    const int weights_param_count = GetParamCount(weights);
    cntk::ComputationNodeBasePtr inputs = cntk_layer_params.at(NodeAttribute::Input);
    const int input_count = GetParamCount(inputs);
    CHECK(weights_param_count % input_count == 0);
    const int output_count = weights_param_count / input_count;
    param.mutable_inner_product_param()->set_num_output(output_count);
    if (save_weights)
    {
        // Save weights.
        // Weights in CNTK are stored col-major, so we have to transpose them.
        auto transposed = Transpose(GetNodeParameters(weights), input_count, output_count);
        AddBlob(param, transposed.data(), { output_count, input_count });

        if (has_bias)
        {
            // Save bias.
            cntk::ComputationNodeBasePtr bias = cntk_layer_params.at(NodeAttribute::Bias);
            AddBlob(param, GetNodeParameters(bias), { output_count });
        }
    }
}

static int GetNumberOfOutputsInCaffe(int kernel, int stride, int input_dimension, int padding)
{
    return (input_dimension + 2 * padding - kernel) / stride + 1;
}

static int GetPadding(int kernel, int stride, int input_dimension)
{
    const int output_dimension = (input_dimension - 1) / stride + 1;
    const int field_of_view = (output_dimension - 1) * stride + kernel;
    const int extra_view = field_of_view - input_dimension;
    const int left_padding = extra_view / 2;
    const int right_padding = extra_view - left_padding;
    int padding = left_padding;
    if (left_padding != right_padding)
    {
        const int caffe_outputs = GetNumberOfOutputsInCaffe(kernel, stride, input_dimension, left_padding);
        if (caffe_outputs != output_dimension)
        {
            padding = right_padding;
            CHECK(output_dimension == GetNumberOfOutputsInCaffe(kernel, stride, input_dimension, right_padding));
        }
    }

    return padding;
}

static bool IsAutoPaddingEnabled(const vector<bool>& auto_padding, int dimension)
{
    if (auto_padding.empty())
    {
        return false;
    }

    if (auto_padding.size() == 1)
    {
        // When auto_padding vector has one element, it is used for all dimensions.
        return auto_padding[0];
    }
    else
    {
        return auto_padding.at(dimension);
    }
}

static int GetPadding(cntk::ComputationNodeBasePtr cntk_input_node, ConvolutionNodeWrapperPtr wrapper, int dimension)
{
    int padding = 0;
    const int kernel = wrapper->GetKernelShape()[dimension];
    const int stride = wrapper->GetStrideShape()[dimension];
    const auto& lower_pad = wrapper->GetLowerPad();
    const auto& upper_pad = wrapper->GetUpperPad();

    // Get input dimensions.
    auto input_layout = cntk_input_node->GetSampleLayout();
    CHECK(input_layout.size() == 3);
    const int input_dimension = input_layout[dimension];

    // Calculate padding.
    auto auto_padding = wrapper->GetAutoPad();
    if (IsAutoPaddingEnabled(auto_padding, dimension))
    {
        padding = GetPadding(kernel, stride, input_dimension);
    }
    else
    {
        if (lower_pad.size() > 0)
        {
            // If left and right padding are not the same, we cannot convert CNTK network to Caffe.
            if (lower_pad[dimension] != upper_pad[dimension])
            {
                LOG(FATAL) << "Different lower and upper padding values";
            }
            padding = lower_pad[dimension];
        }
    }
    return padding;
}

static const int c_width_dimension = 0;
static const int c_height_dimension = 1;
static const int c_channel_dimension = 2;

static void SetConvolutionLayerParams(shared_ptr<Node> layer, caffe::LayerParameter& param, bool save_weights)
{
    const NodeAttributes& cntk_layer_params = layer->GetAttributes();

    // Get convolution params.
    std::shared_ptr<cntk::ConvolutionNode<float>> conv_cntk_params =
        dynamic_pointer_cast<cntk::ConvolutionNode<float>>(cntk_layer_params.at(NodeAttribute::Operation));
    const int device_id = DeviceInfo::GetInstance().GetId();
    ConvolutionNodeWrapperPtr wrapper = ConvolutionNodeWrapper::CreateWrapper(device_id, *conv_cntk_params);

    if (!wrapper->IsTransposed())
    {
        param.set_type("Convolution");
    }
    else
    {
        param.set_type("Deconvolution");
    }

    // Get weights params.
    cntk::ComputationNodeBasePtr weights = cntk_layer_params.at(NodeAttribute::Weights);
    const size_t weights_param_count = GetParamCount(weights);

    const size_t kernel_elements = wrapper->GetKernelShape().GetNumElements();
    const size_t output_maps = weights_param_count / kernel_elements;

    const int kernel_width = wrapper->GetKernelShape()[c_width_dimension];
    const int kernel_height = wrapper->GetKernelShape()[c_height_dimension];
    const int kernel_channels = wrapper->GetKernelShape()[c_channel_dimension];
    caffe::ConvolutionParameter* conv_params = param.mutable_convolution_param();
    if (kernel_height != kernel_width)
    {
        conv_params->set_kernel_h(kernel_height);
        conv_params->set_kernel_w(kernel_width);
    }
    else
    {
        conv_params->mutable_kernel_size()->Add(kernel_height);
    }

    const int stride_width = wrapper->GetStrideShape()[c_width_dimension];
    const int stride_height = wrapper->GetStrideShape()[c_height_dimension];
    if (stride_height != stride_width)
    {
        conv_params->set_stride_h(stride_height);
        conv_params->set_stride_w(stride_width);
    }
    else
    {
        conv_params->mutable_stride()->Add(stride_width);
    }

    auto bottom_layers = layer->GetBottomConnections();
    CHECK(bottom_layers.size() == 1);
    cntk::ComputationNodeBasePtr cntk_input_node = bottom_layers.front()->GetCntkHeadNode();

    const int padding_w = GetPadding(cntk_input_node, wrapper, c_width_dimension);
    const int padding_h = GetPadding(cntk_input_node, wrapper, c_width_dimension);
    if (padding_h != padding_w)
    {
        conv_params->set_pad_h(padding_h);
        conv_params->set_pad_w(padding_w);
    }
    else
    {
        conv_params->mutable_pad()->Add(padding_h);
    }

    // Get bias params.
    bool has_bias = cntk_layer_params.find(NodeAttribute::Bias) != cntk_layer_params.end();
    if (!has_bias)
    {
        conv_params->set_bias_term(false);
    }

    conv_params->set_num_output(output_maps);
    if (save_weights)
    {
        // Save weights.
        vector<int> dimensions{ static_cast<int>(output_maps), kernel_channels, kernel_height, kernel_width };
        AddBlob(param, GetNodeParameters(weights), dimensions);

        // Save bias.
        if (has_bias)
        {
            cntk::ComputationNodeBasePtr bias = cntk_layer_params.at(NodeAttribute::Bias);
            const int bias_param_count = GetParamCount(bias);
            CHECK(bias_param_count == static_cast<int>(output_maps));
            AddBlob(param, GetNodeParameters(bias), { bias_param_count });
        }
    }
}

static caffe::PoolingParameter_PoolMethod CntkToCaffePoolKind(cntk::PoolKind kind)
{
    switch (kind)
    {
    case Microsoft::MSR::CNTK::PoolKind::None:
        throw runtime_error("Pooling type None found");
    case Microsoft::MSR::CNTK::PoolKind::Max:
        return caffe::PoolingParameter_PoolMethod_MAX;
    case Microsoft::MSR::CNTK::PoolKind::Average:
        return caffe::PoolingParameter_PoolMethod_AVE;
    default:
        throw runtime_error("Couldn't serialize pooling type");
    }
}

void SetStrideParam(caffe::PoolingParameter* pooling_params, int stride_width, int stride_height)
{
    if (stride_height != stride_width)
    {
        pooling_params->set_stride_h(stride_height);
        pooling_params->set_stride_w(stride_width);
    }
    else
    {
        pooling_params->set_stride(stride_height);
    }
}

void SetKernelParam(caffe::PoolingParameter* pooling_params, int kernel_width, int kernel_height)
{
    if (kernel_width != kernel_height)
    {
        pooling_params->set_kernel_h(kernel_height);
        pooling_params->set_kernel_w(kernel_width);
    }
    else
    {
        pooling_params->set_kernel_size(kernel_height);
    }
}

static void AddPoolingParameters(caffe::LayerParameter& param, int kernel_width, int kernel_height, int stride_width,
    int stride_height, caffe::PoolingParameter_PoolMethod pool_method)
{
    caffe::PoolingParameter* pooling_params = param.mutable_pooling_param();
    pooling_params->set_pool(pool_method);
    SetKernelParam(pooling_params, kernel_width, kernel_height);
    SetStrideParam(pooling_params, stride_width, stride_height);
}

static void SetPoolingLayerParams(shared_ptr<Node> layer, caffe::LayerParameter& param)
{
    auto cntk_pooling = dynamic_pointer_cast<cntk::PoolingNode<float>>(layer->GetCntkHeadNode());
    CHECK(cntk_pooling != nullptr);
    param.set_type("Pooling");
    const int device_id = DeviceInfo::GetInstance().GetId();
    auto wrapper = cntk::ConvolutionNodeBaseWrapper<cntk::PoolingNode<float>>::CreateWrapper(device_id, *cntk_pooling);
    const int kernel_width = wrapper->GetKernelShape()[c_width_dimension];
    const int kernel_height = wrapper->GetKernelShape()[c_height_dimension];
    const int stride_width = wrapper->GetStrideShape()[c_width_dimension];
    const int stride_height = wrapper->GetStrideShape()[c_height_dimension];

    AddPoolingParameters(param, kernel_width, kernel_height, stride_width, stride_height,
        CntkToCaffePoolKind(wrapper->GetPoolKind()));
}

template <typename CntkPoolingNodeType>
caffe::PoolingParameter_PoolMethod GetPoolingMethod()
{
    static_assert("Unknown CNTK pooling node type");
}

template <>
caffe::PoolingParameter_PoolMethod GetPoolingMethod<cntk::MaxPoolingNode<float>>()
{
    return caffe::PoolingParameter_PoolMethod::PoolingParameter_PoolMethod_MAX;
}

template <>
caffe::PoolingParameter_PoolMethod GetPoolingMethod<cntk::AveragePoolingNode<float>>()
{
    return caffe::PoolingParameter_PoolMethod::PoolingParameter_PoolMethod_AVE;
}

template <typename CntkPoolingNodeType>
static void SetPoolingLayerParams(
    shared_ptr<Node> layer,
    caffe::LayerParameter& param)
{
    auto cntk_pooling = dynamic_pointer_cast<CntkPoolingNodeType>(layer->GetCntkHeadNode());
    CHECK(cntk_pooling != nullptr);
    param.set_type("Pooling");
    const int device_id = DeviceInfo::GetInstance().GetId();
    auto wrapper = cntk::PoolingNodeBaseWrapper<CntkPoolingNodeType>::CreateWrapper(device_id, *cntk_pooling);
    const int kernel_width = wrapper->GetWindowWidth();
    const int kernel_height = wrapper->GetWindowHeight();
    const int stride_width = wrapper->GetHorizontalSubsample();
    const int stride_height = wrapper->GetVerticalSubsample();

    AddPoolingParameters(param, kernel_width, kernel_height, stride_width, stride_height,
    GetPoolingMethod<CntkPoolingNodeType>());
}

static vector<float> GetVariance(const float* inv_std_dev, int element_count)
{
    vector<float> variance(element_count, 0.0f);
    for (int index = 0; index < element_count; ++index)
    {
        const float inv_std_val = inv_std_dev[index];
        const float inv_var_val = inv_std_val * inv_std_val;
        const float c_eps = 0.00001f;
        const float var_val = 1.0f / (c_eps + inv_var_val);
        variance[index] = var_val;
    }
    return variance;
}

static void SetBatchNormLayerParams(shared_ptr<Node> layer, caffe::LayerParameter& param, bool save_weights)
{
    param.set_type("BatchNorm");
    const NodeAttributes& cntk_layer_params = layer->GetAttributes();
    cntk::ComputationNodeBasePtr inv_std_dev = cntk_layer_params.at(NodeAttribute::InvStdDev);
    cntk::ComputationNodeBasePtr mean = cntk_layer_params.at(NodeAttribute::Mean);

    const int element_count = GetParamCount(mean);
    const int inv_std_dev_element_count = GetParamCount(inv_std_dev);
    CHECK(element_count == inv_std_dev_element_count);

    if (save_weights)
    {
        // Save mean.
        AddBlob(param, GetNodeParameters(mean), { element_count });

        // Save variance
        // CNTK stores inverse standard deviation values, while Caffe stores variance, so we have to convert
        // inverse standard deviation values to variances.
        auto variance = GetVariance(GetNodeParameters(inv_std_dev), element_count);
        AddBlob(param, variance.data(), { element_count });


        // Add moving average constant.
        vector<float> c_moving_average_constant{ 1.0f };
        AddBlob(param, c_moving_average_constant.data(), {1});
    }
}

static void SetBatchNormLayerScaleParams(shared_ptr<Node> layer, caffe::LayerParameter& param, bool save_weights)
{
    param.set_type("Scale");
    const NodeAttributes& cntk_layer_params = layer->GetAttributes();
    cntk::ComputationNodeBasePtr scale = cntk_layer_params.at(NodeAttribute::Scale);
    const int element_count = GetParamCount(scale);
    const bool has_bias = cntk_layer_params.find(NodeAttribute::Bias) != cntk_layer_params.end();
    param.mutable_scale_param()->set_bias_term(has_bias);

    if (save_weights)
    {
        // Save weights.
        AddBlob(param, GetNodeParameters(scale), {element_count});

        // Save bias.
        if (has_bias)
        {
            cntk::ComputationNodeBasePtr bias = cntk_layer_params.at(NodeAttribute::Bias);
            const int bias_param_count = GetParamCount(bias);
            CHECK(bias_param_count == element_count);
            AddBlob(param, GetNodeParameters(bias), { element_count });
        }
    }
}

#ifdef CONVERT_CROP_NODE
static void SetCropLayerParams(shared_ptr<Node> layer, caffe::LayerParameter& param)
{
    auto cntk_crop = dynamic_pointer_cast<cntk::CropNode<float>>(layer->GetCntkHeadNode());
    CHECK(cntk_crop != nullptr);
    param.set_type("Crop");
    const int device_id = DeviceInfo::GetInstance().GetId();
    auto wrapper = cntk::CropNodeWrapper<cntk::CropNode<float>>::CreateWrapper(device_id, *cntk_crop);
    const int x_offset = wrapper->GetOffsetX();
    const int y_offset = wrapper->GetOffsetY();

    caffe::CropParameter* crop_param = param.mutable_crop_param();
    crop_param->add_offset(x_offset);
    crop_param->add_offset(y_offset);
}
#endif

static NodeTag GetLayerType(shared_ptr<Node> layer)
{
    static const vector<NodeTag> c_caffe_layer_types =
    {
        NodeTag::AveragePooling,
        NodeTag::BatchNorm,
        NodeTag::Convolution,
#ifdef CONVERT_CROP_NODE
        NodeTag::Crop,
#endif
        NodeTag::Eltwise,
        NodeTag::InnerProduct,
        NodeTag::InputValue,
        NodeTag::MaxPooling,
        NodeTag::Pooling,
        NodeTag::ReLU,
        NodeTag::Sigmoid };
    const auto& tags = layer->GetTags();

    NodeTag layer_type = NodeTag::IsLayer;
    bool is_caffe_layer = false;
    for (const auto caffe_layer_type : c_caffe_layer_types)
    {
        if (tags.find(caffe_layer_type) != tags.end())
        {
            if (is_caffe_layer)
            {
                throw runtime_error("Found more than one Caffe layer type for serialization.");
            }
            else
            {
                layer_type = caffe_layer_type;
                is_caffe_layer = true;
            }
        }
    }

    if (is_caffe_layer)
    {
        return layer_type;
    }
    else
    {
        throw runtime_error("Couldn't serialize layer type.");
    }
}

static string GetTopBlobName(shared_ptr<Node> top_node)
{
    wstring_convert<codecvt_utf8<wchar_t>> wchar_to_char;
    return wchar_to_char.to_bytes(top_node->GetName());
}

static string GetBottomBlobName(shared_ptr<Node> bottom_node)
{
    const NodeTag tag = GetLayerType(bottom_node);
    wstring_convert<codecvt_utf8<wchar_t>> wchar_to_char;
    string top_blob_name = wchar_to_char.to_bytes(bottom_node->GetName());
    if (NodeTag::BatchNorm == tag)
    {
        top_blob_name += ".Scale";
    }
    return top_blob_name;
}

static caffe::LayerParameter* AddLayer(caffe::NetParameter& net_param, const std::string& layer_name)
{
    caffe::LayerParameter* layer = net_param.add_layer();
    CHECK(nullptr != layer);
    layer->Clear();
    layer->clear_blobs();
    layer->set_name(layer_name);
    return layer;
}

static void NodeToProto(
    shared_ptr<Node> layer,
    caffe::NetParameter& net_param,
    bool save_weights)
{
    CHECK(layer != nullptr);
    // Fill layer parameter.
    caffe::LayerParameter* param = AddLayer(net_param, GetTopBlobName(layer));

    // Set bottom blobs.
    for (shared_ptr<Node> bottom : layer->GetBottomConnections())
    {
        param->add_bottom(GetBottomBlobName(bottom));
    }

    // Set top blob.
    param->add_top(GetTopBlobName(layer));

    // Set layer specific parameters.
    const NodeTag layer_type = GetLayerType(layer);
    switch (layer_type)
    {
    case NodeTag::AveragePooling:
        SetPoolingLayerParams<cntk::AveragePoolingNode<float>>(layer, *param);
        break;
    case NodeTag::BatchNorm:
        {
            SetBatchNormLayerParams(layer, *param, save_weights);
            caffe::LayerParameter* scale_param = AddLayer(net_param, GetBottomBlobName(layer));
            scale_param->add_bottom(GetTopBlobName(layer));
            scale_param->add_top(GetBottomBlobName(layer));
            SetBatchNormLayerScaleParams(layer, *scale_param, save_weights);
        }
        break;
    case NodeTag::Convolution:
        SetConvolutionLayerParams(layer, *param, save_weights);
        break;
#ifdef CONVERT_CROP_NODE
    case NodeTag::Crop:
        SetCropLayerParams(layer, *param);
        break;
#endif
    case NodeTag::Eltwise:
        param->set_type("Eltwise");
        break;
    case NodeTag::InputValue:
        SetInputLayerParams(layer, *param);
        break;
    case NodeTag::InnerProduct:
        SetInnerProductLayerParams(layer, *param, save_weights);
        break;
    case NodeTag::MaxPooling:
        SetPoolingLayerParams<cntk::MaxPoolingNode<float>>(layer, *param);
        break;
    case NodeTag::Pooling:
        SetPoolingLayerParams(layer, *param);
        break;
    case NodeTag::ReLU:
        param->set_type("ReLU");
        break;
    case NodeTag::Sigmoid:
        param->set_type("Sigmoid");
        break;
    default:
        throw runtime_error("Couldn't serialize layer type.");
    }
}

static int64_t GetId(shared_ptr<Node> node)
{
    return reinterpret_cast<int64_t>(node.get());
}

// Caffe requires that bottom blobs are defined before they are used.
// Here, we create save order where we ensure that bottom layer is saved before all of its top layers.
static vector<shared_ptr<Node>> GetSaveOrder(shared_ptr<Node> root_node)
{
    vector<shared_ptr<Node>> save_order;
    queue<shared_ptr<Node>> layers;
    unordered_map<int64_t, int> node_level;
    layers.push(root_node);
    node_level[GetId(root_node)] = 0;
    while (!layers.empty())
    {
        shared_ptr<Node> layer = layers.front();
        layers.pop();
        CHECK(layer != nullptr);
        int current_node_level = node_level[GetId(layer)];

        // Add child layers.
        const auto& child_layers = layer->GetBottomConnections();
        for (const shared_ptr<Node>& child : child_layers)
        {
            const int64_t child_id = GetId(child);
            // operator[] will create entry with default value (0 for int) if entry doesn't exist.
            if (current_node_level + 1 > node_level[child_id])
            {
                node_level[child_id] = current_node_level + 1;
                layers.push(child);
            }
        }

        if (save_order.cend() == find(save_order.cbegin(), save_order.cend(), layer))
        {
            // In residual networks, one layer can have multiple parent nodes.
            // We do not want to save layer more than once.
            save_order.push_back(layer);
        }
    }

    sort(save_order.begin(), save_order.end(), [&](shared_ptr<Node> node1, shared_ptr<Node> node2)
    {
        const int node1_level = node_level[GetId(node1)];
        const int node2_level = node_level[GetId(node2)];
        return node1_level > node2_level;
    });

    return save_order;
}

static void LayeredArchitectureToProto(
    shared_ptr<Node> root_node,
    caffe::NetParameter& param,
    bool save_weights = true)
{
    param.Clear();
    vector<shared_ptr<Node>> layers = GetSaveOrder(root_node);
    for (auto layer = layers.begin(); layer != layers.end(); layer++)
    {
        NodeToProto(*layer, param, save_weights);
    }
}

void NodeSaver::Save(shared_ptr<Node> root_node, const string& file)
{
    // Ensure output folder exists.
    boost::filesystem::path output(file);
    boost::filesystem::create_directories(output.parent_path());

    // Save model with weights.
    caffe::NetParameter net_param;
    LayeredArchitectureToProto(root_node, net_param, true);
    WriteProtoToBinaryFile(net_param, file);

    // Save model without weights.
    caffe::NetParameter filtered_net;
    LayeredArchitectureToProto(root_node, filtered_net, false);
    ofstream model_desc(file + ".model");
    model_desc << filtered_net.DebugString();
}