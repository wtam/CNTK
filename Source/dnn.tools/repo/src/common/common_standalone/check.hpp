#pragma once

#include <string>

// Throws if given variable is false.
void CHECK(bool b);

// If given variable is false prints the message and throws.
void CHECK(bool b, const std::string& message);