// Declares interface for data set prefetching.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "dataset.hpp"

struct DeserializedHeader;
struct MemoryChunk;
struct ChannelsetID;
class DatasetEventsSink;

// Class that wraps deserialized channelsets. It provided access to channelset descriptors and instances.
// Deserialized channelsets contain all channelsets that correspond to one example (although real example may
// contain less channelsets, subset, based on runtime load configuration).
struct DeserializedChannelsets
{
public:
  DeserializedChannelsets();

  DeserializedChannelsets(std::shared_ptr<MemoryChunk> parent_mem, DeserializedHeader* h, const char* start_address);

  ~DeserializedChannelsets();

  DeserializedChannelsets(DeserializedChannelsets&& other);

  DeserializedChannelsets& operator=(DeserializedChannelsets&& other);

  DeserializedChannelsets(const DeserializedChannelsets& other) = delete;

  DeserializedChannelsets& operator=(const DeserializedChannelsets& other) = delete;

  const ChannelSet* GetChannelset(const ChannelsetID* id) const;

  const char* GetChannelsetMemory(const ChannelsetID* id) const;

  const ChannelSetInstance* GetChannelsetInstance(const ChannelsetID* id) const;

private:
  std::shared_ptr<MemoryChunk> parent_memory_;
  DeserializedHeader* deserialized_header_;
  const ChannelSetInstance* channelset_instances_;
};

// Interface for accessing deserialized description of image dataset.
class IIDSDescriptor
{
public:
  virtual ~IIDSDescriptor() {}
  // Returns number of examples.
  virtual int GetExamplesCount() = 0;
  // Returns channelset instance for channelset with given id for the example with given index.
  virtual const ChannelSetInstance* GetExampleChannelsetInstance(int example_index, const ChannelsetID* id) = 0;
  // Given the name returns id of the channelset.
  virtual const ChannelsetID* GetChannelsetID(const std::string& channelset_name) = 0;
  // Returns description of the channelset with given id.
  virtual const ChannelSet* GetChannelset(const ChannelsetID* id) = 0;
};

// Interface for accessing deserialized examples.
class IExamplesDeserializer
{
public:
  virtual ~IExamplesDeserializer() {}
  // Returns next batch of loaded examples.
  virtual std::vector<DeserializedChannelsets> GetExamples() = 0;
  // All examples returned by GetExamples must be released before abort.
  virtual void AbortDeserializing() = 0;
};

// Interface for complete image detaset deserializer. It is derived from previous two interfaces to help in separating
// client code logical groups. Namely, transformable objects need IIDSDescriptor to be created but once created no one
// should have access to IIDSDescriptor (some info may be transformed like dimensions, so it should be accessible just
// through transformable objects) just IExamplesDeserializer is needed.
class IIDSDeserializer : public IExamplesDeserializer, public IIDSDescriptor
{
public:
  virtual ~IIDSDeserializer() {}
};

// Deserialization parameters.
struct DeserializeParameters
{
  std::string ds_file_path;
  int64_t prefetch_size;
  DatasetEventsSink* events_sink;
  bool shuffle_chunks;
  size_t derializer_index_;
  size_t derializers_count_;
};

std::unique_ptr<IIDSDeserializer> CreateIdsDeserializer(const DeserializeParameters& parameters);