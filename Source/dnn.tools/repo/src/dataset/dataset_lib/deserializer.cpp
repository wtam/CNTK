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
  Chunk(size_t file_ind, int64_t start_off, int64_t s) :
    file_index(file_ind), start_offset(start_off), size(s) {}

  size_t file_index;
  int64_t start_offset;
  int64_t size;
};

// Memory equivalent of the disk chunk (chunk loaded to memory). Needs to be reference counted since
// DeserializedChannelsets directly reference this memory. Once all examples release reference memory
// is returned to memory pool.
struct MemoryChunk
{
  MemoryChunk(char* mem, size_t memory_size, void* /*context*/)
  {
    memory_ = mem;
    memory_size_ = memory_size;
  }

  // Deleter to be used with shared pointer. Memory chunk is referenced by DeserializedChannelsets. Once all references
  // are released this object returns memory to memory managed pool.
  struct Deleter
  {
  public:
    void operator()(MemoryChunk* ptr)
    {
      ptr->mem_mgr_->Dealloc(ptr);
    }
  };

  // Parent memory manager object which maintains the pool from which this memory chunk comes from.
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

// Describes deserialized header. This header contains information related to the whole dataset (DsHeader + all info
// related to whole dataset that is not serialized as part of DsHeader due to file format).
struct DeserializedDsHeader
{
public:
  DeserializedDsHeader(const DsHeader& header, std::vector<ChannelSet>&& channelsets,
    std::vector<ChannelSetInstance>&& cached_channelset_instances, vector<int64_t>&& channelset_instances_file_start)
  {
    header_ = header;
    channelsets_ = move(channelsets);
    cached_channelset_instances_ = move(cached_channelset_instances);
    channelset_instances_file_start_ = move(channelset_instances_file_start);
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

  const size_t GetFilesCount()
  {
    return channelset_instances_file_start_.size();
  }

  const std::vector<ChannelSetInstance>* GetChannelsetInstances() const
  {
    return &cached_channelset_instances_;
  }

  const ChannelSetInstance* GetChannelsetInstancesForFile(size_t index) const
  {
    return &cached_channelset_instances_[channelset_instances_file_start_[index]];
  }

  const int GetExamplesCount()
  {
    return static_cast<int>(cached_channelset_instances_.size() / channelsets_.size());
  }

  const size_t GetExamplesCountPerFile(size_t index)
  {
    CHECK(index < channelset_instances_file_start_.size(),
      "File index out of range %lu >= %lu", index, channelset_instances_file_start_.size());
    size_t channelset_instances_per_file = 0;
    if (index + 1 == channelset_instances_file_start_.size())
    {
      channelset_instances_per_file = cached_channelset_instances_.size() - channelset_instances_file_start_[index];
    }
    else
    {
      channelset_instances_per_file =
        channelset_instances_file_start_[index + 1] - channelset_instances_file_start_[index];
    }
    CHECK(channelset_instances_per_file % channelsets_.size() == 0,
      "Number of channelset instances per file %lu is not divisible with channelsets count %lu for file %lu",
      channelset_instances_per_file,
      channelsets_.size(),
      index
      );
    return channelset_instances_per_file / channelsets_.size();
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
  // Original header (as serialized).
  DsHeader header_;
  // Global description of all channelsets.
  std::vector<ChannelSet> channelsets_;
  // Unique identifier per channelset.
  std::vector<ChannelsetID> channelsets_ids_;
  // Collection of all channelset instances (for all files being loaded).
  std::vector<ChannelSetInstance> cached_channelset_instances_;
  // Indicates where channelset instances per one file start in cached_channelset_instances_.
  vector<int64_t> channelset_instances_file_start_;
};

DeserializedChannelsets::DeserializedChannelsets()
{
  parent_memory_ = nullptr;
  deserialized_header_ = nullptr;
  channelset_instances_ = nullptr;
}

DeserializedChannelsets::DeserializedChannelsets(
  std::shared_ptr<MemoryChunk> parent_mem,
  DeserializedDsHeader* h,
  const char* start_address
  )
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
  const char* start_mem =
    reinterpret_cast<const char*>(channelset_instances_ + deserialized_header_->GetChannelsetsCount());
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
class DsDeserializerWithPrefetching :
  public BackgroundWorkersPool<Chunk, pair<Chunk, MemoryChunk*>>, public IIDSDeserializer
{
public:
  // In deserializer constructor we need to do:
  // 1) Load headers and cached channelset instances for all the IDS files.
  // 2) Calculate range of examples that this deserializer needs to work with.
  // 3) Determine how range of examples is distributed accross files (find part of files this deserializer works with).
  // 4) Perform chunking for each part of file this deserializer works with.
  // 5) Kick off first chunk loading.
  DsDeserializerWithPrefetching(const DeserializeParameters& parameters) :
    BackgroundWorkersPool<Chunk, pair<Chunk, MemoryChunk*>>(1), events_sink_(parameters.events_sink),
    shuffle_chunks_(parameters.shuffle_chunks), max_chunk_size_(0)
  {
    // Perform IDS files parsing.
    int64_t examples_start_offset = ParseIdsFiles(parameters);

    // Compute file portions that this deserializer works with.
    vector<IdsFileExamplesPortion> examples_portion_per_file = CalculateFilePortions(parameters);

    // Divide file portions into chunks.
    CalculateChunks(parameters, examples_portion_per_file, examples_start_offset);

    // Randomize chunks if necessary.
    ShuffleChunks();

    // We need two buffers + one in case we finished second one while first one is still in processing.
    // We do not expect more allocations if we use 3.
    const int number_of_elements = 3;
    mem_mgr_.reset(
      new FixedMemoryManager<MemoryChunk, char, void*>(max_chunk_size_, number_of_elements, nullptr, false)
      );

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
    for (size_t iF = 0; iF < ds_files_.size(); iF++)
    {
      fclose(ds_files_[iF]);
    }
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

    const Chunk chunk = result.first;
    shared_ptr<MemoryChunk> memory_chunk(result.second, MemoryChunk::Deleter());

    int64_t offset = 0;
    while (offset < chunk.size)
    {
      deserialized_channelsets.emplace_back(memory_chunk, deserialized_header_.get(), &memory_chunk->memory_[offset]);

      // Size on the disk is equal to header + channelset sizes.
      int size_on_disk = sizeof(ChannelSetInstance) * deserialized_header_->GetChannelsetsCount();
      const ChannelSetInstance* channelset_instances =
        reinterpret_cast<const ChannelSetInstance*>(&memory_chunk->memory_[offset]);
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
  // Helper structure that describes file portion relevant to deserializer.
  struct IdsFileExamplesPortion
  {
    // Index of the file in ds_files_ array.
    size_t ids_file_index;
    // Defines boundaries of interest.
    int64_t ids_file_start_example;
    int64_t ids_file_end_example;
  };

  // Performs parsing of ids files. Parsed results will be stored in deserialized_header_.
  int64_t ParseIdsFiles(const DeserializeParameters& parameters)
  {
    ds_files_.resize(parameters.ds_files_paths.size(), NULL);

    int reference_ids_index = -1;
    unique_ptr<DsHeader> sharedHeader;
    unique_ptr<vector<ChannelSet>> shared_channelsets;
    vector<int64_t> channelset_instances_file_start(parameters.ds_files_paths.size());
    vector<ChannelSetInstance> cached_channelset_instances_all;
    const int64_t c_examples_start_offset_init = -1;
    int64_t examples_start_offset = c_examples_start_offset_init;
    // Go over all provided ids files.
    for (int iF = 0; iF < ds_files_.size(); iF++)
    {
      // Open ids file.
      CHECK_ERRNO(Platform::fopen_s(&ds_files_[iF], parameters.ds_files_paths[iF].c_str(), "rb") == 0,
        "Failed to open ds file %s", parameters.ds_files_paths[iF].c_str());
      CHECK(ds_files_[iF] != nullptr, "Failed to open ds file %s", parameters.ds_files_paths[iF].c_str());

      // Read header.
      DsHeader header;
      CHECK_ERRNO(fread(&header, sizeof(DsHeader), 1, ds_files_[iF]) == 1,
        "Reading dataset header for file %s failed", parameters.ds_files_paths[iF].c_str());
      CHECK(header.ids_file_version_ == c_ids_file_version, "Version mismatch in IDS loading, %d given %d expected.",
        header.ids_file_version_, c_ids_file_version);
      if (sharedHeader == nullptr)
      {
        // Save current header as shared header.
        sharedHeader.reset(new DsHeader(header));
        reference_ids_index = iF;
      }
      else
      {
        // Check that this header is compatible with shared one.
        CHECK(sharedHeader->CheckCompatible(header),
          "Incompatible ids files %s and %s",
          parameters.ds_files_paths[reference_ids_index].c_str(),
          parameters.ds_files_paths[iF].c_str());
      }

      // Now read channelsets info.
      std::vector<ChannelSet> channelsets;
      channelsets.resize(header.channelsets_count);
      CHECK_ERRNO(
        fread(channelsets.data(), sizeof(ChannelSet), channelsets.size(), ds_files_[iF]) == channelsets.size(),
        "Reading channelset descriptors for file %s failed", parameters.ds_files_paths[iF].c_str()
        );
      if (shared_channelsets == nullptr)
      {
        // Save current channelset instances as shared ones.
        shared_channelsets.reset(new std::vector<ChannelSet>(channelsets));
        CHECK(reference_ids_index == iF, "Incorrect reference ids index (%d != %d)", reference_ids_index, iF);
      }
      else
      {
        // Check that channelset instances are compatible with shared ones.
        for (int ic = 0; ic < channelsets.size(); ic++)
        {
          (*shared_channelsets.get())[ic].CheckCompatible(channelsets[ic]);
        }
      }

      // Examples start after header (at current position) and end before cached channelset instances.
      if (examples_start_offset == c_examples_start_offset_init)
      {
        examples_start_offset = Platform::ftell64(ds_files_[iF]);
      }
      else
      {
        CHECK(examples_start_offset == Platform::ftell64(ds_files_[iF]),
          "Inconsistent example start offsets %ld and %ld accross files %s and %s",
          examples_start_offset,
          Platform::ftell64(ds_files_[iF]),
          parameters.ds_files_paths[reference_ids_index].c_str(),
          parameters.ds_files_paths[iF].c_str());
      }

      // Read cached channelset instances.
      Platform::fseek64(ds_files_[iF], 0, SEEK_END);
      int64_t file_size = Platform::ftell64(ds_files_[iF]);
      int64_t cached_channelset_instances_size = file_size - header.cached_channelset_instances_start_;
      vector<ChannelSetInstance> cached_channelset_instances;
      cached_channelset_instances.resize(cached_channelset_instances_size / sizeof(ChannelSetInstance));
      CHECK((cached_channelset_instances_size % sizeof(ChannelSetInstance)) == 0,
        "Invalid cached_channelset_instances_size %d (not multiple of sizeof(ChannelSetInstance) = %u).",
        cached_channelset_instances_size, sizeof(ChannelSetInstance));
      CHECK(cached_channelset_instances.size() > 0, "Cached channelset instances size cannot be zero.");
      Platform::fseek64(ds_files_[iF], header.cached_channelset_instances_start_, SEEK_SET);
      CHECK_ERRNO(
        fread(
        cached_channelset_instances.data(),
        sizeof(ChannelSetInstance),
        cached_channelset_instances.size(),
        ds_files_[iF]
        ) == cached_channelset_instances.size(),
        "Reading cached channelset instanced for file %s failed.", parameters.ds_files_paths[iF].c_str());

      // Remember where this file channelset instances start in vector of all channelset instances.
      channelset_instances_file_start[iF] = cached_channelset_instances_all.size();
      // Append channelset instances to the vector of all cached channelset instances.
      cached_channelset_instances_all.insert(
        cached_channelset_instances_all.end(),
        cached_channelset_instances.begin(),
        cached_channelset_instances.end()
        );
    }

    // Move all read objects to extended header.
    deserialized_header_.reset(
      new DeserializedDsHeader(
      *sharedHeader.get(),
      move(*shared_channelsets.get()),
      move(cached_channelset_instances_all),
      move(channelset_instances_file_start)
      )
      );

    return examples_start_offset;
  }

  // Given parsed IDS files result calculates parts of the files relevant for this deserializer.
  vector<IdsFileExamplesPortion> CalculateFilePortions(
    const DeserializeParameters& parameters
    )
  {
    // Now handle distributed reading, each deserializer needs to read equal number of examples (if examples count
    // is not divisible with deserializers count first n deserializers will read one more sample (where n is division
    // remainder)).
    struct DeserializerExamplesPortion
    {
      size_t start_example_;
      size_t examples_count_;
    };
    // We will calculate example counts for all deserializers (distribute them as evenly as possible using strategy
    // described above).
    vector<DeserializerExamplesPortion> examples_per_deserializer(parameters.deserializers_count_);
    size_t total_examples_count = deserialized_header_->GetExamplesCount();
    size_t one_deserializer_examples_count = total_examples_count / parameters.deserializers_count_;
    // Ensure we properly distribute leftover examples (add 1 to first deserializers).
    size_t leftover_samples = total_examples_count % parameters.deserializers_count_;
    for (size_t ir = 0; ir < parameters.deserializers_count_; ir++)
    {
      examples_per_deserializer[ir].examples_count_ = one_deserializer_examples_count;
      if (ir < leftover_samples)
      {
        examples_per_deserializer[ir].examples_count_++;
      }
    }
    // Now calculate start example position for all deserializers.
    examples_per_deserializer[0].start_example_ = 0;
    for (size_t ir = 1; ir < parameters.deserializers_count_; ir++)
    {
      examples_per_deserializer[ir].start_example_ =
        examples_per_deserializer[ir - 1].start_example_ + examples_per_deserializer[ir - 1].examples_count_;
    }

    // Calculate examples per ids file for this deserializer.
    vector<IdsFileExamplesPortion> examples_portion_per_file;
    // Take this deserializer boundaries.
    int64_t this_deserializer_start_example = examples_per_deserializer[parameters.deserializer_index_].start_example_;
    int64_t this_deserializer_end_example =
      this_deserializer_start_example + examples_per_deserializer[parameters.deserializer_index_].examples_count_;
    int64_t curr_file_start_example = 0;
    for (size_t iF = 0; iF < deserialized_header_->GetFilesCount(); iF++)
    {
      int64_t examples_per_file = static_cast<int64_t>(deserialized_header_->GetExamplesCountPerFile(iF));
      int64_t curr_file_end_example = curr_file_start_example + examples_per_file;
      if (curr_file_end_example > this_deserializer_start_example)
      {
        // This deserializer deserializes part of this file. Save this part to do chunking later.
        IdsFileExamplesPortion new_file_portion;
        new_file_portion.ids_file_index = iF;
        new_file_portion.ids_file_start_example = this_deserializer_start_example - curr_file_start_example;
        new_file_portion.ids_file_end_example =
          min(this_deserializer_end_example - curr_file_start_example, examples_per_file);
        examples_portion_per_file.emplace_back(new_file_portion);

        // Adjust deserializer start example for the next file.
        this_deserializer_start_example = curr_file_end_example;
      }
      // Adjust file start example for the next file.
      curr_file_start_example += examples_per_file;
      if (curr_file_start_example >= this_deserializer_end_example)
      {
        // We passed range of interest, stop looping.
        break;
      }
    }

    return examples_portion_per_file;
  }

  // Peforms chunking of part of file this deserializer works with.
  void CalculateChunks(
    const DeserializeParameters& parameters,
    const vector<IdsFileExamplesPortion>& examples_portion_per_file,
    int64_t examples_start_offset
    )
  {
    // Loop over all file portions associated with this deserializer and calculate chunks.
    for (const IdsFileExamplesPortion& curr_examples_portion_per_file : examples_portion_per_file)
    {
      size_t file_index = curr_examples_portion_per_file.ids_file_index;
      size_t examples_per_current_file =
        curr_examples_portion_per_file.ids_file_end_example - curr_examples_portion_per_file.ids_file_start_example;
      const ChannelSetInstance* channelset_instances_per_file =
        deserialized_header_->GetChannelsetInstancesForFile(file_index);

      // Calculate examples range offsets for this deserializer in current file.
      const int64_t example_header_size = deserialized_header_->GetChannelsetsCount() * sizeof(ChannelSetInstance);
      int curr_cached_channelset_instance_start = 0;
      // Loop from the beginning over all examples in current file that are not handled by this deserializer
      // and update start offset (keep adding to it).
      int64_t file_examples_start_offset = examples_start_offset;
      for (int64_t ie = 0; ie < curr_examples_portion_per_file.ids_file_start_example; ie++)
      {
        for (size_t ic = 0; ic < deserialized_header_->GetChannelsetsCount(); ic++)
        {
          file_examples_start_offset += channelset_instances_per_file[curr_cached_channelset_instance_start + ic].size;
        }
        file_examples_start_offset += example_header_size;
        curr_cached_channelset_instance_start += deserialized_header_->GetChannelsetsCount();
      }

      // We are at start here, remember our cached instance start.
      int cached_channelset_instance_start = curr_cached_channelset_instance_start;
      // Now calculate end offset (starts with start offset and keep adding).
      int curr_cached_channelset_instance_end = curr_cached_channelset_instance_start;
      int64_t file_examples_end_offset = file_examples_start_offset;
      for (size_t ie = 0; ie < examples_per_current_file; ie++)
      {
        for (size_t ic = 0; ic < deserialized_header_->GetChannelsetsCount(); ic++)
        {
          file_examples_end_offset += channelset_instances_per_file[curr_cached_channelset_instance_end + ic].size;
        }
        file_examples_end_offset += example_header_size;
        curr_cached_channelset_instance_end += deserialized_header_->GetChannelsetsCount();
      }
      // We are at the end here, remember our cached instance end.
      int cached_channelset_instance_end = curr_cached_channelset_instance_end;
      // Here we have all data related to examples portion for this deserializer. Now we can do chunking.

      // Calculate average prefetch size based on desired prefetch size.
      int64_t examples_total_size = file_examples_end_offset - file_examples_start_offset;
      CHECK(examples_total_size > 0, "examples_total_size cannot be 0.");
      const int64_t desired_prefetch_size = parameters.prefetch_size;
      int64_t chunks_count = (examples_total_size + desired_prefetch_size - 1) / desired_prefetch_size;
      int64_t average_prefetch_size = examples_total_size / chunks_count;

      // Partition file into chunks with the size roughly equal to average prefetch size. We will use these chunks to
      // load portions of the file into the memory.
      int64_t offset = file_examples_start_offset;
      int64_t curr_chunk_size = 0;
      int64_t curr_chunk_start = offset;
      int curr_cached_channelset_instance_chunk = cached_channelset_instance_start;
      while (offset < file_examples_end_offset)
      {
        // Calculate the size of the example.
        int example_data_size = 0;
        for (size_t ic = 0; ic < deserialized_header_->GetChannelsetsCount(); ic++)
        {
          example_data_size += channelset_instances_per_file[curr_cached_channelset_instance_chunk + ic].size;
        }

        int64_t example_size = example_header_size + example_data_size;

        if (curr_chunk_size + example_size > average_prefetch_size)
        {
          // Done with this chunk, save it.
          max_chunk_size_ = max(max_chunk_size_, curr_chunk_size + example_size);
          chunks_.emplace_back(curr_examples_portion_per_file.ids_file_index, curr_chunk_start, curr_chunk_size + example_size);

          // Reset current chunk.
          curr_chunk_size = 0;
          curr_chunk_start = chunks_.back().start_offset + chunks_.back().size;
        }
        else
        {
          // Keep adding to current chunk.
          curr_chunk_size += example_size;
        }

        offset += example_size;
        curr_cached_channelset_instance_chunk += deserialized_header_->GetChannelsetsCount();
      }
      CHECK(offset == file_examples_end_offset,
        "offset %d must be equal to examples_end_offset %d.", offset, file_examples_end_offset);
      CHECK(curr_cached_channelset_instance_chunk == cached_channelset_instance_end,
        "Mismatch between curr_cached_channelset_instance_chunk (%d) and cached_channelset_instance_end (%d)",
        curr_cached_channelset_instance_chunk, cached_channelset_instance_end);
      // Save the last chunk if we have file portion left at the end.
      if (curr_chunk_size != 0)
      {
        max_chunk_size_ = max(max_chunk_size_, curr_chunk_size);
        chunks_.emplace_back(curr_examples_portion_per_file.ids_file_index, curr_chunk_start, curr_chunk_size);
      }
    }
  }

  virtual pair<Chunk, MemoryChunk*> BackgroundProcesingMethod(Chunk& chunk, int thread_id)
  {
    if (events_sink_ != nullptr)
    {
      events_sink_->DataReadStart(thread_id);
    }

    // Take new memory buffer.
    MemoryChunk* buffer = mem_mgr_->Alloc();
    buffer->mem_mgr_ = mem_mgr_.get();

    CHECK(chunk.size <= buffer->memory_size_,
      "Chunk size %d greater than buffer size %d.", chunk.size, buffer->memory_size_);
    // Load from disk.
    Platform::fseek64(ds_files_[chunk.file_index], chunk.start_offset, SEEK_SET);
    CHECK_ERRNO(
      fread(buffer->memory_, sizeof(char), chunk.size, ds_files_[chunk.file_index]) == static_cast<size_t>(chunk.size),
      "Reading chunk start_offset=%d, size=%d failed", chunk.start_offset, chunk.size
      );

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

  unique_ptr<DeserializedDsHeader> deserialized_header_;

  vector<FILE*> ds_files_;

  vector<Chunk> chunks_;
  size_t curr_chunk_;
  int64_t max_chunk_size_;

  bool shuffle_chunks_;

  // Externally provided object that acts as a event sink for IDS loading related events.
  DatasetEventsSink* events_sink_;
};

unique_ptr<IIDSDeserializer> CreateIdsDeserializer(const DeserializeParameters& parameters)
{
  return unique_ptr<IIDSDeserializer>(new DsDeserializerWithPrefetching(parameters));
}