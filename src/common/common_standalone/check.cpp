#include "check.hpp"

#include <iostream>

using namespace std;

void CHECK(bool b)
{
  if (!b) throw;
}

void CHECK(bool b, const string& message)
{
  if (!b)
  {
    cerr << "ERROR: " << message << endl;
    throw;
  }
}