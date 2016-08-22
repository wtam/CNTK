#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Describes command line argument.
struct ArgumentDesc
{
  std::string name;         // Name of the command line argument (if "xxx" command line must be invoked with "--xxx value")..
  std::string short_name;   // Short name of the command lien argument (if "o" command line must be invoked with "-o value").
  std::string description;  // Description of the command line argument.
};

// Base class for all command line scenarios. Each derived class if expected to also provide two static methods:
//
// static string GetScenarioName();
// static Scenario* Create();
//
// First one returns string which identifies scenario in command line. Second one is the factory function for the
// derived class.
// Derived class needs to provide descriptors for expected command line arguments - GetCommandLineOptions method.
// Once this class performs parsing derived class will be provided with command line arguement values - Run method.
// Before invoking ParseAndRun allowed scenarios need to be registered using RegisterScenario method.
//
// Given previous usual pattern would be:
// 
// Scenario::RegisterScenario<Scenario1>();
// Scenario::RegisterScenario<Scenario1>();
// ...
// Scenario::ParseAndRun(argc, argv); -> will invoke appropriate scenario based on command line arguments.
class Scenario
{
public:
  template <class T>
  static void RegisterScenario()
  {
    scenarios_map_.insert({ T::GetScenarioName(), T::Create });
  }

  static void ParseAndRun(int argc, char* argv[]);

  virtual ~Scenario() {}

private:
  virtual std::vector<ArgumentDesc> GetCommandLineOptions() = 0;

  virtual void Run(std::unordered_map<std::string, std::string>& arguments) = 0;

  static std::unique_ptr<Scenario> GetScenario(int argc, char* argv[]);

private:
  typedef std::unique_ptr<Scenario> (*ScenarioFactory)();
  static std::unordered_map<std::string, ScenarioFactory> scenarios_map_;
};