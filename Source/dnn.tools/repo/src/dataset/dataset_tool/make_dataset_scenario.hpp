#pragma once

#include "scenario.hpp"

#include <memory>
#include <string>
#include <vector>

// Creates image dataset based on the config file.
class MakeDatasetScenario : public Scenario
{
public:
  static std::string GetScenarioName() { return "makeds"; }

  static std::unique_ptr<Scenario> Create() { return std::unique_ptr<Scenario>(new MakeDatasetScenario()); }

  virtual std::vector<ArgumentDesc> GetCommandLineOptions() override;

  virtual void Run(std::unordered_map<std::string, std::string>& arguments) override;
};