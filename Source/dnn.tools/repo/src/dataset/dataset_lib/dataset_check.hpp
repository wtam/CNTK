#pragma once

#include <string>

// Report to standard output all image files specified by config file that cannot be decoded.
// Non-image files are ignored.
void CheckDecoding(const std::string& config_file_path);