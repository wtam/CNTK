#include "background_workers_pool.hpp"
#include "deserializer.hpp"

#include "fixed_memory_manager.hpp"
#include "dataset.hpp"
#include "check.hpp"
#include "platform.hpp"
#include "dataset_events_sink.hpp"

#include <algorithm>
#include <random>

using namespace std;

// Defines piece of dataset file to be loaded.
struct Chunk
{
  int64_t start_offset;
  int64_t size;
};

// Memory equivalent of the disk chunk (chunk loaded to memory). Needs to be reference counted since DeserializedChannelsets directly
// reference this memory. Once all examples release reference memory is returned to memory pool.
struct MemoryChunk
{
  MemoryChunk(char* mem, size_t memory_size, void* /*context*/)
  {
    memory_ = mem;
    memory_size_ = memory_size;
  }

  struct Deleter
  {
  public:
    void operator()(MemoryChunk* ptr)
    {
      ptr->mem_mgr_->Dealloc(ptr);
    }
  };

  FixedMemoryManager<MemoryChunk, char, void*>* mem_mgr_;
  char* memory_;
  int64_t memory_size_;
};

// Channelset identifier. Used to abstract away channelset ordering which results in less general implementation
// and is error prone.
struct ChannelsetID
{
  size_t index;
};

// Describes deserialized header.
struct DeserializedHeader
{
public:
  DeserializedHeader(const DsHeader& header, std::vector<ChannelSet>&& channelsets,
    std::vector<ChannelSetInstance>&& cached_channelset_instances)
  {
    header_ = header;
    channelsets_ = move(channelsets);
    cached_channelset_instances_ = move(cached_channelset_instances);
    CHECK(cached_channelset_instances_.size() % channelsets_.size() == 0, "Invalid number of channelset instances.");

    // Fill in channelset ids.
    channelsets_ids_.resize(channelsets_.size());
    for (size_t i = 0; i < channelsets_ids_.size(); i++)
    {
      channelsets_ids_[i].index = i;
    }
  }

  const ChannelSet* GetChannelset(const ChannelsetID* id) const
  {
    return &channelsets_[id->index];
  }

  int GetChannelsetsCount() const
  {
    return static_cast<int>(channelsets_.size());
  }

  const std::vector<ChannelSetInstance>* GetChannelsetInstances() const
  {
    return &cached_channelset_instances_;
  }

  const int GetExamplesCount()
  {
    return static_cast<int>(cached_channelset_instances_.size() / channelsets_.size());
  }

  const ChannelSetInstance* GetExampleChannelsetInstance(int example_index, const ChannelsetID* id)
  {
    return &cached_channelset_instances_[example_index * channelsets_.size() + id->index];
  }

  const ChannelsetID* GetChannelsetID(const std::string& channelset_name)
  {
    ChannelsetID* id = nullptr;

    for (size_t ic = 0; ic < channelsets_.size(); ic++)
    {
      if (channelset_name == channelsets_[ic].name)
      {
        id = &channelsets_ids_[ic];
        break;
      }
    }

    return id;
  }

private:
  DsHeader header_;
  std::vector<ChannelSet> channelsets_;
  std::vector<ChannelsetID> channelsets_ids_;
  std::vector<ChannelSetInstance> cached_channelset_instances_;
};

DeserializedChannelsets::DeserializedChannelsets()
{
  parent_memory_ = nullptr;
  deserialized_header_ = nullptr;
  channelset_instances_ = nullptr;
}

DeserializedChannelsets::DeserializedChannelsets(std::shared_ptr<MemoryChunk> parent_mem, DeserializedHeader* h, const char* start_address)
{
  // Let parent memory know we are referencing it now.
  parent_memory_ = parent_mem;
  // Store header and channelset instances.
  deserialized_header_ = h;
  channelset_instances_ = reinterpret_cast<const ChannelSetInstance*>(start_address);
}

DeserializedChannelsets::~DeserializedChannelsets()
{
  // If we hold reference to parent memory make sure it is released.
  if (parent_memory_ != nullptr)
  {
    parent_memory_ = nullptr;
  }
}

DeserializedChannelsets::DeserializedChannelsets(DeserializedChannelsets&& other)
{
  // Move all members making sure other object does not reference memory anymore.
  parent_memory_ = other.parent_memory_;
  deserialized_header_ = other.deserialized_header_;
  channelset_instances_ = other.channelset_instances_;
  other.parent_memory_ = nullptr;
}

DeserializedChannelsets& DeserializedChannelsets::operator=(DeserializedChannelsets&& other)
{
  // Move all members making sure other object does not reference memory anymore.
  parent_memory_ = other.parent_memory_;
  deserialized_header_ = other.deserialized_header_;
  channelset_instances_ = other.channelset_instances_;
  other.parent_memory_ = nullptr;
  return *this;
}

const ChannelSet* DeserializedChannelsets::GetChannelset(const ChannelsetID* id) const
{
  return deserialized_header_->GetChannelset(id);
}

const char* DeserializedChannelsets::GetChannelsetMemory(const ChannelsetID* id) const
{
  const char* start_mem = reinterpret_cast<const char*>(channelset_instances_ + deserialized_header_->GetChannelsetsCount());
  for (int ic = 0; ic < id->index; ic++)
  {
    start_mem += channelset_instances_[ic].size;
  }
  return start_mem;
}

const ChannelSetInstance* DeserializedChannelsets::GetChannelsetInstance(const ChannelsetID* id) const
{
  return &channelset_instances_[id->index];
}

// Background dataset prefetching object.
class DsDeserializerWithPrefetching : public BackgroundWorkersPool<Chunk, pair<Chunk, MemoryChunk*>>, public IIDSDeserializer
{
public:
  DsDeserializerWithPrefetching(const DeserializeParameters& parameters) :
    BackgroundWorkersPool<Chunk, pair<Chunk, MemoryChunk*>>(1), events_sink_(parameters.events_sink),
    shuffle_chunks_(parameters.shuffle_chunks)
  {
    // Open ids file.
    CHECK(Platform::fopen_s(&ds_file_, parameters.ds_file_path.c_str(), "rb") == 0,
      "Failed to open ds file %s", parameters.ds_file_path.c_str());
    CHECK(ds_file_ != nullptr, "Failed to open ds file %s", parameters.ds_file_path.c_str());

    // Read header.
    DsHeader header;
    CHECK(fread(&header, sizeof(DsHeader), 1, ds_file_) == 1,
      "Reading dataset header for file %s failed", parameters.ds_file_path.c_str());
    CHECK(header.ids_file_version_ == c_ids_file_version, "Version mismatch in IDS loading, %d given %d expected.",
      header.ids_file_version_, c_ids_file_version);

    // Now read channelsets info.
    std::vector<ChannelSet> channelsets;
    channelsets.resize(header.channelsets_count);
    CHECK(fread(channelsets.data(), sizeof(ChannelSet), channelsets.size(), ds_file_) == channelsets.size(),
      "Reading channelset descriptors for file %s failed", parameters.ds_file_path.c_str());

    // Examples start after header (at current position) and end before cached channelset instances.
    int64_t examples_start = Platform::ftell64(ds_file_);
    int64_t examples_end = header.cached_channelset_instances_start_;

    // Read cached channelset instances.
    Platform::fseek64(ds_file_, 0, SEEK_END);
    int64_t file_size = Platform::ftell64(ds_file_);
    int64_t cached_channelset_instances_size = file_size - header.cached_channelset_instances_start_;
    vector<ChannelSetInstance> cached_channelset_instances;
    cached_channelset_instances.resize(cached_channelset_instances_size / sizeof(ChannelSetInstance));
    CHECK((cached_channelset_instances_size % sizeof(ChannelSetInstance)) == 0,
      "Invalid cached_channelset_instances_size %d (not multiple of sizeof(ChannelSetInstance) = %u).",
      cached_channelset_instances_size, sizeof(ChannelSetInstance));
    CHECK(cached_channelset_instances.size() > 0, "Cached channelset instances size cannot be zero.");
    Platform::fseek64(ds_file_, header.cached_channelset_instances_start_, SEEK_SET);
    CHECK(fread(cached_channelset_instances.data(), sizeof(ChannelSetInstance), cached_channelset_instances.size(), ds_file_) ==
      cached_channelset_instances.size(),
      "Reading cached channelset instanced for file %s failed.", parameters.ds_file_path.c_str());

    // Move all read objects to extended header.
    deserialized_header_.reset(new DeserializedHeader(header, move(channelsets), move(cached_channelset_instances)));

    // Calculate average prefetch size based on desired prefetch size.
    int64_t examples_total_size = examples_end - examples_start;
    CHECK(examples_total_size > 0, "examples_total_size cannot be 0.");
    const int64_t desired_prefetch_size = parameters.prefetch_size;
    int64_t chunks_count = (examples_total_size + desired_prefetch_size - 1) / desired_prefetch_size;
    int64_t average_prefetch_size = examples_total_size / chunks_count;

    // Partition file into chunks with the size roughly equal to average prefetch size. We will use these chunks to
    // load portions of the file into the memory.
    vector<Chunk> all_chunks;
    int64_t offset = examples_start;
    max_chunk_size_ = 0;
    int64_t curr_chunk_size = 0;
    int64_t curr_chunk_start = offset;
    int64_t example_header_size = deserialized_header_->GetChannelsetsCount() * sizeof(ChannelSetInstance);
    int curr_cached_channelset_instance = 0;
    while (offset < examples_end)
    {
      // Calculate the size of the example.
      int example_data_size = 0;
      for (size_t ic = 0; ic < deserialized_header_->GetChannelsetsCount(); ic++) {
        example_data_size += (*deserialized_header_->GetChannelsetInstances())[curr_cached_channelset_instance + ic].size;
      }

      int64_t example_size = example_header_size + example_data_size;

      if (curr_chunk_size + example_size > average_prefetch_size)
      {
        // Done with this chunk, save it.
        max_chunk_size_ = max(max_chunk_size_, curr_chunk_size + example_size);
        Chunk new_chunk;
        new_chunk.size = curr_chunk_size + example_size;
        new_chunk.start_offset = curr_chunk_start;
        all_chunks.push_back(new_chunk);

        // Reset current chunk.
        curr_chunk_size = 0;
        curr_chunk_start = new_chunk.start_offset + new_chunk.size;
      }
      else {
        // Keep adding to current chunk.
        curr_chunk_size += example_size;
      }

      offset += example_size;
      curr_cached_channelset_instance += deserialized_header_->GetChannelsetsCount();
    }
    CHECK(offset == examples_end, "offset %d must be equal to examples_end %d.", offset, examples_end);
    CHECK(curr_cached_channelset_instance == static_cast<int>(deserialized_header_->GetChannelsetInstances()->size()),
      "curr_cached_channelset_instance %d is not equal to cached channelset instances count %u.",
      curr_cached_channelset_instance, deserialized_header_->GetChannelsetInstances()->size());
    // Save the last chunk if we have file portion left at the end.
    if (curr_chunk_size != 0)
    {
      max_chunk_size_ = max(max_chunk_size_, curr_chunk_size);
      Chunk new_chunk;
      new_chunk.size = curr_chunk_size;
      new_chunk.start_offset = curr_chunk_start;
      all_chunks.push_back(new_chunk);
    }
    // Here we have all chunks calculated. In case of distributed loading we want each loader/deserializer to deal with
    // non-overlapping part of dataset file. Now we will calculate our portion of chunks to follow this principle.
    CHECK(parameters.derializer_index_ < parameters.derializers_count_,
      "Invalid deserializer index %u (deserializers count is %u)", parameters.derializer_index_, parameters.derializers_count_);
    size_t deserializer_chunk_count = all_chunks.size() / parameters.derializers_count_;
    CHECK(deserializer_chunk_count > 0, "More readers than chunks in dataset file. Decrease the chunks size or readers count.");
    size_t start_chunk = parameters.derializer_index_ * deserializer_chunk_count;
    size_t end_chunk = (parameters.derializer_index_ + 1) * deserializer_chunk_count;
    // Ensure last reader reads till end.
    if (parameters.derializer_index_ + 1 == parameters.derializers_count_)
    {
      end_chunk = all_chunks.size();
    }
    // Copy our portion of chunks to internal vector.
    chunks_.insert(chunks_.begin(), all_chunks.begin() + start_chunk, all_chunks.begin() + end_chunk);

    ShuffleChunks();

    // We need two buffers + one in case we finished second one while first one is still in processing. We do not expect
    // more allocations if we use 3.
    const int number_of_elements = 3;
    mem_mgr_.reset(new FixedMemoryManager<MemoryChunk, char, void*>(max_chunk_size_, number_of_elements, nullptr, false));

    if (events_sink_ != nullptr)
    {
      events_sink_->DataReadThreadsCount(1);
    }

    // Kick off background loading by pushing one chunk.
    curr_chunk_ = 0;
    PushJobWithCopy(chunks_[curr_chunk_]);
  }

  ~DsDeserializerWithPrefetching()
  {
    fclose(ds_file_);
  }

  virtual void AbortDeserializing() override
  {
    // Stop all background threads.
    AbortAll();
    // Release memory by deleting memory manager.
    mem_mgr_.reset(nullptr);
  }

  virtual vector<DeserializedChannelsets> GetExamples() override
  {
    vector<DeserializedChannelsets> deserialized_channelsets;

    // Take one result.
    pair<Chunk, MemoryChunk*> result = PopResult();

    Chunk chunk = result.first;
    shared_ptr<MemoryChunk> memory_chunk(result.second, MemoryChunk::Deleter());

    int64_t offset = 0;
    while (offset < chunk.size)
    {
      deserialized_channelsets.emplace_back(memory_chunk, deserialized_header_.get(), &memory_chunk->memory_[offset]);

      // Size on the disk is equal to header + channelset sizes.
      int size_on_disk = sizeof(ChannelSetInstance) * deserialized_header_->GetChannelsetsCount();
      const ChannelSetInstance* channelset_instances = reinterpret_cast<const ChannelSetInstance*>(&memory_chunk->memory_[offset]);
      for (int i = 0; i < deserialized_header_->GetChannelsetsCount(); i++)
      {
        size_on_disk += channelset_instances[i].size;
      }

      offset += size_on_disk;
    }

    curr_chunk_++;
    if (curr_chunk_ == chunks_.size())
    {
      ShuffleChunks();
      curr_chunk_ = 0;
    }

    // Initiate new chunk loading.
    PushJobWithCopy(chunks_[curr_chunk_]);

    return deserialized_channelsets;
  }

  virtual int GetExamplesCount() override
  {
    return deserialized_header_->GetExamplesCount();
  }

  virtual const ChannelSetInstance* GetExampleChannelsetInstance(int example_index, const ChannelsetID* id) override
  {
    return deserialized_header_->GetExampleChannelsetInstance(example_index, id);
  }

  virtual const ChannelsetID* GetChannelsetID(const std::string& channelset_name) override
  {
    return deserialized_header_->GetChannelsetID(channelset_name);
  }

  virtual const ChannelSet* GetChannelset(const ChannelsetID* id) override
  {
    return deserialized_header_->GetChannelset(id);
  }

private:
  virtual pair<Chunk, MemoryChunk*> BackgroundProcesingMethod(Chunk& chunk, int thread_id)
  {
    if (events_sink_ != nullptr)
    {
      events_sink_->DataReadStart(thread_id);
    }

    // Take new memory buffer.
    MemoryChunk* buffer = mem_mgr_->Alloc();
    buffer->mem_mgr_ = mem_mgr_.get();

    CHECK(chunk.size <= buffer->memory_size_, "Chunk size %d greater than buffer size %d.", chunk.size, buffer->memory_size_);
    // Load from disk.
    Platform::fseek64(ds_file_, chunk.start_offset, SEEK_SET);
    CHECK(fread(buffer->memory_, sizeof(char), chunk.size, ds_file_) == static_cast<size_t>(chunk.size),
      "Reading chunk start_offset=%d, size=%d failed", chunk.start_offset, chunk.size);

    if (events_sink_ != nullptr)
    {
      events_sink_->DataReadEnd(thread_id, chunk.size);
    }

    return make_pair(chunk, buffer);
  }

  void ShuffleChunks()
  {
    if (shuffle_chunks_)
    {
      shuffle(chunks_.begin(), chunks_.end(), default_random_engine(0));
    }
  }
  std::unique_ptr<FixedMemoryManager<MemoryChunk, char, void*>> mem_mgr_;

  unique_ptr<DeserializedHeader> deserialized_header_;

  FILE* ds_file_;

  vector<Chunk> chunks_;
  size_t curr_chunk_;
  int64_t max_chunk_size_;

  bool shuffle_chunks_;

  DatasetEventsSink* events_sink_;
};

unique_ptr<IIDSDeserializer> CreateIdsDeserializer(const DeserializeParameters& parameters)
{
  return unique_ptr<IIDSDeserializer>(new DsDeserializerWithPrefetching(parameters));
}