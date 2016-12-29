#include "make_dataset_scenario.hpp"

#include "check.hpp"
#include "dataset_io.hpp"

#include <algorithm>

using namespace std;

static const string c_input_param = "input";
static const string c_input_param_short = "i";
static const string c_input_param_desc = "Path to the input config file.";

static const string c_output_param = "output";
static const string c_output_param_short = "o";
static const string c_output_param_desc = "Output dataset path.";

static const string c_mode_param = "mode";
static const string c_mode_param_short = "m";
static const string c_mode_param_desc = "Mode of the run with respect to image read errors.";

vector<ArgumentDesc> MakeDatasetScenario::GetCommandLineOptions()
{
  // We expect input config path and output ds path.
  return{
    { c_input_param, c_input_param_short, c_input_param_desc },
    { c_output_param, c_output_param_short, c_output_param_desc },
    { c_mode_param, c_mode_param_short, c_mode_param_desc }
  };
}

void MakeDatasetScenario::Run(std::unordered_map<string, string>& arguments)
{
  // Take parameters.
  string input_prototxt_config_path = arguments.find(c_input_param)->second;
  string output_ds_path = arguments.find(c_output_param)->second;
  string run_mode = arguments.find(c_mode_param)->second;
  transform(run_mode.begin(), run_mode.end(), run_mode.begin(), ::tolower);
  CHECK(run_mode == "strict" || run_mode == "loose", "Valid option for \"%s\" option is either 'strict' or 'loose'", c_mode_param.c_str());
  bool strict_mode = run_mode == "strict";

  // Initiate dataset creation.
  MakeDataset(input_prototxt_config_path, output_ds_path, strict_mode);
}