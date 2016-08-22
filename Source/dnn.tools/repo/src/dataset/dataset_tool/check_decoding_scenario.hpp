#pragma once

#include "scenario.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Reports to standard output all images specified by config file that cannot be decoded.
// Ignores non-image files.
class CheckDecodingScenario : public Scenario
{
public:
  static std::string GetScenarioName() { return "checkdecode"; }

  static std::unique_ptr<Scenario> Create() { return std::unique_ptr<Scenario>(new CheckDecodingScenario()); }

  virtual std::vector<ArgumentDesc> GetCommandLineOptions() override;

  virtual void Run(std::unordered_map<std::string, std::string>& arguments) override;
};