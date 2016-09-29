#include "check.hpp"
#include "dataset.hpp"
#include "platform.hpp"
#include "tensor.hpp"

#include <istream>
#include <fstream>
#include <stdio.h>
#include <vector>

using namespace std;

enum class TensorType : int { Int, Float, Double };

istream& operator>>(istream& stream, TensorType& type)
{
  unsigned int value = 0;
  if (stream >> value)
  {
    type = static_cast<TensorType>(value);
  }
  return stream;
}

// Unpacks a tensor.
template <typename T>
void DecompressTensorValues(const char* in, int in_size, int out_channels, int out_height, int out_width, float* out)
{
  const int out_count = out_height * out_width * out_channels;
  CHECK(in_size == out_count * sizeof(T), "Invalid tensor input size %d (out_count is %d).", in_size, out_count);
  for (int i = 0; i < out_count; ++i)
  {
    out[i] = static_cast<float>(reinterpret_cast<const T*>(in)[i]);
  }
}

void DecompressTensor(
  const char* in,
  int in_size,
  int out_channels,
  int out_height,
  int out_width,
  float* out)
{
  // Decompress data type.
  const TensorType tensor_type = *reinterpret_cast<const TensorType*>(in);
  in += sizeof(TensorType);
  in_size -= sizeof(TensorType);
  switch (tensor_type)
  {
  case TensorType::Int:
    DecompressTensorValues<int>(in, in_size, out_channels, out_height, out_width, out);
    break;
  case TensorType::Float:
    DecompressTensorValues<float>(in, in_size, out_channels, out_height, out_width, out);
    break;
  case TensorType::Double:
    DecompressTensorValues<double>(in, in_size, out_channels, out_height, out_width, out);
    break;
  default:
    CHECK(false, "Unknown tensor type");
  }
}

template <typename T>
void ReadTensorValuesFromTextFile(ifstream& in_file, int count, vector<char>& buffer)
{
  CHECK(count > 0, "Invalid tensor values count %d (must be positive).", count);
  buffer.resize(count * sizeof(T));
  char* ptr = buffer.data();
  while (in_file.good())
  {
    T value;
    if (in_file >> value)
    {
      *reinterpret_cast<T*>(ptr) = value;
      ptr += sizeof(T);
      count--;
    }
  }
  CHECK(count == 0, "Invalid tensor values count %d after reading (0 expected).", count);
}

void ReadTensorFromTextFile(
  const string& file_path,
  TensorType& type,
  int& height,
  int& width,
  int& channels,
  vector<char>& buffer)
{
  ifstream in_file(file_path);
  CHECK(in_file.is_open(), "Cannot open text file %s", file_path.c_str());

  // Read data type and shape.
  CHECK(static_cast<bool>(in_file >> type >> height >> width >> channels), "Invalid format of tensor txt file.");

  const int count = height * width * channels;

  // Read values.
  switch (type)
  {
  case TensorType::Int:
    ReadTensorValuesFromTextFile<int>(in_file, count, buffer);
    break;
  case TensorType::Float:
    ReadTensorValuesFromTextFile<float>(in_file, count, buffer);
    break;
  case TensorType::Double:
    ReadTensorValuesFromTextFile<double>(in_file, count, buffer);
    break;
  default:
    CHECK(false, "Unknown tensor data type");
  }

  in_file.close();
}

template <typename T>
void ReadTensorValuesFromBinaryFile(FILE* in_file, int count, vector<char>& buffer)
{
  // Read values.
  buffer.resize(count * sizeof(T));
  char* ptr = buffer.data();
  CHECK(fread(ptr, sizeof(T), count, in_file) == count, "Reading tensor values from binary file failed.");
}

// Expected file format:
// <tensor_type>: a single value of type TensorType (int)
// <height>: a singe value of type int
// <width>: a single value of type int
// <channels>: a single value of type int
// <values>: a sequence of <height> * <width> * <channels> values whose type is determined by <tensor_type>
void ReadTensorFromBinaryFile(
  const string& file_path,
  TensorType& type,
  int& height,
  int& width,
  int& channels,
  vector<char>& buffer)
{
  FILE* in_file;
  CHECK(Platform::fopen_s(&in_file, file_path.c_str(), "rb") == 0,
    "Cannot open tensor binary file %s", file_path.c_str());
  CHECK(in_file != nullptr, "Cannot open tensor binary file %s", file_path.c_str());

  // Read data type and shape.
  CHECK(fread(&type, sizeof(TensorType), 1, in_file) == 1, "Reading tensor type from binary file failed.");
  CHECK(fread(&height, sizeof(int), 1, in_file) == 1, "Reading tensor height from binary file failed.");
  CHECK(fread(&width, sizeof(int), 1, in_file) == 1, "Reading tensor width from binary file failed.");
  CHECK(fread(&channels, sizeof(int), 1, in_file) == 1, "Reading tensor channels from binary file failed.");

  const int count = height * width * channels;

  // Read values.
  switch (type)
  {
  case TensorType::Int:
    ReadTensorValuesFromBinaryFile<int>(in_file, count, buffer);
    break;
  case TensorType::Float:
    ReadTensorValuesFromBinaryFile<float>(in_file, count, buffer);
    break;
  case TensorType::Double:
    ReadTensorValuesFromBinaryFile<double>(in_file, count, buffer);
    break;
  default:
    CHECK(false, "Unknown tensor data type");
  }

  fclose(in_file);
}

void WriteTensorToDataset(
  TensorType type,
  int height,
  int width,
  int channels,
  const vector<char>& buffer,
  FILE* out_file,
  ChannelSet& channelset_desc,
  ChannelSetInstance& channelset_inst)
{
  // Write data.
  channelset_desc.channels = channels;
  channelset_desc.compression = Compression::Tensor;

  channelset_inst.size = static_cast<int>(buffer.size() + sizeof(TensorType));
  channelset_inst.width = width;
  channelset_inst.height = height;

  fwrite(&type, sizeof(TensorType), 1, out_file);
  fwrite(buffer.data(), 1, buffer.size(), out_file);
}

// Serializes a 3D tensor to output file.
void TensorSerialize(
  const string& in_file_path,
  FILE* out_file,
  ChannelSet& channelset_desc,
  ChannelSetInstance& channelset_inst)
{
  // Reuse buffer across function calls.
  static vector<char> buffer;

  TensorType type = TensorType::Int;
  int height = 0;
  int width = 0;
  int channels = 0;

  const size_t last_dot_pos = in_file_path.find_last_of('.');
  CHECK(last_dot_pos != string::npos, "Invalid tensor file name %s (extension expected).", in_file_path.c_str());
  const string ext = in_file_path.substr(last_dot_pos);
  if (ext == ".txt")
  {
    ReadTensorFromTextFile(in_file_path, type, height, width, channels, buffer);
  }
  else if (ext == ".bin")
  {
    ReadTensorFromBinaryFile(in_file_path, type, height, width, channels, buffer);
  }
  else
  {
    CHECK(false, "Unknown extension of tensor file: %s", ext.c_str());
  }
  WriteTensorToDataset(type, height, width, channels, buffer, out_file, channelset_desc, channelset_inst);
}
