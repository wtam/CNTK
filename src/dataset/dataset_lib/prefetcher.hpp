// Declares interface for data set prefetching.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "dataset.hpp"

struct DsHeaderEx;
struct MemoryChunk;
class DatasetEventsSink;

// Describes deserialized blob info (shared across all instances of this blob in all examples).
struct BlobEx
{
  Blob blob;
  std::vector<int> channelsets_ind;
};

// Describes deserialized header.
struct DsHeaderEx
{
public:
  DsHeaderEx(const DsHeader& header, std::vector<BlobEx>&& blobs_ex, std::vector<ChannelSet>&& channelsets,
    std::vector<ChannelSetInstance>&& cached_channelset_instances)
  {
    header_ = header;
    blobs_ex_ = move(blobs_ex);
    channelsets_ = move(channelsets);
    cached_channelset_instances_ = move(cached_channelset_instances);
  }

  int GetBlobsCount() const
  {
    return static_cast<int>(blobs_ex_.size());
  }

  int GetBlobChannelsCount(int ib) const
  {
    int res = 0;
    for (size_t ic = 0; ic < blobs_ex_[ib].channelsets_ind.size(); ic++)
    {
      res += channelsets_[blobs_ex_[ib].channelsets_ind[ic]].channels;
    }
    return res;
  }

  const std::vector<int>* GetChannelsetsForBlob(int ib) const
  {
    return &blobs_ex_[ib].channelsets_ind;
  }

  const ChannelSet* GetChannelset(int i) const
  {
    return &channelsets_[i];
  }

  int GetChannelsetsCount() const
  {
    return static_cast<int>(channelsets_.size());
  }

  int GetTotalChannelsCount() const
  {
    int res = 0;
    for (size_t ic = 0; ic < channelsets_.size(); ic++)
    {
      res += channelsets_[ic].channels;
    }
    return res;
  }

  const char* GetBlobName(int index) const
  {
    return blobs_ex_[index].blob.name;
  }

  const std::vector<ChannelSetInstance>* GetChannelsetInstances() const
  {
    return &cached_channelset_instances_;
  }

private:
  DsHeader header_;
  std::vector<BlobEx> blobs_ex_;
  std::vector<ChannelSet> channelsets_;
  std::vector<ChannelSetInstance> cached_channelset_instances_;
};

// Describes deserialized example.
struct ExampleEx
{
public:
  ExampleEx();

  ExampleEx(std::shared_ptr<MemoryChunk> parent_mem, DsHeaderEx* h, const char* start_address);

  ~ExampleEx();

  ExampleEx(ExampleEx&& other);

  ExampleEx& operator=(ExampleEx&& other);

  int GetSizeOnDisk() const;

  int GetBlobsCount() const;

  const char* GetBlobName(int index) const;

  const std::vector<int>* GetChannelsetsForBlob(int ib) const;

  int GetChannelsCount() const;

  int GetChannelsetsCount() const;

  const ChannelSet* GetChannelset(int i) const;

  const char* GetChannelsetMemory(int i) const;

  const ChannelSetInstance* GetChannelsetInstance(int i) const;

private:
  ExampleEx(const ExampleEx& other) = delete;
  ExampleEx& operator=(const ExampleEx& other) = delete;

private:
  std::shared_ptr<MemoryChunk> parent_memory_;
  DsHeaderEx* header_ex_;
  const ChannelSetInstance* channelset_instances_;
};

// Interface for prefetching object.
class IPrefetcher
{
public:
  virtual ~IPrefetcher() {}
  virtual const DsHeaderEx* GetHeaderEx() = 0;
  virtual std::vector<ExampleEx> GetExamples() = 0;
  // All examples returned by GetExamples must be released before abort.
  virtual void AbortPrefeching() = 0;
};

// Prefetcher parameters.
struct PrefetcherParameters
{
  std::string ds_file_path;
  int64_t prefetch_size;
  DatasetEventsSink* events_sink;
  bool shuffle_chunks;
};

std::unique_ptr<IPrefetcher> CreatePrefetcher(const PrefetcherParameters& parameters);