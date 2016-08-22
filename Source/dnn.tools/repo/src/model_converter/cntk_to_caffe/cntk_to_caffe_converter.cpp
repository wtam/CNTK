#include "cntk_to_caffe_converter.h"
#include "cntk_postfix_iterator.h"
#include "check.hpp"
#include "device_info.h"
#include "node.h"
#include "node_factory.h"
#include "node_saver.h"
#include "nodes_processor.h"
#include "reduce_rules.h"

#include "cntk_includes.h"

#include <list>
#include <memory>
#include <vector>

namespace cntk = Microsoft::MSR::CNTK;
using namespace std;

typedef float NetworkWeightType;

#ifndef DEBUG_REDUCE_RULES
// Comment to avoid printing state of stack to console on every stack change.
#define DEBUG_REDUCE_RULES
#endif

#ifdef DEBUG_REDUCE_RULES
void PrintStack(const list<shared_ptr<Node>>& nodes)
{
    for (auto node = nodes.rbegin(); node != nodes.rend(); node++)
    {
        const auto& tags = (*node)->GetTags();
        cout << "[";
        bool is_first = true;
        for (const auto& tag : tags)
        {
            if (!is_first)
            {
                cout << " ";
            }
            is_first = false;
            cout << ToString(tag);
        }
        cout << "]";
    }
    cout << endl;
}
#endif

class RuleBasedNodeProcessor : public NodesProcessor
{
public:
    RuleBasedNodeProcessor(NodeFactory* factory) : factory_(factory)
    {
        CHECK(factory_ != nullptr);
    }

    bool CanBeApplied(const std::list<std::shared_ptr<Node>>& sequence) override
    {
        return nullptr != GetApplicableRule(sequence);
    }

    ~RuleBasedNodeProcessor() override = default;

protected:
    const ReduceRule* GetApplicableRule(const list<shared_ptr<Node>>& nodes) const
    {
        for (const auto& rule : rules_)
        {
            CHECK(nullptr != rule);
            if (rule->CanBeApplied(nodes))
            {
                return rule.get();
            }
        }
        return nullptr;
    }

    vector<unique_ptr<ReduceRule>> rules_;
    NodeFactory* factory_;
};

// Replaces CNTK nodes used for describing input layer with scale transformation with single node that can be directly
// transformed to Caffe layer.
class InputNodeWithScaleTransformProcessor : public RuleBasedNodeProcessor
{
public:
    InputNodeWithScaleTransformProcessor(NodeFactory* factory) : RuleBasedNodeProcessor(factory)
    {
        Filter<NodeTag> input_layer_filter({ NodeTag::InputValue }, { NodeTag::ScaleParam });
        Filter<NodeTag> learnable_parameter_filter({ NodeTag::LearnableParameter });
        Filter<NodeTag> element_times_filter({ NodeTag::ElementTimes }, { NodeTag::IsLayer });

        unordered_set<NodeTag> product{ NodeTag::IsLayer, NodeTag::InputValue, NodeTag::ScaleParam };

        rules_.emplace_back(
            make_unique<ReduceRule>(
            vector<Filter<NodeTag>>{element_times_filter, learnable_parameter_filter, input_layer_filter },
            unordered_set<NodeTag>(product)));
        rules_.emplace_back(
            make_unique<ReduceRule>(
            vector<Filter<NodeTag>>{element_times_filter, input_layer_filter, learnable_parameter_filter },
            unordered_set<NodeTag>(product)));
    }

    void Apply(list<shared_ptr<Node>>& sequence) override
    {
        const ReduceRule* rule = GetApplicableRule(sequence);
        CHECK(rule != nullptr);

        std::shared_ptr<Node> element_times = sequence.front();
        sequence.pop_front();

        cntk::ComputationNodeBasePtr scale_param = nullptr;
        cntk::ComputationNodeBasePtr input = nullptr;

        if (rule == rules_[0].get())
        {
            scale_param = sequence.front()->GetCntkHeadNode();
            sequence.pop_front();
            input = sequence.front()->GetCntkHeadNode();
            sequence.pop_front();
        }
        else
        {
            CHECK(rule == rules_[1].get());
            input = sequence.front()->GetCntkHeadNode();
            sequence.pop_front();
            scale_param = sequence.front()->GetCntkHeadNode();
            sequence.pop_front();
        }

        element_times->SetTags(rule->GetResolvedTags());
        element_times->AddAttribute(NodeAttribute::Input, input);
        element_times->AddAttribute(NodeAttribute::Scale, scale_param);
        sequence.push_front(element_times);
    }

    ~InputNodeWithScaleTransformProcessor() override = default;
};

// Replaces CNTK nodes used for describing architecture where there is binary operation which involves learnable
// parameter as one argument with single node that can be directly transformed to Caffe layer or is intermediate result
// in constructing Caffe layer. 
class BinaryOpWithLearnableParameterProcessor : public RuleBasedNodeProcessor
{
public:
    BinaryOpWithLearnableParameterProcessor(NodeFactory* factory) : RuleBasedNodeProcessor(factory)
    {
        AddRules(NodeTag::Times, unordered_set<NodeTag>{ NodeTag::InnerProduct, NodeTag::IsLayer });
        AddRules(NodeTag::Convolution, unordered_set<NodeTag>{ NodeTag::Convolution, NodeTag::IsLayer });
    }

    void Apply(list<shared_ptr<Node>>& sequence) override
    {
        const ReduceRule* rule = GetApplicableRule(sequence);
        CHECK(rule != nullptr);

        shared_ptr<Node> operation_node = sequence.front();
        sequence.pop_front();

        cntk::ComputationNodeBasePtr weights = nullptr;
        shared_ptr<Node> input = nullptr;
        if (IsLearnableParameterFirst(rule))
        {
            weights = sequence.front()->GetCntkHeadNode();
            sequence.pop_front();
            input = sequence.front();
            sequence.pop_front();
        }
        else
        {
            input = sequence.front();
            sequence.pop_front();
            weights = sequence.front()->GetCntkHeadNode();
            sequence.pop_front();
        }

        operation_node->SetTags(rule->GetResolvedTags());
        operation_node->AddAttribute(NodeAttribute::Input, input->GetCntkHeadNode());
        operation_node->AddAttribute(NodeAttribute::Weights, weights);
        operation_node->AddAttribute(NodeAttribute::Operation, operation_node->GetCntkHeadNode());
        operation_node->AddBottomConnection(input);
        input->AddTopConnection(operation_node);
        sequence.push_front(operation_node);
    }

    ~BinaryOpWithLearnableParameterProcessor() override = default;

private:
    bool IsLearnableParameterFirst(const ReduceRule* rule) const
    {
        bool is_learnable_param_first = false;
        for (size_t i = 0; i < rules_.size(); i += 2)
        {
            if (rules_[i].get() == rule)
            {
                is_learnable_param_first = true;
                break;
            }
        }
        return is_learnable_param_first;
    }

    void AddRules(const NodeTag& operation_tag, const unordered_set<NodeTag>& product_tags)
    {
        Filter<NodeTag> input_layer_filter({ NodeTag::IsLayer });
        Filter<NodeTag> learnable_parameter_filter({ NodeTag::LearnableParameter });
        Filter<NodeTag> operation_filter({ operation_tag }, { NodeTag::IsLayer });
        rules_.emplace_back(
            make_unique<ReduceRule>(
            vector<Filter<NodeTag>>{operation_filter, learnable_parameter_filter, input_layer_filter },
            unordered_set<NodeTag>(product_tags)));
        rules_.emplace_back(
            make_unique<ReduceRule>(
            vector<Filter<NodeTag>> {operation_filter, input_layer_filter, learnable_parameter_filter },
            unordered_set<NodeTag>(product_tags)));
    }
};

// Rule for constructing nodes that can be directly serialized as Caffe layer and which contain bias term.
class AddBiasProcessor : public RuleBasedNodeProcessor
{
public:
    AddBiasProcessor(NodeFactory* factory) : RuleBasedNodeProcessor(factory)
    {
        AddRules(NodeTag::InnerProduct, NodeTag::InnerProduct);
        AddRules(NodeTag::Convolution, NodeTag::Convolution);
    }

    void Apply(list<shared_ptr<Node>>& nodes) override
    {
        const ReduceRule* rule = GetApplicableRule(nodes);
        CHECK(rule != nullptr);

        std::shared_ptr<Node> plus = nodes.front();
        nodes.pop_front();

        cntk::ComputationNodeBasePtr bias = nullptr;
        shared_ptr<Node> non_bias_part = nullptr;

        if (IsBiasFirst(rule))
        {
            bias = nodes.front()->GetCntkHeadNode();
            nodes.pop_front();
            non_bias_part = nodes.front();
            nodes.pop_front();
        }
        else
        {
            non_bias_part = nodes.front();
            nodes.pop_front();
            bias = nodes.front()->GetCntkHeadNode();
            nodes.pop_front();
        }

        plus->SetTags(rule->GetResolvedTags());

        // Copy input and weights attributes.
        plus->SetAttributes(non_bias_part->GetAttributes());
        plus->AddAttribute(NodeAttribute::Bias, bias);

        vector<shared_ptr<Node>> inputs = non_bias_part->GetBottomConnections();
        CHECK(inputs.size() == 1);
        shared_ptr<Node> input = inputs.front();
        input->RemoveTopConnection(non_bias_part);
        input->AddTopConnection(plus);
        plus->AddBottomConnection(input);

        nodes.push_front(plus);
    }

    ~AddBiasProcessor() override = default;

private:
    bool IsBiasFirst(const ReduceRule* rule) const
    {
        bool is_bias_first = false;
        for (size_t i = 0; i < rules_.size(); i += 2)
        {
            if (rules_[i].get() == rule)
            {
                is_bias_first = true;
                break;
            }
        }
        return is_bias_first;
    }

    void AddRules(const NodeTag& input_layer_tag, const NodeTag& product_tag)
    {
        Filter<NodeTag> input_layer_filter({ input_layer_tag }, { NodeTag::HasBiasTerm });
        Filter<NodeTag> bias_node_filter({ NodeTag::LearnableParameter });
        Filter<NodeTag> plus_filter({ NodeTag::Plus }, { NodeTag::IsLayer });
        unordered_set<NodeTag> product{ product_tag, NodeTag::IsLayer, NodeTag::HasBiasTerm };

        assert(rules_.size() % 2 == 0); // There should be N x 2 rules (2 due to commutative + op).
        rules_.emplace_back(make_unique<ReduceRule>(
            vector<Filter<NodeTag>>{ plus_filter, bias_node_filter, input_layer_filter },
            unordered_set<NodeTag>(product)));
        rules_.emplace_back(
            make_unique<ReduceRule>(
            vector<Filter<NodeTag>> { plus_filter, input_layer_filter, bias_node_filter },
            unordered_set<NodeTag>(product)));
    }
};

// Rules for replacing CNTK nodes which can be directly mapped to some Caffe layer which has one input and one output
// blob.
class DirectMappingUnaryProcessors : public RuleBasedNodeProcessor
{
public:
    DirectMappingUnaryProcessors(NodeFactory* factory) : RuleBasedNodeProcessor(factory)
    {
        rules_.emplace_back(CreateDirectingMappingRule(NodeTag::AveragePooling));
        rules_.emplace_back(CreateDirectingMappingRule(NodeTag::MaxPooling));
        rules_.emplace_back(CreateDirectingMappingRule(NodeTag::Pooling));
        rules_.emplace_back(CreateDirectingMappingRule(NodeTag::ReLU));
        rules_.emplace_back(CreateDirectingMappingRule(NodeTag::Sigmoid));
    }

    void Apply(list<shared_ptr<Node>>& nodes) override
    {
        const ReduceRule* rule = GetApplicableRule(nodes);
        CHECK(rule != nullptr);

        shared_ptr<Node> head_node = nodes.front();
        nodes.pop_front();
        shared_ptr<Node> input_layer = nodes.front();
        nodes.pop_front();

        head_node->SetTags(rule->GetResolvedTags());
        input_layer->AddTopConnection(head_node);
        head_node->AddBottomConnection(input_layer);

        nodes.push_front(head_node);
    }

    ~DirectMappingUnaryProcessors() override = default;
protected:
    static unique_ptr<ReduceRule> CreateDirectingMappingRule(NodeTag tag)
    {
        Filter<NodeTag> input_layer({ NodeTag::IsLayer });
        Filter<NodeTag> operation({ tag }, { NodeTag::IsLayer });
        unordered_set<NodeTag> product{ tag, NodeTag::IsLayer };
        return make_unique<ReduceRule>(vector<Filter<NodeTag>>{ operation, input_layer }, move(product));
    }
};

// Replaces CNTK input node with node that corresponds to Caffe data layer.
class InputNodeProcessor : public RuleBasedNodeProcessor
{
public:
    InputNodeProcessor(NodeFactory* factory) : RuleBasedNodeProcessor(factory)
    {
        Filter<NodeTag> input_layer({ NodeTag::InputValue }, { NodeTag::IsLayer });
        unordered_set<NodeTag> product{ NodeTag::InputValue, NodeTag::IsLayer };
        rules_.emplace_back(
            make_unique<ReduceRule>(vector<Filter<NodeTag>>{ input_layer }, unordered_set<NodeTag>(product)));
    }

    void Apply(list<shared_ptr<Node>>& nodes) override
    {
        const ReduceRule* rule = GetApplicableRule(nodes);
        CHECK(rule != nullptr);
        cntk::ComputationNodeBasePtr head_node = nodes.front()->GetCntkHeadNode();
        nodes.front()->AddAttribute(NodeAttribute::Input, head_node);
        nodes.front()->SetTags(rule->GetResolvedTags());
    }

    ~InputNodeProcessor() override = default;
};

// Replaces CNTK nodes used for describing batch normalization and scale layers with single node that can serialized
// to two Caffe layer.
class BatchNormalizationProcessors : public RuleBasedNodeProcessor
{
public:
    BatchNormalizationProcessors(NodeFactory* factory) : RuleBasedNodeProcessor(factory)
    {
        Filter<NodeTag> batch_norm({ NodeTag::BatchNorm }, { NodeTag::IsLayer });
        Filter<NodeTag> inv_std_dev({ NodeTag::LearnableParameter });
        Filter<NodeTag> mean({ NodeTag::LearnableParameter });
        Filter<NodeTag> bias({ NodeTag::LearnableParameter });
        Filter<NodeTag> scale({ NodeTag::LearnableParameter });
        Filter<NodeTag> input_layer({ NodeTag::IsLayer });
        unordered_set<NodeTag> product{ NodeTag::BatchNorm, NodeTag::IsLayer };
        vector<Filter<NodeTag>> filters{ batch_norm, inv_std_dev, mean, bias, scale, input_layer };
        rules_.emplace_back(make_unique<ReduceRule>(move(filters), unordered_set<NodeTag>(product)));
    }

    void Apply(list<shared_ptr<Node>>& nodes) override
    {
        const ReduceRule* rule = GetApplicableRule(nodes);
        CHECK(rule != nullptr);
        shared_ptr<Node> batch_norm = nodes.front();
        nodes.pop_front();
        shared_ptr<Node> inv_std_dev = nodes.front();
        nodes.pop_front();
        shared_ptr<Node> mean = nodes.front();
        nodes.pop_front();
        shared_ptr<Node> bias = nodes.front();
        nodes.pop_front();
        shared_ptr<Node> scale = nodes.front();
        nodes.pop_front();
        shared_ptr<Node> input_layer = nodes.front();
        nodes.pop_front();

        batch_norm->SetTags(rule->GetResolvedTags());
        input_layer->AddTopConnection(batch_norm);
        batch_norm->AddBottomConnection(input_layer);
        batch_norm->AddAttribute(NodeAttribute::InvStdDev, inv_std_dev->GetCntkHeadNode());
        batch_norm->AddAttribute(NodeAttribute::Mean, mean->GetCntkHeadNode());
        batch_norm->AddAttribute(NodeAttribute::Bias, bias->GetCntkHeadNode());
        batch_norm->AddAttribute(NodeAttribute::Scale, scale->GetCntkHeadNode());
        nodes.push_front(batch_norm);
    }

    ~BatchNormalizationProcessors() override = default;
};

// Rules for replacing CNTK nodes which can be directly mapped to some Caffe layer which has two input blobs and one
// output blob.
class DirectMappingBinaryProcessor : public RuleBasedNodeProcessor
{
public:
    DirectMappingBinaryProcessor(NodeFactory* factory) : RuleBasedNodeProcessor(factory)
    {
        rules_.emplace_back(CreateDirectingMappingRule(NodeTag::Plus, NodeTag::Eltwise));
    }

    void Apply(list<shared_ptr<Node>>& nodes) override
    {
        const ReduceRule* rule = GetApplicableRule(nodes);
        CHECK(rule != nullptr);
        shared_ptr<Node> operation = nodes.front();
        nodes.pop_front();
        shared_ptr<Node> operand_1 = nodes.front();
        nodes.pop_front();
        shared_ptr<Node> operand_2 = nodes.front();
        nodes.pop_front();

        operation->SetTags(rule->GetResolvedTags());
        operand_1->AddTopConnection(operation);
        operand_2->AddTopConnection(operation);
        operation->AddBottomConnection(operand_1);
        operation->AddBottomConnection(operand_2);
        nodes.push_front(operation);
    }

    ~DirectMappingBinaryProcessor() override = default;
protected:
    static unique_ptr<ReduceRule> CreateDirectingMappingRule(NodeTag operation_tag, NodeTag result_tag)
    {
        Filter<NodeTag> operation({ operation_tag }, { NodeTag::IsLayer });
        Filter<NodeTag> operand_1({ NodeTag::IsLayer });
        Filter<NodeTag> operand_2({ NodeTag::IsLayer });
        unordered_set<NodeTag> product{ result_tag, NodeTag::IsLayer };
        vector<Filter<NodeTag>> filters{ operation, operand_1, operand_2 };
        return make_unique<ReduceRule>(move(filters), unordered_set<NodeTag>(product));
    }
};

class CntkToCaffeConverter
{
public:
    CntkToCaffeConverter()
    {
        node_factory_ = make_unique<NodeFactory>();
        processors_.emplace_back(make_unique<AddBiasProcessor>(node_factory_.get()));
        processors_.emplace_back(make_unique<BatchNormalizationProcessors>(node_factory_.get()));
        processors_.emplace_back(make_unique<BinaryOpWithLearnableParameterProcessor>(node_factory_.get()));
        processors_.emplace_back(make_unique<DirectMappingBinaryProcessor>(node_factory_.get()));
        processors_.emplace_back(make_unique<DirectMappingUnaryProcessors>(node_factory_.get()));
        processors_.emplace_back(make_unique<InputNodeProcessor>(node_factory_.get()));
        processors_.emplace_back(make_unique<InputNodeWithScaleTransformProcessor>(node_factory_.get()));
    }

    bool ConvertCntkTree(const std::string& input_file, const std::string& output_file)
    {
        const wstring input_file_w(input_file.begin(), input_file.end());
        DEVICEID_TYPE deviceID = DeviceInfo::GetInstance().GetId();
        cntk::ComputationNetworkPtr net =
            cntk::ComputationNetwork::CreateFromFile<NetworkWeightType>(deviceID, input_file_w);

        CntkPostfixIterator iterator(net);
        list<shared_ptr<Node>> nodes;
        node_factory_->Clear();
        bool has_stack_changed = true;
        while (has_stack_changed)
        {
            has_stack_changed = false;
            // Reduce node sequence as much as possible.
            NodesProcessor* processor = GetApplicableProcessor(nodes);
            while (processor != nullptr)
            {
                processor->Apply(nodes);
                has_stack_changed = true;
#ifdef DEBUG_REDUCE_RULES
                PrintStack(nodes);
#endif
                processor = GetApplicableProcessor(nodes);
            }

            // We reduced node sequence using all rules that could be applied.
            // Move to next node.
            if (iterator.HasNext())
            {
                PushNext(iterator, nodes);
                has_stack_changed = true;
#ifdef DEBUG_REDUCE_RULES
                PrintStack(nodes);
#endif
            }
        }

        // We succeeded if all nodes from iterator have been processed and only one node (root) is left in the list.
        const bool is_converted = !iterator.HasNext() && nodes.size() == 1;
        if (is_converted)
        {
            NodeSaver saver;
            saver.Save(*nodes.begin(), output_file);
        }
        return is_converted;
    }

private:
    NodesProcessor* CntkToCaffeConverter::GetApplicableProcessor(const std::list<std::shared_ptr<Node>>& sequence)
    {
        NodesProcessor* applicable_processor = nullptr;
        for (const auto& processor : processors_)
        {
            if (processor->CanBeApplied(sequence))
            {
                CHECK(applicable_processor == nullptr, "Found more than one applicable reduce rule.");
                applicable_processor = processor.get();
            }
        }
        return applicable_processor;
    }

    void PushNext(CntkPostfixIterator& iterator, std::list<std::shared_ptr<Node>>& nodes)
    {
        CHECK(iterator.HasNext());
        cntk::ComputationNodeBasePtr cntk_node = iterator.GetNext();
        CHECK(cntk_node != nullptr);
        if (!node_factory_->HasNode(cntk_node))
        {
            // If not already visited, we create new node and set tags to it.
            unordered_set<NodeTag> cntk_node_tags{ GetTag(*cntk_node) };
            shared_ptr<Node> node = node_factory_->CreateNode(cntk_node);
            node->SetTags(cntk_node_tags);
        }
        shared_ptr<Node> node = node_factory_->GetNode(cntk_node);
        nodes.emplace_front(node);
    }

    std::unique_ptr<NodeFactory> node_factory_;
    std::vector<std::unique_ptr<NodesProcessor>> processors_;
};

bool ConvertCntkToCaffe(const string& input_file, const string& output_file)
{
    CntkToCaffeConverter converter;
    return converter.ConvertCntkTree(input_file, output_file);
}