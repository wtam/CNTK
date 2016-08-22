#include "dataset_io.hpp"
#include "dataset.hpp"
#include "dataset_events_sink.hpp"
#include "prefetcher.hpp"
#include "background_workers_pool.hpp"
#include "transformer.hpp"
#include "fixed_memory_manager.hpp"
#include "check.hpp"
#include "decompressor.hpp"
#include "proto_io.hpp"

#include "ds_load_params.hpp"

#include <random>

using namespace std;

struct ChannelsetInitInfo
{
  ChannelsetInitInfo(const char* channelset_name, const char* blob_name, int memory) :
    channelset_name_(channelset_name), blob_name_(blob_name), memory_(memory) {}

  const char* channelset_name_;
  const char* blob_name_;
  int memory_;
};

// Helper class that combines results from prefether and adapts them to input expected by transformer. This class is
// expected to be allocated frequently and it should be managed by memory manager.
struct ProcessedExample
{
public:
  ProcessedExample(float* memory, size_t memory_size, const vector<ChannelsetInitInfo>& channelset_init_info) : memory_size_(memory_size)
  {
    // We shoud get equal memory for the image and workspace.
    CHECK(memory_size_ % 2 == 0);
    // Initialize image and workspace memory.
    final_memory_ = memory;
    workspace_memory_ = &memory[memory_size_ / 2];

    // Create adapters for transformers (channelset_init_info describes init parameters for each channelset).
    transformable_channelsets_sp_.resize(channelset_init_info.size());
    transformable_channelsets_.resize(channelset_init_info.size());
    float* curr_final_mem = final_memory_;
    float* curr_worspace_mem = workspace_memory_;
    for (size_t ic = 0; ic < channelset_init_info.size(); ic++)
    {
      const ChannelsetInitInfo& chs_info = channelset_init_info[ic];
      transformable_channelsets_sp_[ic].reset(new TransformableChannelset(
        chs_info.blob_name_, chs_info.channelset_name_, curr_final_mem, curr_worspace_mem));
      transformable_channelsets_[ic] = transformable_channelsets_sp_[ic].get();

      curr_final_mem += chs_info.memory_;
      curr_worspace_mem += chs_info.memory_;
    }
  }

  ~ProcessedExample()
  {
    // If we have not been deinitialized make sure we do it now.
    Deinit();
  }

  // Performs initialization with given example. From this call ExampleEx is owned by this object. It will be released
  // either by deinit call or in destructor.
  void Init(ExampleEx&& ex)
  {
    example_ = std::move(ex);
    // Here we need to setup TransformableChannelset dimensions.
    CHECK(example_.GetChannelsetsCount() == transformable_channelsets_.size());

    for (int ic = 0; ic < example_.GetChannelsetsCount(); ic++)
    {
      const ChannelSet* channelset = example_.GetChannelset(ic);
      const ChannelSetInstance* channelset_inst = example_.GetChannelsetInstance(ic);
      transformable_channelsets_[ic]->SetChannels(channelset->channels);
      transformable_channelsets_[ic]->SetWidth(channelset_inst->width);
      transformable_channelsets_[ic]->SetHeight(channelset_inst->height);
    }
  }

  // Releases ExampleEx object.
  void Deinit()
  {
    example_.~ExampleEx();
  }

  // Returns transformable channel set at the given index.
  TransformableChannelset* GetTransformableChannelset(int ic)
  {
    CHECK(ic >= 0 && ic < transformable_channelsets_.size(), "Invalid channelset index.");
    return transformable_channelsets_[ic];
  }

  // Returns all transformable channelsets.
  const vector<TransformableChannelset*>* GetTransformableChannelsets()
  {
    return &transformable_channelsets_;
  }

  // Returns reference to owned ExampleEx object.
  const ExampleEx& GetExample()
  {
    return example_;
  }

private:
  // Processed example must be managed by memory manager and should not be copied.
  ProcessedExample(const ProcessedExample& other) = delete;
  ProcessedExample(ProcessedExample&& other) = delete;

  ProcessedExample& operator=(const ProcessedExample& other) = delete;
  ProcessedExample& operator=(ProcessedExample&& other) = delete;

private:
  float* final_memory_;
  float* workspace_memory_;
  size_t memory_size_;

  vector<unique_ptr<TransformableChannelset>> transformable_channelsets_sp_;
  vector<TransformableChannelset*> transformable_channelsets_;

  ExampleEx example_;
};

template <typename Dtype>
class DsLoaderImpl : public BackgroundWorkersPool<ExampleEx, ProcessedExample*>, public IDsLoader<Dtype>
{
public:
  DsLoaderImpl(const DsLoadParameters& parameters, DatasetEventsSink* events_sink) :
    BackgroundWorkersPool<ExampleEx, ProcessedExample*>(parameters.threads_count())
  {
    // Save configuration.
    configuration_string_ = parameters.DebugString();

    // Save events sink.
    events_sink_ = events_sink;

    shuffle_examples_ = parameters.shuffle_examples();

    // Kick off ids file prefetching.
    prefetcher_ = CreatePrefetcher({
      parameters.source(),
      parameters.disk_prefetch_size(),
      events_sink,
      parameters.shuffle_chunks()
    });

    for (int it = 0; it < parameters.transform_parameter_size(); it++)
    {
      transformers_.push_back(CreateTransformer(parameters.transform_parameter(it)));
    }

    const DsHeaderEx* header = prefetcher_->GetHeaderEx();

    // Determine maximal channelset dimensions as we move through transformers.
    vector<int> max_channelset_heights(header->GetChannelsetsCount());
    vector<int> max_channelset_widths(header->GetChannelsetsCount());
    const vector<ChannelSetInstance>& cached_channelset_instances = *header->GetChannelsetInstances();
    // Go over all blobs.
    for (int ib = 0; ib < header->GetBlobsCount(); ib++)
    {
      const vector<int>& channelset_inds_per_blob = *header->GetChannelsetsForBlob(ib);

      // Loop through all blob's channelsets and ask transformers how they change their size.
      for (size_t ic = 0; ic < channelset_inds_per_blob.size(); ic++)
      {
        // Take current channelset.
        int channelset_ind = channelset_inds_per_blob[ic];
        const ChannelSet* channelset = header->GetChannelset(channelset_ind);

        // Initialize max channelset dims to initial values.
        max_channelset_heights[channelset_ind] = cached_channelset_instances[channelset_ind].height;
        max_channelset_widths[channelset_ind] = cached_channelset_instances[channelset_ind].width;

        // Go over all channelset instances in cached array and calculate maximal dimensions.
        size_t example_start_index = 0;
        while (example_start_index < cached_channelset_instances.size())
        {
          // Loop through transformers and ask for size.
          int curr_width = cached_channelset_instances[example_start_index + channelset_ind].width;
          int curr_height = cached_channelset_instances[example_start_index + channelset_ind].height;
          max_channelset_heights[channelset_ind] = max(max_channelset_heights[channelset_ind], curr_height);
          max_channelset_widths[channelset_ind] = max(max_channelset_widths[channelset_ind], curr_width);
          for (size_t it = 0; it < transformers_.size(); it++)
          {
            transformers_[it]->GetTransformedSize(header->GetBlobName(ib), channelset->name, curr_width, curr_height, curr_width, curr_height);
            max_channelset_heights[channelset_ind] = max(max_channelset_heights[channelset_ind], curr_height);
            max_channelset_widths[channelset_ind] = max(max_channelset_widths[channelset_ind], curr_width);
          }
          // Move to the next example.
          example_start_index += header->GetChannelsetsCount();
        }
      }

      // All channelsets inside blob after transformers must have same size.
      const int channelset_ind_0 = channelset_inds_per_blob[0];
      for (size_t ic = 1; ic < channelset_inds_per_blob.size(); ic++)
      {
        const int channelset_ind = channelset_inds_per_blob[ic];
        CHECK(max_channelset_heights[channelset_ind_0] == max_channelset_heights[channelset_ind],
          "Channelset dimensions inside one blob differ after transformers.");
        CHECK(max_channelset_widths[channelset_ind_0] == max_channelset_widths[channelset_ind],
          "Channelset dimensions inside one blob differ after transformers.");
      }
    }

    // Given maximal dimensions calculate maximal memory required for each channelset.
    int max_memory = 0;
    vector<ChannelsetInitInfo> channelset_init_info;
    for (int ib = 0; ib < header->GetBlobsCount(); ib++)
    {
      const char* blob_name = header->GetBlobName(ib);
      const vector<int>& blob_channelset_inds = *header->GetChannelsetsForBlob(ib);
      for (size_t ic = 0; ic < blob_channelset_inds.size(); ic++)
      {
        const int ics = blob_channelset_inds[ic];
        const ChannelSet* channelset = header->GetChannelset(ics);
        const int required_memory = channelset->channels * max_channelset_heights[ics] * max_channelset_widths[ics];
        channelset_init_info.emplace_back(channelset->name, blob_name, required_memory);
        max_memory += required_memory;
      }
    }

    // Create pool of ProcessedExamples of the given chache_size.
    mem_mgr_.reset(new FixedMemoryManager<ProcessedExample, float, vector<ChannelsetInitInfo>>(
      2 * max_memory, parameters.cache_size(), channelset_init_info, true));

    // Take first set of examples from prefetcher and randomize them.
    examples_ = prefetcher_->GetExamples();
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
    // Examples must be released before releasing prefetcher.
    examples_.clear();
    // Processed examples also hold reference to examples, release memory manager to clean up.
    mem_mgr_.reset(nullptr);
    // Now we can release prefetcher.
    prefetcher_->AbortPrefeching();
  }

  // Returns number of blobs per example.
  virtual int GetBlobsCount() override
  {
    // Just peek one result and delegate call.
    ProcessedExample* processedExample = PeekResult();
    return processedExample->GetExample().GetBlobsCount();
  }

  // Returns name of the blob at the given index.
  virtual const char* GetBlobName(int index) override
  {
    // Just peek one result and delegate call.
    ProcessedExample* processedExample = PeekResult();
    return processedExample->GetExample().GetBlobName(index);
  }

  // Returns one processed example after this call returned example is not available anymore as we move to the next one.
  virtual void GetExample(IExample<Dtype>* out_example) override
  {
    // Take one result from results queue.
    ProcessedExample* processedExample = PopResult();

    // Go over example blobs and copy them to user provided object.
    int blobs_count = processedExample->GetExample().GetBlobsCount();
    for (int ib = 0; ib < blobs_count; ib++)
    {
      // First we need to inform client object of how much memory we need.
      int height = 0;
      int width = 0;
      int total_channels = 0;
      const vector<int>* channelsets_per_blob = processedExample->GetExample().GetChannelsetsForBlob(ib);
      for (size_t ic = 0; ic < channelsets_per_blob->size(); ic++)
      {
        int ics = (*channelsets_per_blob)[ic];
        const ChannelSet* channelset = processedExample->GetExample().GetChannelset(ics);
        TransformableChannelset* transformable_channelset = processedExample->GetTransformableChannelset(ics);
        width = transformable_channelset->Width();
        height = transformable_channelset->Height();
        total_channels += channelset->channels;
      }
      out_example->ReshapeBlob(ib, total_channels, height, width);

      // Now copy the blob to client provided memory.
      Dtype* dest_buffer = out_example->GetBlobMemory(ib);
      int channel_size = height * width;
      int channels_done = 0;
      // Go over the blob's channelsets and copy them one by one.
      for (size_t ic = 0; ic < channelsets_per_blob->size(); ic++)
      {
        // We need to "unpack" channelset into sequential channels.
        int ics = (*channelsets_per_blob)[ic];
        TransformableChannelset* transformable_channelset = processedExample->GetTransformableChannelset(ics);
        float* src_buffer = transformable_channelset->GetFinalMemory();
        int src_index = 0;

        const ChannelSet* channelset = processedExample->GetExample().GetChannelset(ics);
        int channelset_channels = channelset->channels;
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

    // Done with this example, deinitialize processed example and return it to the pool of available objects.
    processedExample->Deinit();
    mem_mgr_->Dealloc(processedExample);

    // If we delivered all examples ask prefether for the new set of examples.
    if (curr_example_for_job_ == examples_.size())
    {
      examples_ = prefetcher_->GetExamples();
      RandomizeExamples();
      curr_example_for_job_ = 0;
    }

    // Add new job.
    PushJobWithMove(move(examples_[curr_example_for_job_]));
    curr_example_for_job_++;
  }

  // Returns string that represent loading configuration.
  virtual const char* GetConfiguration() override
  {
    return configuration_string_.data();
  }

private:
  // Method used to process examples in background thread. Effectively decompresses image and performs transforms on
  // top of the decompressed result.
  virtual ProcessedExample* BackgroundProcesingMethod(ExampleEx& ex, int thread_id) override
  {
    if (events_sink_ != nullptr)
    {
      events_sink_->ImageProcessingStart(thread_id);
    }

    // Take new processed example from the pool of available examples and initialize it.
    ProcessedExample* proc_example = mem_mgr_->Alloc();
    proc_example->Init(move(ex));

    // First we need to decompress channelsets.
    for (int ics = 0; ics < proc_example->GetExample().GetChannelsetsCount(); ics++)
    {
      const ChannelSet* channelset = proc_example->GetExample().GetChannelset(ics);
      const ChannelSetInstance* channelset_inst = proc_example->GetExample().GetChannelsetInstance(ics);
      const char* channelset_mem = proc_example->GetExample().GetChannelsetMemory(ics);

      TransformableChannelset* transformable_channelset = proc_example->GetTransformableChannelset(ics);

      float* out_mem = transformable_channelset->GetFinalMemory();
      float* work_mem = transformable_channelset->GetWorkspaceMemory();

      Decompress(channelset_mem, channelset_inst->size, channelset->channels, channelset_inst->height, channelset_inst->width, out_mem, work_mem, channelset->compression);
    }

    // Loop over transformers and apply them to channelsets.
    for (size_t it = 0; it < transformers_.size(); it++) {
      transformers_[it]->Transform(*proc_example->GetTransformableChannelsets());
    }

    if (events_sink_ != nullptr)
    {
      events_sink_->ImageProcessingEnd(thread_id);
    }

    return proc_example;
  }

  // Shuffles set of examples randomly.
  void RandomizeExamples()
  {
    if (shuffle_examples_)
    {
      shuffle(examples_.begin(), examples_.end(), default_random_engine(0));
    }
  }

  // Push initial set of examples for background processing.
  void InitJobsStack(int maxJobs)
  {
    curr_example_for_job_ = 0;
    while (curr_example_for_job_ < maxJobs && curr_example_for_job_ < examples_.size())
    {
      PushJobWithMove(move(examples_[curr_example_for_job_]));
      curr_example_for_job_++;
    }
  }

private:
  unique_ptr<IPrefetcher> prefetcher_;
  vector<ExampleEx> examples_;
  bool shuffle_examples_;

  size_t curr_example_for_job_;

  vector<unique_ptr<ITransformer>> transformers_;

  unique_ptr<FixedMemoryManager<ProcessedExample, float, vector<ChannelsetInitInfo>>> mem_mgr_;

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