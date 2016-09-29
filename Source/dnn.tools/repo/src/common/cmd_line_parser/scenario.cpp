#include "scenario.hpp"
#include "check.hpp"

#include "boost/program_options.hpp" 

using namespace std;
using namespace boost::program_options;

unordered_map<string, Scenario::ScenarioFactory> Scenario::scenarios_map_;

const string c_scenario_param = "scenario";
const string c_scenario_param_short = "s";
const string c_scenario_param_desc = "Defines the name of the scenario to run.";

options_description GetBaseOptions()
{
  options_description options_desc_base;
  options_desc_base.add_options()(
    (c_scenario_param + "," + c_scenario_param_short).c_str(),
    value<string>(),
    c_scenario_param_desc.c_str()
    );
  return options_desc_base;
}

void Scenario::ParseAndRun(int argc, char* argv[])
{
  // Get desired scenario object based on command line.
  unique_ptr<Scenario> scenario = GetScenario(argc, argv);

  // Create descriptions based on derived + base descriptions.
  options_description options_desc_base = GetBaseOptions();

  vector<ArgumentDesc> derived_arguments = scenario->GetCommandLineOptions();

  options_description options_desc_derived;
  for (size_t i = 0; i < derived_arguments.size(); i++)
  {
    options_desc_derived.add_options()(
      (derived_arguments[i].name + "," + derived_arguments[i].short_name).c_str(),
      value<string>(),
      derived_arguments[i].description.c_str()
      );
  }

  options_description options_desc_all;
  options_desc_all.add(options_desc_base).add(options_desc_derived);

  variables_map options_map;
  store(command_line_parser(argc, argv).options(options_desc_all).run(), options_map);
  CHECK(options_map.count(c_scenario_param) == 1, "Missing scenario parameter.");
  options_map.erase(c_scenario_param);

  unordered_map<string, string> final_arguments;
  for (auto iter : options_map)
  {
    final_arguments.insert({ iter.first, options_map[iter.first].as<string>() });
  }

  scenario->Run(final_arguments);
}

unique_ptr<Scenario> Scenario::GetScenario(int argc, char* argv[])
{
  options_description options_desc = GetBaseOptions();

  variables_map scenarios_opt_map;
  store(command_line_parser(argc, argv).options(options_desc).allow_unregistered().run(), scenarios_opt_map);

  CHECK(scenarios_opt_map.size() == 1, "Missing scenario parameter.");

  string scenario_name = scenarios_opt_map[c_scenario_param].as<string>();

  auto scenario_desc = scenarios_map_.find(scenario_name);
  if (scenario_desc != scenarios_map_.end())
  {
    // We have proper scenario, create it and perform additional parsing.
    return scenario_desc->second();
  }
  else
  {
    CHECK(false, "ERROR: Unknown scenario %s.", scenario_name.c_str());
  }
  return nullptr;
}
