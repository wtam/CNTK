#include "test_load_dataset_scenario.hpp"

#include "dataset_events_sink.hpp"
#include "dataset_io.hpp"

#include <array>

using namespace std;

static const string c_input_param = "input";
static const string c_input_param_short = "i";
static const string c_input_param_desc = "Input dataset for loading.";

// Helper class to use with dataset loading.
template <class Dtype>
class TestExample : public IExample<Dtype> {
public:
  TestExample(vector<const char*>&& blob_names) :
    blob_names_(move(blob_names)), blobs(blob_names_.size()),
    blob_shapes_(blob_names_.size()), blob_counts(blob_names_.size())
  {
  }

  virtual void ReshapeBlob(int index, int channels, int height, int width) override
  {
    static_assert(blob_dims == 3, "Invalid blob dims.");
    blob_shapes_[index][0] = channels;
    blob_shapes_[index][1] = height;
    blob_shapes_[index][2] = width;
    blob_counts[index] = channels * height* width;
    blobs[index].resize(blob_counts[index]);
  }

  virtual Dtype* GetBlobMemory(int index) override
  {
    return blobs[index].data();
  }

private:
  static const int blob_dims = 3;
  vector<const char*> blob_names_;
  vector<vector<Dtype>> blobs;
  vector<array<int, blob_dims>> blob_shapes_;
  vector<int> blob_counts;
};

vector<ArgumentDesc> TestLoadingScenario::GetCommandLineOptions()
{
  return { { c_input_param, c_input_param_short, c_input_param_desc } };
}

void TestLoadingScenario::Run(std::unordered_map<string, string>& arguments)
{
  string input_load_config_path = arguments.find(c_input_param)->second;

  unique_ptr<IDsLoader<float>> ds_loader = CreateLoader<float>(input_load_config_path, nullptr, nullptr);

  int blobs_count = ds_loader->GetBlobsCount();
  std::vector<const char*> blob_names;
  for (int ib = 0; ib < blobs_count; ib++)
  {
    blob_names.emplace_back(ds_loader->GetBlobName(ib));
  }
  TestExample<float> datum(std::move(blob_names));

  const int c_samples_count = 1000;
  for (int i = 0; i < c_samples_count; i++)
  {
    ds_loader->GetExample(&datum);
  }
}
