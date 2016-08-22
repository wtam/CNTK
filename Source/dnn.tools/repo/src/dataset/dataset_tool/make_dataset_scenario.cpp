#include "make_dataset_scenario.hpp"

#include "dataset_io.hpp"

using namespace std;

static const string c_input_param = "input";
static const string c_input_param_short = "i";
static const string c_input_param_desc = "Path to the input config file.";

static const string c_output_param = "output";
static const string c_output_param_short = "o";
static const string c_output_param_desc = "Output dataset path.";

vector<ArgumentDesc> MakeDatasetScenario::GetCommandLineOptions()
{
  // We expect input config path and output ds path.
  return{
    { c_input_param, c_input_param_short, c_input_param_desc },
    { c_output_param, c_output_param_short, c_output_param_desc }
  };
}

void MakeDatasetScenario::Run(std::unordered_map<string, string>& arguments)
{
  // Take parameters.
  string input_prototxt_config_path = arguments.find(c_input_param)->second;
  string output_ds_path = arguments.find(c_output_param)->second;

  // Initiate dataset creation.
  MakeDataset(input_prototxt_config_path, output_ds_path);
}