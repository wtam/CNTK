#include "check_decoding_scenario.hpp"

#include "dataset_check.hpp"

using namespace std;

static const string c_input_param = "input";
static const string c_input_param_short = "i";
static const string c_input_param_desc = "Path to the input config file.";

vector<ArgumentDesc> CheckDecodingScenario::GetCommandLineOptions()
{
  // We expect input config path.
  return{
    { c_input_param, c_input_param_short, c_input_param_desc }
  };
}

void CheckDecodingScenario::Run(std::unordered_map<string, string>& arguments)
{
  // Take parameters.
  string input_prototxt_config_path = arguments.find(c_input_param)->second;

  // Initiate check.
  CheckDecoding(input_prototxt_config_path);
}