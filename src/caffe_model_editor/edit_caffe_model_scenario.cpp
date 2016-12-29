#include "edit_caffe_model_scenario.hpp"

#include "caffe_model_editor.hpp"

using namespace std;

static const string c_input_param = "input";
static const string c_input_param_short = "i";
static const string c_input_param_desc = "Path to the input prototxt file.";

static const string c_config_param = "config";
static const string c_config_param_short = "c";
static const string c_config_param_desc = "Path to the edit configuration prototxt file.";

static const string c_output_param = "output";
static const string c_output_param_short = "o";
static const string c_output_param_desc = "Path to the output prototxt file.";

vector<ArgumentDesc> EditCaffeModelScenario::GetCommandLineOptions()
{
    return
    {
        { c_input_param, c_input_param_short, c_input_param_desc },
        { c_config_param, c_config_param_short, c_config_param_desc },
        { c_output_param, c_output_param_short, c_output_param_desc }
    };
}

void EditCaffeModelScenario::Run(unordered_map<string, string>& arguments)
{
    // Take parameters.
    string input_model_path = arguments.find(c_input_param)->second;
    string config_path = arguments.find(c_config_param)->second;
    string output_model_path = arguments.find(c_output_param)->second;

    // Invoke editing.
    EditCaffeModel(input_model_path, config_path, output_model_path);
}