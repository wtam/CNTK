#include "background_workers_pool.hpp"
#include "prefetcher.hpp"

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

// Memory equivalent of the disk chunk (chunk loaded to memory). Needs to be reference counted since ExampleEx directly
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

ExampleEx::ExampleEx()
{
  parent_memory_ = nullptr;
  header_ex_ = nullptr;
  channelset_instances_ = nullptr;
}

ExampleEx::ExampleEx(std::shared_ptr<MemoryChunk> parent_mem, DsHeaderEx* h, const char* start_address)
{
  // Let parent memory know we are referencing it now.
  parent_memory_ = parent_mem;
  // Store header and channelset instances.
  header_ex_ = h;
  channelset_instances_ = reinterpret_cast<const ChannelSetInstance*>(start_address);
}

ExampleEx::~ExampleEx()
{
  // If we hold reference to parent memory make sure it is released.
  if (parent_memory_ != nullptr)
  {
    parent_memory_ = nullptr;
  }
}

ExampleEx::ExampleEx(ExampleEx&& other)
{
  // Move all members making sure other object does not reference memory anymore.
  parent_memory_ = other.parent_memory_;
  header_ex_ = other.header_ex_;
  channelset_instances_ = other.channelset_instances_;
  other.parent_memory_ = nullptr;
}

ExampleEx& ExampleEx::operator=(ExampleEx&& other)
{
  // Move all members making sure other object does not reference memory anymore.
  parent_memory_ = other.parent_memory_;
  header_ex_ = other.header_ex_;
  channelset_instances_ = other.channelset_instances_;
  other.parent_memory_ = nullptr;
  return *this;
}

int ExampleEx::GetSizeOnDisk() const
{
  // Size on the disk is equal to header + channelset sizes.
  int size = sizeof(ChannelSetInstance) * header_ex_->GetChannelsetsCount();
  for (int i = 0; i < header_ex_->GetChannelsetsCount(); i++)
  {
    size += channelset_instances_[i].size;
  }
  return size;
}

int ExampleEx::GetBlobsCount() const
{
  return header_ex_->GetBlobsCount();
}

const char* ExampleEx::GetBlobName(int index) const
{
  return header_ex_->GetBlobName(index);
}

int ExampleEx::GetChannelsCount() const
{
  return header_ex_->GetTotalChannelsCount();
}

const vector<int>* ExampleEx::GetChannelsetsForBlob(int ib) const
{
  return header_ex_->GetChannelsetsForBlob(ib);
}

int ExampleEx::GetChannelsetsCount() const
{
  return header_ex_->GetChannelsetsCount();
}

const ChannelSet* ExampleEx::GetChannelset(int i) const
{
  return header_ex_->GetChannelset(i);
}

const char* ExampleEx::GetChannelsetMemory(int i) const
{
  const char* start_mem = reinterpret_cast<const char*>(channelset_instances_ + header_ex_->GetChannelsetsCount());
  for (int ic = 0; ic < i; ic++)
  {
    start_mem += channelset_instances_[ic].size;
  }
  return start_mem;
}

const ChannelSetInstance* ExampleEx::GetChannelsetInstance(int i) const
{
  return &channelset_instances_[i];
}

// Background dataset prefetching object.
class DsPrefetcher : public BackgroundWorkersPool<Chunk, pair<Chunk, MemoryChunk*>>, public IPrefetcher
{
public:
  DsPrefetcher(const PrefetcherParameters& parameters) :
    BackgroundWorkersPool<Chunk, pair<Chunk, MemoryChunk*>>(1), events_sink_(parameters.events_sink),
    shuffle_chunks_(parameters.shuffle_chunks)
  {
    // Open ids file.
    CHECK(Platform::fopen_s(&ds_file_, parameters.ds_file_path.c_str(), "rb") == 0);
    CHECK(ds_file_ != nullptr);

    // Read header.
    DsHeader header;
    std::vector<BlobEx> blobs_ex;
    std::vector<ChannelSet> channelsets;
    fread(&header, sizeof(DsHeader), 1, ds_file_);
    CHECK(header.ids_file_version_ == c_ids_file_version);

    // Now read blobs one by one and blob channelsets info.
    blobs_ex.resize(header.blobs_count_);
    for (size_t ib = 0; ib < blobs_ex.size(); ib++) {
      fread(&blobs_ex[ib].blob, sizeof(Blob), 1, ds_file_);
      blobs_ex[ib].channelsets_ind.resize(blobs_ex[ib].blob.channelsets_count);
      size_t curr_channelsets = channelsets.size();
      channelsets.resize(curr_channelsets + blobs_ex[ib].blob.channelsets_count);
      fread(channelsets.data() + curr_channelsets, sizeof(ChannelSet), blobs_ex[ib].channelsets_ind.size(), ds_file_);
      for (int ic = 0; ic < blobs_ex[ib].channelsets_ind.size(); ic++)
      {
        blobs_ex[ib].channelsets_ind[ic] = static_cast<int>(curr_channelsets);
        curr_channelsets++;
      }
    }

    // Examples start after header (at current position) and end before cached channelset instances.
    int64_t examples_start = Platform::ftell64(ds_file_);
    int64_t examples_end = header.cached_channelset_instances_start_;

    // Read cached channelset instances.
    Platform::fseek64(ds_file_, 0, SEEK_END);
    int64_t file_size = Platform::ftell64(ds_file_);
    int64_t cached_channelset_instances_size = file_size - header.cached_channelset_instances_start_;
    vector<ChannelSetInstance> cached_channelset_instances;
    cached_channelset_instances.resize(cached_channelset_instances_size / sizeof(ChannelSetInstance));
    CHECK((cached_channelset_instances_size % sizeof(ChannelSetInstance)) == 0);
    CHECK(cached_channelset_instances.size() > 0);
    Platform::fseek64(ds_file_, header.cached_channelset_instances_start_, SEEK_SET);
    fread(cached_channelset_instances.data(), sizeof(ChannelSetInstance), cached_channelset_instances.size(), ds_file_);

    // Move all read objects to extended header.
    header_ex_.reset(new DsHeaderEx(header, move(blobs_ex), move(channelsets), move(cached_channelset_instances)));

    // Calculate average prefetch size based on desired prefetch size.
    int64_t examples_total_size = examples_end - examples_start;
    CHECK(examples_total_size > 0);
    const int64_t desired_prefetch_size = parameters.prefetch_size;
    int64_t chunks_count = (examples_total_size + desired_prefetch_size - 1) / desired_prefetch_size;
    int64_t average_prefetch_size = examples_total_size / chunks_count;

    // Partition file into chunks with the size roughly equal to average prefetch size. We will use these chunks to
    // load portions of the file into the memory.
    int64_t offset = examples_start;
    max_chunk_size_ = 0;
    int64_t curr_chunk_size = 0;
    int64_t curr_chunk_start = offset;
    int64_t example_header_size = header_ex_->GetChannelsetsCount() * sizeof(ChannelSetInstance);
    int curr_cached_channelset_instance = 0;
    while (offset < examples_end)
    {
      // Calculate the size of the example.
      int example_data_size = 0;
      for (size_t ic = 0; ic < header_ex_->GetChannelsetsCount(); ic++) {
        example_data_size += (*header_ex_->GetChannelsetInstances())[curr_cached_channelset_instance + ic].size;
      }

      int64_t example_size = example_header_size + example_data_size;

      if (curr_chunk_size + example_size > average_prefetch_size)
      {
        // Done with this chunk, save it.
        max_chunk_size_ = max(max_chunk_size_, curr_chunk_size + example_size);
        Chunk new_chunk;
        new_chunk.size = curr_chunk_size + example_size;
        new_chunk.start_offset = curr_chunk_start;
        chunks_.push_back(new_chunk);

        // Reset current chunk.
        curr_chunk_size = 0;
        curr_chunk_start = new_chunk.start_offset + new_chunk.size;
      }
      else {
        // Keep adding to current chunk.
        curr_chunk_size += example_size;
      }

      offset += example_size;
      curr_cached_channelset_instance += header_ex_->GetChannelsetsCount();
    }
    CHECK(offset == examples_end);
    CHECK(curr_cached_channelset_instance == header_ex_->GetChannelsetInstances()->size());
    // Save the last chunk if we have file portion left at the end.
    if (curr_chunk_size != 0)
    {
      max_chunk_size_ = max(max_chunk_size_, curr_chunk_size);
      Chunk new_chunk;
      new_chunk.size = curr_chunk_size;
      new_chunk.start_offset = curr_chunk_start;
      chunks_.push_back(new_chunk);
    }
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

  ~DsPrefetcher()
  {
    fclose(ds_file_);
  }

  virtual void AbortPrefeching() override
  {
    // Stop all background threads.
    AbortAll();
    // Release memory by deleting memory manager.
    mem_mgr_.reset(nullptr);
  }

  vector<ExampleEx> GetExamples()
  {
    vector<ExampleEx> examples;

    // Take one result.
    pair<Chunk, MemoryChunk*> result = PopResult();

    Chunk chunk = result.first;
    shared_ptr<MemoryChunk> memory_chunk(result.second, MemoryChunk::Deleter());

    // Initialize examples.
    examples.clear();
    int64_t offset = 0;
    while (offset < chunk.size)
    {
      examples.emplace_back(memory_chunk, header_ex_.get(), &memory_chunk->memory_[offset]);

      offset += examples.back().GetSizeOnDisk();
    }

    curr_chunk_++;
    if (curr_chunk_ == chunks_.size())
    {
      ShuffleChunks();
      curr_chunk_ = 0;
    }

    // Initiate new chunk loading.
    PushJobWithCopy(chunks_[curr_chunk_]);

    return examples;
  }

  DsHeaderEx* GetHeaderEx()
  {
    return header_ex_.get();
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

    CHECK(chunk.size <= buffer->memory_size_);
    // Load from disk.
    Platform::fseek64(ds_file_, chunk.start_offset, SEEK_SET);
    fread(buffer->memory_, sizeof(char), chunk.size, ds_file_);

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

  unique_ptr<DsHeaderEx> header_ex_;

  FILE* ds_file_;

  vector<Chunk> chunks_;
  size_t curr_chunk_;
  int64_t max_chunk_size_;

  bool shuffle_chunks_;

  DatasetEventsSink* events_sink_;
};

unique_ptr<IPrefetcher> CreatePrefetcher(const PrefetcherParameters& parameters)
{
  return unique_ptr<IPrefetcher>(new DsPrefetcher(parameters));
}