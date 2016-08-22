#pragma once

#include "scenario.hpp"

#include <string>
#include <unordered_map>
#include <vector>

// Performs loading of the dataset, useful for testing save/load functionality.
class TestLoadingScenario : public Scenario
{
public:
  static std::string GetScenarioName() { return "testload"; }

  static std::unique_ptr<Scenario> Create() { return std::unique_ptr<Scenario>(new TestLoadingScenario()); }

  virtual std::vector<ArgumentDesc> GetCommandLineOptions() override;

  virtual void Run(std::unordered_map<std::string, std::string>& arguments) override;
};