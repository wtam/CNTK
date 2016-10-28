#include "converter_version.h"
#include "ComputationNode.h"
#include <iostream>

using namespace std;

// The latest version of CNTK model that can be converted to Caffe.
// Newer version might work as well, but this has to be verified.
#define VERIFIED_CNTK_MODEL_VERSION CNTK_MODEL_VERSION_14

void CheckCntkVersion()
{
#if VERIFIED_CNTK_MODEL_VERSION < CURRENT_CNTK_MODEL_VERSION
        cout << "Warning: It is not verified that conversion for CNTK model version " << CURRENT_CNTK_MODEL_VERSION
             << " will work. The latest verified is version " << VERIFIED_CNTK_MODEL_VERSION << "." << endl;
#endif
}