#include "scenario.hpp"
#include "edit_caffe_model_scenario.hpp"

// Main entry point for different Caffe model editor scenarios.
int main(int argc, char* argv[])
{
#ifdef _MSC_VER
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    //_CrtSetBreakAlloc(...); // To be used for memory leaks detection.
#endif

    Scenario::RegisterScenario<EditCaffeModelScenario>();

    Scenario::ParseAndRun(argc, argv);

    return 0;
}