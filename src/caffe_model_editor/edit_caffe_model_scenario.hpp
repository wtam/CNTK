#pragma once

#include "scenario.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class EditCaffeModelScenario : public Scenario
{
public:
    static std::string GetScenarioName() { return "editmodel"; }

    static std::unique_ptr<Scenario> Create() { return std::unique_ptr<Scenario>(new EditCaffeModelScenario()); }

    virtual std::vector<ArgumentDesc> GetCommandLineOptions() override;

    virtual void Run(std::unordered_map<std::string, std::string>& arguments) override;
};