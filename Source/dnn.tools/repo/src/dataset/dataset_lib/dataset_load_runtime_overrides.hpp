#pragma once

#include "dataset_io.hpp"

#include <vector>

class DsLoadParameters;

// Applies given set of runtime overrides on top of load parameters from config file.
void ApplyRuntimeOverrides(DsLoadParameters& load_parameters, std::vector<OverridableParam>* runtime_params);