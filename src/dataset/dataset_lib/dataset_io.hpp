// Main interface file for dataset library.
#pragma once

#include <string>
#include <vector>
#include <memory>

class DatasetEventsSink;

// Creates dataset given the config file and output path.
void MakeDataset(const std::string& config_file_path, const std::string& out_ds_file_path);

// Declaration of example exported by loader.
template <typename Dtype>
class IExample
{
public:
  virtual void ReshapeBlob(int index, int channels, int height, int width) = 0;

  virtual Dtype* GetBlobMemory(int index) = 0;
};

// Manages dataset loading.
template <typename Dtype>
class IDsLoader
{
public:
  virtual ~IDsLoader() {};

  virtual int GetBlobsCount() = 0;

  virtual const char* GetBlobName(int index) = 0;

  virtual int GetExamplesCount() = 0;

  virtual void GetExample(IExample<Dtype>* out_example) = 0;

  virtual const char* GetConfiguration() = 0;
};

// IDs for available overridable parameters.
enum class OverridableParamID
{
  source_path = 0,  // Path to ids file.
  loader_index,     // In case of distributed reading denotes the index of the reader.
  loaders_count,    // In case of distributed reading number of readers.
  count             // Number of allowed overridable parameters.
};

// Describes how to override parameter.
struct OverridableParam
{
  OverridableParamID id;  // ID of parameter to derive.
  std::string value;      // New parameter value.
};

template <typename Dtype>
std::unique_ptr<IDsLoader<Dtype>> CreateLoader(
  const std::string& params_file_path,            // Path to the load config file.
  std::vector<OverridableParam>* runtime_params,  // Optional runtime overrides.
  std::unique_ptr<DatasetEventsSink> events_sink  // Optional dataset events sink.
  );
