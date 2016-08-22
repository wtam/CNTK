#include "scenario.hpp"
#include "check_decoding_scenario.hpp"
#include "make_dataset_scenario.hpp"
#include "test_load_dataset_scenario.hpp"

// Main entry point for different ds tool scenarios.
int main(int argc, char* argv[])
{
#ifdef __MSC_VER
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
  //_CrtSetBreakAlloc(...); // To be used for memory leaks detection.
#endif

  Scenario::RegisterScenario<TestLoadingScenario>();
  Scenario::RegisterScenario<MakeDatasetScenario>();
  Scenario::RegisterScenario<CheckDecodingScenario>();

  Scenario::ParseAndRun(argc, argv);

  return 0;
}
