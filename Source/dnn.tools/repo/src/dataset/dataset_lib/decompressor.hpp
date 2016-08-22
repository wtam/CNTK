// Declares interface for image decompression.
#pragma once

enum class Compression;

// Performs decompression of the input data described with in_* parameters and stores result in out buffer.
// Caller is responsible to allocated enough memory for out and work buffers (at least out_channels * out_height * out_width).
void Decompress(
  const char* in,
  int in_size,
  int out_channels,
  int out_height,
  int out_width,
  float* out,
  float* work,
  Compression compression
  );