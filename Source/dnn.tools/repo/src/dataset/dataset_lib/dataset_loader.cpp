#include "dataset_io.hpp"
#include "dataset.hpp"
#include "dataset_events_sink.hpp"
#include "deserializer.hpp"
#include "background_workers_pool.hpp"
#include "transformer.hpp"
#include "fixed_memory_manager.hpp"
#include "check.hpp"
#include "decompressor.hpp"
#include "proto_io.hpp"

#include "ds_load_params.hpp"

#include <random>
#include <unordered_map>
#include <unordered_set>

using namespace std;

// Helper structure that holds info needed to create transformable channelsets within TransformableExample.
struct ChannelsetInitInfo
{
  ChannelsetInitInfo(const char* channelset_name, const ChannelsetID* channelset_id, int memory) :
    channelset_name_(channelset_name), channelset_id_(channelset_id), memory_(memory) {}

  const ChannelsetID* channelset_id_;
  const char* channelset_name_;
  int memory_;
};

// Helper class that holds blob description.
class BlobDesc
{
public:
  BlobDesc(const string& name, vector<const ChannelsetID*>&& ids) : name_(name), channelset_ids_(move(ids))
  {
  }

  const string* GetName() const
  {
    return &name_;
  }

  int GetChannelsetsCount() const
  {
    return static_cast<int>(channelset_ids_.size());
  }

  const ChannelsetID* GetChannelsetsID(int ic) const
  {
    return channelset_ids_[ic];
  }

private:
  string name_;
  vector<const ChannelsetID*> channelset_ids_;
};

// Extends TransformableChannelset to hold some additional data. Once transformable channelsets are initialized
// DeserializedChannelsets should not be used, for all calls transformable objects (channelsets, examples) should be used.
class TransformableChannelsetEx : public TransformableChannelset
{
public:
  TransformableChannelsetEx(const ChannelsetID* channelset_id, const char* channelset_name, float* final_mem, float* work_mem)
    : TransformableChannelset(channelset_name, final_mem, work_mem), channelset_id_(channelset_id)
  {
  }

  void Init(DeserializedChannelsets* deserialized_channelsets)
  {
    deserialized_channelsets_ = deserialized_channelsets;

    const ChannelSet* channelset = deserialized_channelsets_->GetChannelset(channelset_id_);
    const ChannelSetInstance* channelset_inst = deserialized_channelsets_->GetChannelsetInstance(channelset_id_);
    SetChannels(channelset->channels);
    SetWidth(channelset_inst->width);
    SetHeight(channelset_inst->height);
  }

  const ChannelsetID* GetChannelsetID()
  {
    return channelset_id_;
  }

  const char* GetDeserializedChannelsetMemory() const
  {
    return deserialized_channelsets_->GetChannelsetMemory(channelset_id_);
  }

  int GetDeserializedChannelsetMemorySize() const
  {
    return deserialized_channelsets_->GetChannelsetInstance(channelset_id_)->size;
  }

  Compression GetDeserializedChannelsetMemoryCompression() const
  {
    return deserialized_channelsets_->GetChannelset(channelset_id_)->compression;
  }

private:
  const ChannelsetID* channelset_id_;                 // Channelset id that identifies original deserialized channelset.
  DeserializedChannelsets* deserialized_channelsets_; // All deserialized channelsets.
};

// Helper class that combines results from deserializer and adapts them to input expected by transformer. This class is
// expected to be allocated frequently and it should be managed by memory manager.
struct TransformableExample : ITransformableChannelsetIterator
{
public:
  TransformableExample(float* memory, size_t memory_size, const vector<ChannelsetInitInfo>& channelset_init_info)
  {
    // We should get equal memory for the image and workspace.
    CHECK(memory_size % 2 == 0, "Memory size in TransformableExample must be even, %u given.", memory_size);
    // Initialize image and workspace memory.
    float* final_memory = memory;
    float* workspace_memory = &memory[memory_size / 2];

    // Create adapters for transformers (channelset_init_info describes init parameters for each channelset).
    transformable_channelsets_sp_.resize(channelset_init_info.size());
    float* curr_final_mem = final_memory;
    float* curr_worspace_mem = workspace_memory;
    for (size_t ic = 0; ic < channelset_init_info.size(); ic++)
    {
      const ChannelsetInitInfo& chs_info = channelset_init_info[ic];
      transformable_channelsets_sp_[ic].reset(new TransformableChannelsetEx(
        chs_info.channelset_id_, chs_info.channelset_name_, curr_final_mem, curr_worspace_mem));

      curr_final_mem += chs_info.memory_;
      curr_worspace_mem += chs_info.memory_;
    }
  }

  ~TransformableExample()
  {
    // If we have not been deinitialized make sure we do it now.
    Deinit();
  }

  // Performs initialization with given deserialized channelsets. From this call DeserializedChannelsets is owned by
  // this object. It will be released either by deinit call or in destructor.
  void Init(DeserializedChannelsets&& deserialized_channelsets)
  {
    deserialized_channelsets_ = std::move(deserialized_channelsets);

    // Initialize contained transformable channelsets.
    for (size_t itc = 0; itc != transformable_channelsets_sp_.size(); itc++)
    {
      transformable_channelsets_sp_[itc]->Init(&deserialized_channelsets_);
    }
  }

  // Releases DeserializedChannelsets object.
  void Deinit()
  {
    deserialized_channelsets_.~DeserializedChannelsets();
  }

  // Returns transformable channelset set that wraps channelset with given ID.
  TransformableChannelsetEx* GetTransformableChannelset(const ChannelsetID* channelset_id)
  {
    TransformableChannelsetEx* result = nullptr;
    for (size_t itc = 0; itc != transformable_channelsets_sp_.size(); itc++)
    {
      if (transformable_channelsets_sp_[itc]->GetChannelsetID() == channelset_id)
      {
        result = transformable_channelsets_sp_[itc].get();
        break;
      }
    }
    CHECK(result != nullptr, "Invalid channelset id.");
    return result;
  }

  // Returns number of transformable channelsets.
  virtual int GetTransformableChannelsetsCount() override
  {
    return static_cast<int>(transformable_channelsets_sp_.size());
  }

  // Returns transformable channelset at the given index.
  virtual TransformableChannelsetEx* GetTransformableChannelset(int index) override
  {
    CHECK(index >= 0 && index < GetTransformableChannelsetsCount(), "Invalid channelset index.");
    return transformable_channelsets_sp_[index].get();
  }

private:
  // Transformable example must be managed by memory manager and should not be copied.
  TransformableExample(const TransformableExample& other) = delete;
  TransformableExample(TransformableExample&& other) = delete;

  TransformableExample& operator=(const TransformableExample& other) = delete;
  TransformableExample& operator=(TransformableExample&& other) = delete;

private:
  vector<unique_ptr<TransformableChannelsetEx>> transformable_channelsets_sp_;

  DeserializedChannelsets deserialized_channelsets_;
};

template <typename Dtype>
class DsLoaderImpl : public BackgroundWorkersPool<DeserializedChannelsets, TransformableExample*>, public IDsLoader<Dtype>
{
public:
  DsLoaderImpl(const DsLoadParameters& parameters, DatasetEventsSink* events_sink) :
    BackgroundWorkersPool<DeserializedChannelsets, TransformableExample*>(parameters.threads_count())
  {
    // Save configuration.
    configuration_string_ = parameters.DebugString();

    // Save events sink.
    events_sink_ = events_sink;

    shuffle_examples_ = parameters.shuffle_examples();

    // Kick off ids file deserializing.
    unique_ptr<IIDSDeserializer> ids_deserializer = CreateIdsDeserializer({
      parameters.source(),
      parameters.disk_prefetch_size(),
      events_sink,
      parameters.shuffle_chunks()
    });

    // Save total number of examples.
    total_examples_count_ = ids_deserializer->GetExamplesCount();

    // Create blob descriptors based on given parameters. This method also returns all used channelsets.
    vector<const ChannelsetID*> used_channelset_ids = CreateBlobDescriptors(parameters, ids_deserializer.get());

    for (int it = 0; it < parameters.transform_parameter_size(); it++)
    {
      transformers_.push_back(CreateTransformer(parameters.transform_parameter(it)));
    }

    // Determine maximal channelset dimensions as we move through transformers.
    vector<int> max_channelset_memory(used_channelset_ids.size());
    // Go over all channelsets.
    for (size_t ic = 0; ic < used_channelset_ids.size(); ic++)
    {
      // Take current channelset.
      const ChannelsetID* channelset_id = used_channelset_ids[ic];
      const ChannelSet* channelset = ids_deserializer->GetChannelset(channelset_id);

      // Initialize max channelset dims to initial values.
      max_channelset_memory[ic] = channelset->channels *
        ids_deserializer->GetExampleChannelsetInstance(0, channelset_id)->height *
        ids_deserializer->GetExampleChannelsetInstance(0, channelset_id)->width;

      // Go over all channelset instances in IDS and calculate maximal dimensions.
      for (int ie = 0; ie < ids_deserializer->GetExamplesCount(); ie++)
      {
        // Loop through transformers and ask for size.
        int curr_width = ids_deserializer->GetExampleChannelsetInstance(ie, channelset_id)->width;
        int curr_height = ids_deserializer->GetExampleChannelsetInstance(ie, channelset_id)->height;
        max_channelset_memory[ic] = max(max_channelset_memory[ic], channelset->channels * curr_height * curr_width);
        for (size_t it = 0; it < transformers_.size(); it++)
        {
          const int curr_workspace_memory =
            transformers_[it]->GetRequiredWorkspaceMemory(channelset->name, curr_width, curr_height, channelset->channels);
          transformers_[it]->GetTransformedSize(channelset->name, curr_width, curr_height, curr_width, curr_height);
          const int curr_output_memory = channelset->channels * curr_height * curr_width;
          const int curr_memory = max(curr_workspace_memory, curr_output_memory);
          max_channelset_memory[ic] = max(max_channelset_memory[ic], curr_memory);
        }
      }
    }

    // Given maximal dimensions calculate maximal memory required for each channelset.
    int max_memory = 0;
    vector<ChannelsetInitInfo> channelset_init_info;
    for (size_t ic = 0; ic < used_channelset_ids.size(); ic++)
    {
      const ChannelSet* channelset = ids_deserializer->GetChannelset(used_channelset_ids[ic]);
      const int required_memory = max_channelset_memory[ic];
      channelset_init_info.emplace_back(channelset->name, used_channelset_ids[ic], required_memory);
      max_memory += required_memory;
    }

    // Create pool of TransformableExamples of the given chache_size.
    mem_mgr_.reset(new FixedMemoryManager<TransformableExample, float, vector<ChannelsetInitInfo>>(
      2 * max_memory, parameters.cache_size(), channelset_init_info, true));

    // Move ownership to member variable. From now on we just need to collect examples, no need to take dataset descriptions
    // so we will just retain IExamplesDeserializer pointer (all needed descriptions should be available through
    // TransformableExample and TransformableChannelsets).
    examples_deserializer_ = unique_ptr<IExamplesDeserializer>(ids_deserializer.release());

    // Take first set of examples from deserializer and randomize them.
    deserialized_examples_ = examples_deserializer_->GetExamples();
    RandomizeExamples();

    if (events_sink != nullptr)
    {
      events_sink->ImageProcessingThreadsCount(parameters.threads_count());
    }

    // Push initial set of examples for background processing.
    InitJobsStack(parameters.cache_size());
  }

  virtual ~DsLoaderImpl()
  {
    // First we need to stop all background threads and release all their internally held objects.
    AbortAll();
    // Deserialized examples must be released before releasing deserializer.
    deserialized_examples_.clear();
    // Transformable examples also hold reference to deserialized examples, release memory manager to clean up.
    mem_mgr_.reset(nullptr);
    // Now we can release deserializer.
    examples_deserializer_->AbortDeserializing();
  }

  // Returns number of blobs per example.
  virtual int GetBlobsCount() override
  {
    return static_cast<int>(blob_descriptors_.size());
  }

  // Returns name of the blob at the given index.
  virtual const char* GetBlobName(int index) override
  {
    return blob_descriptors_[index].GetName()->c_str();
  }

  // Returns number of examples in dataset.
  virtual int GetExamplesCount() override
  {
      return total_examples_count_;
  }

  // Returns one transformed example. After this call returned example is not available anymore as we move to the next one.
  virtual void GetExample(IExample<Dtype>* out_example) override
  {
    // Take one result from results queue.
    TransformableExample* transformable_example = PopResult();

    // Go over example blobs and copy them to user provided object.
    for (int ib = 0; ib < static_cast<int>(blob_descriptors_.size()); ib++)
    {
      const BlobDesc* blob_desc = &blob_descriptors_[ib];
      // First we need to inform client object of how much memory we need.
      int height = -1;
      int width = -1;
      int total_channels = 0;
      for (int ibc = 0; ibc < blob_desc->GetChannelsetsCount(); ibc++)
      {
        const ChannelsetID* channelset_id = blob_desc->GetChannelsetsID(ibc);
        TransformableChannelset* transformable_channelset = transformable_example->GetTransformableChannelset(channelset_id);
        CHECK((width == -1 && height == -1) || (width == transformable_channelset->Width() && height == transformable_channelset->Height()),
          "Channelsets inside blob have different dimensions");
        width = transformable_channelset->Width();
        height = transformable_channelset->Height();
        total_channels += transformable_channelset->Channels();
      }
      out_example->ReshapeBlob(ib, total_channels, height, width);

      // Now copy the blob to client provided memory.
      Dtype* dest_buffer = out_example->GetBlobMemory(ib);
      int channel_size = height * width;
      int channels_done = 0;
      // Go over the blob's channelsets and copy them one by one.
      for (int ibc = 0; ibc < blob_desc->GetChannelsetsCount(); ibc++)
      {
        // We need to "unpack" channelset into sequential channels.
        const ChannelsetID* channelset_id = blob_desc->GetChannelsetsID(ibc);
        TransformableChannelset* transformable_channelset = transformable_example->GetTransformableChannelset(channelset_id);
        float* src_buffer = transformable_channelset->GetFinalMemory();
        int src_index = 0;

        int channelset_channels = transformable_channelset->Channels();
        int dest_index = channels_done * channel_size;
        for (int iy = 0; iy < height; iy++)
        {
          for (int ix = 0; ix < width; ix++)
          {
            for (int i = 0; i < channelset_channels; i++)
            {
              dest_buffer[dest_index + i * channel_size] = static_cast<Dtype>(src_buffer[src_index++]);
            }
            dest_index++;
          }
        }
        channels_done += channelset_channels;
      }
    }

    // Done with this example, deinitialize transformable example and return it to the pool of available objects.
    transformable_example->Deinit();
    mem_mgr_->Dealloc(transformable_example);

    // If we delivered all examples ask deserializer for the new set of examples.
    if (curr_example_for_job_ == deserialized_examples_.size())
    {
      deserialized_examples_ = examples_deserializer_->GetExamples();
      RandomizeExamples();
      curr_example_for_job_ = 0;
    }

    // Add new job.
    PushJobWithMove(move(deserialized_examples_[curr_example_for_job_]));
    curr_example_for_job_++;
  }

  // Returns string that represent loading configuration.
  virtual const char* GetConfiguration() override
  {
    return configuration_string_.data();
  }

private:
  // Creates blob descriptors based on provided parameters. Returns ids of the channelsets used in the blobs.
  vector<const ChannelsetID*> CreateBlobDescriptors(const DsLoadParameters& parameters, IIDSDescriptor* ids_descriptor)
  {
    vector<const ChannelsetID*> used_channelset_ids;
    for (int ib = 0; ib < parameters.blob_size(); ib++)
    {
      // Check that we do not have two blobs with the same name.
      CHECK(
        find_if(
        blob_descriptors_.begin(),
        blob_descriptors_.end(),
        [&parameters, ib](const BlobDesc& blob_desc) { return *blob_desc.GetName() == parameters.blob(ib).name(); }
        ) == blob_descriptors_.end(),
        "Duplicate blob name %s", parameters.blob(ib).name().c_str()
        );

      vector<const ChannelsetID*> channelset_ids;
      for (int ic = 0; ic < parameters.blob(ib).channelset_size(); ic++)
      {
        const ChannelsetID* channelset_id = ids_descriptor->GetChannelsetID(parameters.blob(ib).channelset(ic));
        CHECK(channelset_id != nullptr, "Channelset %s not found in IDS file.", parameters.blob(ib).channelset(ic).c_str());

        // Check that each channelset is used at most once.
        CHECK(
          find(
          used_channelset_ids.begin(),
          used_channelset_ids.end(),
          channelset_id
          ) == used_channelset_ids.end(),
          "Channelset %s used more than once in blob definitions", parameters.blob(ib).channelset(ic).c_str());

        channelset_ids.push_back(channelset_id);
        used_channelset_ids.push_back(channelset_id);
      }
      blob_descriptors_.emplace_back(parameters.blob(ib).name(), move(channelset_ids));
    }
    return used_channelset_ids;
  }

  // Method used to process examples in background thread. Effectively decompresses image and performs transforms on
  // top of the decompressed result.
  virtual TransformableExample* BackgroundProcesingMethod(DeserializedChannelsets& deserialized_channelsets, int thread_id) override
  {
    if (events_sink_ != nullptr)
    {
      events_sink_->ImageProcessingStart(thread_id);
    }

    // Take new processed example from the pool of available examples and initialize it.
    TransformableExample* transformable_example = mem_mgr_->Alloc();
    transformable_example->Init(move(deserialized_channelsets));

    // First we need to decompress channelsets.
    for (int ics = 0; ics < transformable_example->GetTransformableChannelsetsCount(); ics++)
    {
      TransformableChannelsetEx* transformable_channelset = transformable_example->GetTransformableChannelset(ics);

      const char* deserialized_mem = transformable_channelset->GetDeserializedChannelsetMemory();
      int deserialized_mem_size = transformable_channelset->GetDeserializedChannelsetMemorySize();
      Compression deserialized_mem_compression = transformable_channelset->GetDeserializedChannelsetMemoryCompression();
      float* out_mem = transformable_channelset->GetFinalMemory();
      float* work_mem = transformable_channelset->GetWorkspaceMemory();
      int channels = transformable_channelset->Channels();
      int height = transformable_channelset->Height();
      int width = transformable_channelset->Width();

      Decompress(deserialized_mem, deserialized_mem_size, channels, height, width, out_mem, work_mem, deserialized_mem_compression);
    }

    // Loop over transformers and apply them to channelsets.
    for (size_t it = 0; it < transformers_.size(); it++) {
      transformers_[it]->Transform(*transformable_example);
    }

    if (events_sink_ != nullptr)
    {
      events_sink_->ImageProcessingEnd(thread_id);
    }

    return transformable_example;
  }

  // Shuffles set of examples randomly.
  void RandomizeExamples()
  {
    if (shuffle_examples_)
    {
      shuffle(deserialized_examples_.begin(), deserialized_examples_.end(), default_random_engine(0));
    }
  }

  // Push initial set of examples for background processing.
  void InitJobsStack(int maxJobs)
  {
    curr_example_for_job_ = 0;
    while (curr_example_for_job_ < maxJobs && curr_example_for_job_ < deserialized_examples_.size())
    {
      PushJobWithMove(move(deserialized_examples_[curr_example_for_job_]));
      curr_example_for_job_++;
    }
  }

private:
  unique_ptr<IExamplesDeserializer> examples_deserializer_;
  vector<DeserializedChannelsets> deserialized_examples_;
  vector<BlobDesc> blob_descriptors_;
  bool shuffle_examples_;
  int total_examples_count_;

  size_t curr_example_for_job_;

  vector<unique_ptr<ITransformer>> transformers_;

  unique_ptr<FixedMemoryManager<TransformableExample, float, vector<ChannelsetInitInfo>>> mem_mgr_;

  string configuration_string_;

  DatasetEventsSink* events_sink_;
};

template <typename Dtype>
unique_ptr<IDsLoader<Dtype>> CreateLoader(const string& params_file_path, DatasetEventsSink* events_sink) {
  DsLoadParameters load_parameters;
  ReadProtoFromTextFile(params_file_path.c_str(), &load_parameters);
  return unique_ptr<IDsLoader<Dtype>>(new DsLoaderImpl<Dtype>(load_parameters, events_sink));
}

template
unique_ptr<IDsLoader<float>> CreateLoader<float>(const string& params_file_path, DatasetEventsSink* events_sink);
template
unique_ptr<IDsLoader<double>> CreateLoader<double>(const string& params_file_path, DatasetEventsSink* events_sink);