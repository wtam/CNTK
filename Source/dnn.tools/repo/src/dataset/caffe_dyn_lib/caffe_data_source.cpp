#include "external_lib_data_source.hpp"
#include "dataset_events_sink.hpp"
#include "dataset_io.hpp"
#include "check.hpp"

#include <array>
#include <string>
#include <memory>
#include <vector>
#include <iostream>

#include <cstring>

using namespace std;

#ifdef _MSC_VER
#define CAFFE_DLL_EXPORT __declspec(dllexport)
#else
#define CAFFE_DLL_EXPORT
#endif

template <typename T>
struct TypeMap
{
  static BlobType GetType()
  {
    static_assert(sizeof(T) != 0, "Type not implemented.");
  }
};

template <>
struct TypeMap<float>
{
  static BlobType GetType()
  {
    return BlobType::BlobTypeFLOAT;
  }
};

template <>
struct TypeMap<double>
{
  static BlobType GetType()
  {
    return BlobType::BlobTypeDOUBLE;
  }
};

// Implementation of IDatum interface required by Caffe and IExample interface required by
// dataset loader.
template <class Dtype>
class CaffeDatum : public IDatum, public IExample<Dtype> {
public:
  CaffeDatum(vector<const char*>&& blob_names) :
    blob_names_(move(blob_names)), blobs_(blob_names_.size()), blob_shapes_(blob_names_.size())
  {
  }

  // IExample override.
  virtual void ReshapeBlob(int index, int channels, int height, int width) override
  {
    static_assert(c_blob_dims_ == 3, "Invalid blob dims.");
    blob_shapes_[index][0] = channels;
    blob_shapes_[index][1] = height;
    blob_shapes_[index][2] = width;
    blobs_[index].resize(channels * height* width);
  }

  // IExample override.
  virtual Dtype* GetBlobMemory(int index) override
  {
    return blobs_[index].data();
  }

  // IDatum override.
  virtual void GetBlobShape(const char* blob_name, const int** shape,
    int* shape_count) override
  {
    int ib = BlobIndexFromName(blob_name);
    *shape_count = c_blob_dims_;
    *shape = blob_shapes_[ib].data();
  }

  // IDatum override.
  virtual void GetBlobData(const char* blob_name, void* blob_data,
    BlobType type) override
  {
    // We expect to be asked for the same type we are instantiated with.
    CHECK(type == TypeMap<Dtype>::GetType(), "Invalid type of the buffer provided.");
    int ib = BlobIndexFromName(blob_name);
    memcpy(blob_data, blobs_[ib].data(), blobs_[ib].size() * sizeof(Dtype));
  }

  // Given the blb name returns its index in the vector of blobs.
  int BlobIndexFromName(const char* blob_name)
  {
    int index = -1;
    for (size_t ib = 0; ib < blob_names_.size(); ib++)
    {
      if (strcmp(blob_name, blob_names_[ib]) == 0)
      {
        index = static_cast<int>(ib);
        break;
      }
    }
    CHECK(index != -1, "Blob with name %s required by Caffe not found in dll.", blob_name);
    return index;
  }

private:
  static const int c_blob_dims_ = 3;

  vector<const char*> blob_names_;
  vector<vector<Dtype>> blobs_;
  vector<array<int, c_blob_dims_>> blob_shapes_;
};

// Implementation of data source interface required by Caffe.
template <class Dtype>
class CaffeDataSource : public IExternalLibDataSource
{
public:
  CaffeDataSource(const string external_lib_params) {

    // Create dataset loader.
    ds_loader_ = CreateLoader<Dtype>(external_lib_params, nullptr, nullptr);

    // Take names of the blobs inside dataset.
    int blobs_count = ds_loader_->GetBlobsCount();
    vector<const char*> blob_names;
    for (int ib = 0; ib < blobs_count; ib++)
    {
      blob_names.emplace_back(ds_loader_->GetBlobName(ib));
    }

    // Create datum and populate it from dataset loader.
    datum_ = unique_ptr<CaffeDatum<Dtype>>(new CaffeDatum<Dtype>(move(blob_names)));
    ds_loader_->GetExample(datum_.get());
  }

  // Return configuration string to be logged to Caffe ouput.
  virtual const char* GetConfiguration() override
  {
    return ds_loader_->GetConfiguration();
  }

  virtual void Release() override
  {
    delete this;
  }

  virtual void MoveToNext() override
  {
    // Take new example from dataset loader.
    ds_loader_->GetExample(datum_.get());
  }

  virtual IDatum* GetCurrent() override
  {
    return datum_.get();
  }

private:
  unique_ptr<IDsLoader<Dtype>> ds_loader_;
  unique_ptr<CaffeDatum<Dtype>> datum_;
};

extern "C" CAFFE_DLL_EXPORT IExternalLibDataSource* CreateDataSourceFloat(
  const char* external_lib_params)
{
  return new CaffeDataSource<float>(external_lib_params);
}

extern "C" CAFFE_DLL_EXPORT IExternalLibDataSource* CreateDataSourceDouble(
  const char* external_lib_params)
{
  return new CaffeDataSource<double>(external_lib_params);
}
