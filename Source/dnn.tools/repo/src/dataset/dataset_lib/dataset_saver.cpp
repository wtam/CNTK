#include "check.hpp"
#include "dataset.hpp"
#include "log.hpp"
#include "proto_io.hpp"
#include "platform.hpp"
#include "tensor.hpp"
#include "transformer_save.hpp"

#include "ds_save_params.hpp"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <fstream>
#include <memory>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <vector>
#include <unordered_set>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

using namespace std;

// Jpg encoding params.
const int c_jpg_quality = 100;
const vector<int> c_jpg_compression_params = { CV_IMWRITE_JPEG_QUALITY, c_jpg_quality };

// Png encoding params.
const int c_png_quality = 9;
const vector<int> c_png_compression_params = { CV_IMWRITE_PNG_COMPRESSION, c_png_quality };

// Encoding params struct.
struct ChannelsetEncodeParams
{
  Compression compression;
  string type;
  const vector<int>* params;
};

const ChannelsetEncodeParams c_raw_encode_params = { Compression::Raw, "", nullptr };
const ChannelsetEncodeParams c_jpg_encode_params = { Compression::Jpg, ".jpg", &c_jpg_compression_params };
const ChannelsetEncodeParams c_png_encode_params = { Compression::Png, ".png", &c_png_compression_params };

// Structure that stores config file header channelset description.
struct HeaderChannelSetDesc
{
  HeaderChannelSetDesc() = default;
  HeaderChannelSetDesc(HeaderChannelSetDesc&& other)
  {
    name_ = move(other.name_);
    compression_ = other.compression_;
    channels_ = other.channels_;
    save_transformers_ = move(other.save_transformers_);
  }

  void InitFromParams(const ChannelsetSaveParameters& channelset_save_params)
  {
    name_ = channelset_save_params.name();
    compression_ = HeaderChannelSetDesc::CompressionFromString(channelset_save_params.out_compression());
    channels_ = channelset_save_params.in_channels();
    save_transformers_ = HeaderChannelSetDesc::ParseTransforms(channelset_save_params);
  }

  HeaderChannelSetDesc(const HeaderChannelSetDesc& other) = delete;
  HeaderChannelSetDesc& operator=(const HeaderChannelSetDesc& other) = delete;

  const string* GetName() const
  {
    return &name_;
  }

  const int GetChannels() const
  {
    return channels_;
  }

  const Compression GetCompression() const
  {
    return compression_;
  }

  const vector<unique_ptr<ITransformerSave>>* GetTransformers() const
  {
    return &save_transformers_;
  }

private:
  static Compression CompressionFromString(const string& str)
  {
    if (str == "raw")
    {
      return Compression::Raw;
    }
    else if (str == "jpg")
    {
      return Compression::Jpg;
    }
    else if (str == "png")
    {
      return Compression::Png;
    }
    else if (str == "val")
    {
      return Compression::Value;
    }
    else if (str == "tensor")
    {
      return Compression::Tensor;
    }
    else
    {
      CHECK(false, "Invalid compression string %s", str.c_str());
      return Compression::Unknown;
    }
  }

  static vector<unique_ptr<ITransformerSave>> ParseTransforms(const ChannelsetSaveParameters& channelset_save_params)
  {
    vector<unique_ptr<ITransformerSave>> save_transforms;

    for (int it = 0; it < channelset_save_params.transform_parameter_size(); it++)
    {
      save_transforms.emplace_back(CreateTransformerSave(channelset_save_params.transform_parameter(it)));
    }

    return save_transforms;
  }

private:
  string name_;                                             // Name of the channel.
  int channels_;                                            // Number of channels when loading from disk.
  Compression compression_;                                 // Output compression.
  vector<unique_ptr<ITransformerSave>> save_transformers_;  // Transformers to be applied before saving.
};

// Structure that stores config file header.
struct HeaderDesc
{
public:
  HeaderDesc() = default;

  HeaderDesc(const HeaderDesc& other) = delete;
  HeaderDesc& operator=(const HeaderDesc& other) = delete;

  HeaderDesc(HeaderDesc&& other)
  {
    channelset_descriptors_ = move(other.channelset_descriptors_);
    list_file_path_ = move(other.list_file_path_);
    shuffle_ = other.shuffle_;
  }

  static HeaderDesc ParseHeader(const string& config_file_path)
  {
    DsSaveParameters save_parameters;
    ReadProtoFromTextFile(config_file_path.c_str(), &save_parameters);

    HeaderDesc header_desc;
    header_desc.list_file_path_ = save_parameters.list_file_path();
    header_desc.shuffle_ = save_parameters.shuffle();
    header_desc.channelset_descriptors_.resize(save_parameters.channelset_save_params_size());
    unordered_set<string> channelset_names;
    for (int ic = 0; ic < save_parameters.channelset_save_params_size(); ic++)
    {
      const ChannelsetSaveParameters& channelset_save_params = save_parameters.channelset_save_params(ic);
      HeaderChannelSetDesc& channelset_desc = header_desc.channelset_descriptors_[ic];
      channelset_desc.InitFromParams(channelset_save_params);

      const string& channelset_name = *channelset_desc.GetName();
      // Check that channelset name length is within acceptable limits.
      CHECK(channelset_name.size() + 1 < c_max_name_len, "Channelset name %s is too long.", channelset_name.c_str());
      // Check that channelsets have unique name.
      CHECK(channelset_names.find(channelset_name) == channelset_names.end(),
        "Duplicate channelset name %s", channelset_name.c_str());
      channelset_names.insert(channelset_name);
    }

    return header_desc;
  }

  const string* GetListfilePath() const
  {
    return &list_file_path_;
  }

  bool GetShuffle() const
  {
    return shuffle_;
  }

  int GetChannelsetDescsCount() const
  {
    return static_cast<int>(channelset_descriptors_.size());
  }

  const HeaderChannelSetDesc* GetChannelsetDesc(int ic) const
  {
    return &channelset_descriptors_[ic];
  }

  int GetChannelsetDecodeFlag(int ic)
  {
    if (channelset_descriptors_[ic].GetChannels() == 1)
    {
      return CV_LOAD_IMAGE_GRAYSCALE;
    }
    else if (channelset_descriptors_[ic].GetChannels() == 3)
    {
      return CV_LOAD_IMAGE_COLOR;
    }
    else
    {
      CHECK(false, "Channelset decoding flag %d (number of channels) is invalid (1 or 3 is accepted).",
        channelset_descriptors_[ic].GetChannels());
      return 0;
    }
  }

  const vector<unique_ptr<ITransformerSave>>* GetSaveTransformers(int ic)
  {
    return channelset_descriptors_[ic].GetTransformers();
  }

  const ChannelsetEncodeParams* GetEncodeParams(int ic)
  {
    if (channelset_descriptors_[ic].GetCompression() == Compression::Raw)
    {
      return &c_raw_encode_params;
    }
    else if (channelset_descriptors_[ic].GetCompression() == Compression::Jpg)
    {
      return &c_jpg_encode_params;
    }
    else if (channelset_descriptors_[ic].GetCompression() == Compression::Png)
    {
      return &c_png_encode_params;
    }
    else
    {
      CHECK(false, "Encode params requested for non-image compression.");
      return 0;
    }
  }

private:
  vector<HeaderChannelSetDesc> channelset_descriptors_;
  string list_file_path_;
  bool shuffle_;
};

// Performs parsing of examples from config file. Result is returned as vector of examples where each example is vector
// of channelsets. Channelset is represented as string value which will be interpreted based on output compression
// (path to file or value).
static vector<vector<string>> ParseExamples(const HeaderDesc& header_desc)
{
  vector<vector<string>> examples;

  ifstream in_file_stream(*header_desc.GetListfilePath());
  CHECK(in_file_stream.is_open(), "Failed to open list file %s", header_desc.GetListfilePath()->c_str());

  string line;
  in_file_stream >> line;
  while (!in_file_stream.eof())
  {
    vector<string> new_example;
    for (int ic = 0; ic < header_desc.GetChannelsetDescsCount(); ic++)
    {
      // We must have more input lines here.
      CHECK(!in_file_stream.eof(), "Invalid number of lines (not equal to multiple of channelsets) in list file path %s",
        header_desc.GetListfilePath()->c_str());
      new_example.push_back(line);
      // We stored current line, move to the next line.
      in_file_stream >> line;
    }

    examples.push_back(move(new_example));
  }
  if (header_desc.GetShuffle())
  {
    shuffle(examples.begin(), examples.end(), default_random_engine(0));
  }
  return examples;
}

// Serializes value to output buffer.
// Parameters:
//  [in]  value_string      String representing integer value to be serialized.
//  [out] file_buffer       Byte buffer as vector of bytes.
//  [out] channelset_desc   Channelset descriptor, function will update number of channels and used compression.
//  [out] channelset_inst   Channelset instance, function will update dimensions and byte size.
void ValueSerialize(const string& value_string, vector<unsigned char>& file_buffer, ChannelSet& channelset_desc, ChannelSetInstance& channelset_inst)
{
  // Value is given as string, convert it to integer.
  int value = atoi(value_string.c_str());
  const unsigned char* begin = reinterpret_cast<const unsigned char*>(addressof(value));
  const unsigned char* end = begin + sizeof(int);
  copy(begin, end, back_inserter(file_buffer));

  // Value is 1x1x1 channelset serialized as int.
  channelset_desc.channels = 1;
  channelset_desc.compression = Compression::Value;

  channelset_inst.size = sizeof(int);
  channelset_inst.width = 1;
  channelset_inst.height = 1;
}

size_t ChannelSetInstVectorSerializeToBuffer(const vector<ChannelSetInstance>& channel_inst_vec, vector<unsigned char>& file_buffer)
{
  size_t start_position = file_buffer.size();

  const unsigned char* begin = reinterpret_cast<const unsigned char*>(channel_inst_vec.data());
  const unsigned char* end = begin + sizeof(ChannelSetInstance) * channel_inst_vec.size();

  copy(begin, end, back_inserter(file_buffer));

  return start_position;
}

void ChannelSetInstVectorReplaceInBuffer(const vector<ChannelSetInstance>& channel_inst_vec, const size_t start_position, vector<unsigned char>& file_buffer)
{
  const unsigned char* begin = reinterpret_cast<const unsigned char*>(channel_inst_vec.data());
  const unsigned char* end = begin + sizeof(ChannelSetInstance) * channel_inst_vec.size();

  CHECK(start_position + sizeof(ChannelSetInstance) * channel_inst_vec.size() <= file_buffer.size(),
    "The buffer overrun hapened during call to ChannelSetInstVectorReplaceInBuffer: \
         file_buffer size: %lu start_position: %lu channel_inst_vec size: %lu sizeof(ChannelSetInstance): %lu",
         file_buffer.size(), start_position, channel_inst_vec.size(), sizeof(ChannelSetInstance));

  copy(begin, end, std::begin(file_buffer) + start_position);
}

cv::Mat DecodeImage(const string* image_file_path, int decode_flag)
{
  vector<unsigned char> buffer;

  // Open input file for reading binary.
  FILE* in_file;
  CHECK_ERRNO(Platform::fopen_s(&in_file, image_file_path->c_str(), "rb") == 0,
    "Cannot open image file %s", image_file_path->c_str());
  CHECK(in_file != nullptr, "Cannot open image file %s", image_file_path->c_str());

  // Determine the size of the input file.
  fseek(in_file, 0, SEEK_END);
  long in_file_size = ftell(in_file);
  fseek(in_file, 0, SEEK_SET);

  // Read the contents of the input file into memory buffer.
  buffer.resize(in_file_size);
  CHECK_ERRNO(fread(buffer.data(), sizeof(char), in_file_size, in_file) == in_file_size,
    "Reading image %s for decoding failed", image_file_path->c_str());

  // Decompress image to determine image size.
  cv::Mat encoded_image(1, (int)in_file_size, CV_8UC1, buffer.data());
  cv::Mat decoded_image = cv::imdecode(encoded_image, decode_flag);

  fclose(in_file);
  return decoded_image;
};

// Serializes image to output buffer.
// Parameters:
//  [in]  strict_mode       Strict mode where even a single image read error will throw.
//  [in]  image_file_path   Path to the image to be serialzied.
//  [in]  decode_flag       Flag that instructs how image decoding is performed (essentially gs or color).
//  [in]  transformers_save Array of save transformers to be applied to image before serializing.
//  [in]  encode_params     Defines how image is encoded before serializing.
//  [out]  file_buffer      File buffer.
//  [out]  channelset_desc  Channelset descriptor, function will update number of channels and used compression.
//  [out]  channelset_inst  Channelset instance, function will update dimensions and byte size.
bool ImageSerialize(
  bool strict_mode,
  const string& image_file_path,
  int decode_flag,
  const vector<unique_ptr<ITransformerSave>>* transformers_save,
  const ChannelsetEncodeParams* encode_params,
  vector<unsigned char>& file_buffer,
  ChannelSet& channelset_desc,
  ChannelSetInstance& channelset_inst
  )
{
  vector<unsigned char> recompressed_buffer;

  cv::Mat orig_decoded_image = DecodeImage(&image_file_path, decode_flag);

  if (orig_decoded_image.empty())
  {
    if (!strict_mode)
    {
      WARNING("Decoding of image %s failed\n", image_file_path.c_str());
      return false;
    }
    else
    {
      CHECK(!orig_decoded_image.empty(), "Decoding of image %s failed.", image_file_path.c_str());
    }
  }

  // Store current image of interest.
  cv::Mat* decoded_image = &orig_decoded_image;

  // Apply save transforms.
  cv::Mat transformed_image_in;
  cv::Mat transformed_image_out;
  cv::Mat* transformed_image = &transformed_image_out;
  for (size_t it = 0; it < transformers_save->size(); it++)
  {
    (*transformers_save)[it]->Transform(decoded_image, transformed_image);
    swap(decoded_image, transformed_image);
  }

  CHECK(decoded_image != nullptr, "Decoded image is nullptr after transforms.");
  unsigned char* final_data = nullptr;
  int final_data_size = -1;
  CHECK(encode_params->compression != Compression::Unknown && encode_params->compression != Compression::Value,
    "Serializing image with wrong compression.");
  if (encode_params->compression != Compression::Raw)
  {
    // We need to compress.
    cv::imencode(encode_params->type, *decoded_image, recompressed_buffer, const_cast<vector<int>&>(*encode_params->params));
    final_data_size = static_cast<int>(recompressed_buffer.size());
    final_data = recompressed_buffer.data();
  }
  else
  {
    // Write raw image.
    final_data = decoded_image->data;
    final_data_size = decoded_image->cols * decoded_image->rows * decoded_image->channels();
  }

  channelset_desc.channels = decoded_image->channels();
  channelset_desc.compression = encode_params->compression;

  channelset_inst.size = final_data_size;
  channelset_inst.width = decoded_image->cols;
  channelset_inst.height = decoded_image->rows;

  // Write final data to output file buffer.
  const unsigned char* begin = final_data;
  const unsigned char* end = begin + final_data_size * sizeof(unsigned char);

  copy(begin, end, back_inserter(file_buffer));

  return true;
};

void MakeDataset(const string& config_file_path, const string& out_ds_file_path, bool strict_mode)
{
  HeaderDesc header_desc = HeaderDesc::ParseHeader(config_file_path);

  vector<vector<string>> examples = ParseExamples(header_desc);

  // Open output ids file for writing binary.
  FILE* out_file;
  CHECK_ERRNO(Platform::fopen_s(&out_file, out_ds_file_path.c_str(), "wb") == 0,
    "Cannot open dataset file %s", out_ds_file_path.c_str());
  CHECK(out_file != nullptr, "Cannot open dataset file %s", out_ds_file_path.c_str());

  // Write header.
  DsHeader header;
  fwrite(&header, sizeof(char), sizeof(header), out_file);
  // Write placeholder for channelset descriptors (they will be updated during dataset creation and
  // written to the file at the end).
  vector<ChannelSet> channelsets(header_desc.GetChannelsetDescsCount());
  fwrite(channelsets.data(), sizeof(ChannelSet), channelsets.size(), out_file);

  // We will cache all channelset instances (without memory) at the end of the file to be able to read them in one call
  // and perform chunking and other tasks minimizing disk activity.
  vector<ChannelSetInstance> cached_channelset_insts;

  const size_t num_threads = 16; // Can be exposed as parameters
  // Max number of examples processed by one thread inside batch.
  const size_t load_per_thread = 16; // Can be exposed as parameters
  // Number of examples inside one batch. Batch is a group of consecutive examples processed in multiple threads and serialized before next group of samples is taken.
  const size_t batch_size = num_threads * load_per_thread;
  vector<vector<unsigned char>> file_buffer_big(batch_size);
  vector<vector<ChannelSetInstance>> channelset_insts_big(batch_size);
  vector<bool> valid_images_big(batch_size);
  size_t num_errors_counter = 0;
  // Error counter lock
  mutex errors_counter_lock;

  // Create a worker that can ingest a set of channelsets
  auto worker = [&](size_t start, size_t end, size_t inc)
  {
    for (size_t ie = start; ie < end; ie += inc)
    {
      vector<unsigned char> file_buffer;
      const vector<string>& currExample = examples[ie];

      // Write example channelset size placeholder.
      vector<ChannelSetInstance> channelset_insts(header_desc.GetChannelsetDescsCount());

      // Remember example start offset.
      size_t example_start_offset = ChannelSetInstVectorSerializeToBuffer(channelset_insts, file_buffer);

      bool valid_images = true;

      for (int ics = 0; ics < header_desc.GetChannelsetDescsCount(); ics++)
      {
        ChannelSet channelset;
        const Compression compression = header_desc.GetChannelsetDesc(ics)->GetCompression();
        const string& currExampleCurrChannelset = currExample[ics];
        bool valid_image = true;
        // Perform serialization. Each serialization method fill serialize just value/image/tensor and update channelset and
        // channelset instance (which will not be serialized).
        // This way we can check if channelset descriptions are consistent accross dataset during creation and serialize
        // (actually update, since placeholder is written at the beginning) them at the very end of serialization.
        // Similarly, for channelset instances we will update them once all channelset memory gets serialized.
        if (compression == Compression::Value)
        {
          ValueSerialize(currExampleCurrChannelset, /*out*/file_buffer, /*out*/channelset, /*out*/channelset_insts[ics]);
        }
        else if (compression == Compression::Tensor)
        {
          TensorSerialize(currExampleCurrChannelset, /*out*/file_buffer, /*out*/channelset, /*out*/channelset_insts[ics]);
        }
        else
        {
          valid_image = ImageSerialize(
            strict_mode,
            currExampleCurrChannelset,
            header_desc.GetChannelsetDecodeFlag(ics),
            header_desc.GetSaveTransformers(ics),
            header_desc.GetEncodeParams(ics),
            /*out*/file_buffer,
            /*out*/channelset,
            /*out*/channelset_insts[ics]
            );
        }

        valid_images = valid_images && valid_image;

        if (valid_image)
        {
          // Make sure all channelsest at the same index inside example have same number of channels and compression.
          CHECK(channelsets[ics].channels == -1 || channelsets[ics].channels == channelset.channels,
            "Number of channels in channelset is not consistent across samples %d != %d.", channelsets[ics].channels, channelset.channels);
          CHECK(channelsets[ics].compression == Compression::Unknown || channelsets[ics].compression == channelset.compression,
            "Compression in channelset is not consistent across samples.");
          // Update channelset descriptor.
          channelsets[ics] = channelset;
        }
      }

      // Go back and write channelsets instances.
      // Update the channelset instances that were written at the start
      ChannelSetInstVectorReplaceInBuffer(channelset_insts, example_start_offset, file_buffer);

      if (valid_images)
      {
        channelset_insts_big[ie % batch_size] = move(channelset_insts);
        file_buffer_big[ie % batch_size] = move(file_buffer);
      }
      else
      {
        // Protect this area
        errors_counter_lock.lock();
        num_errors_counter++;
        errors_counter_lock.unlock();
        // End of protection
      }

      valid_images_big[ie % batch_size] = valid_images;
    }
  };

  // Partition the examples for processing
  vector<pair<size_t, size_t>> batches;
  for (size_t ie = 0; ie < examples.size(); ie += batch_size)
  {
    batches.emplace_back(ie, min(ie + batch_size, examples.size()));
  }

  // Create a thread pool
  vector<thread> threads(num_threads);
  // Go through each partition and then serialize the data to file
  for (size_t i = 0; i < batches.size(); i++)
  {
    // Determine maximum threads needed
    size_t required_threads = min(batches[i].second - batches[i].first, num_threads);

    // Schedule threads
    for (size_t thread_counter = 0; thread_counter < required_threads; thread_counter++)
    {
      threads[thread_counter] = thread(worker, batches[i].first + thread_counter, batches[i].second, load_per_thread);
    }

    // Wait for threads to finish
    for (size_t thread_counter = 0; thread_counter < required_threads; thread_counter++)
    {
      threads[thread_counter].join();
    }

    // Write the data for this partition in file and cache the channelset instances for later use
    for (size_t buffer_counter = 0; buffer_counter < batches[i].second - batches[i].first; buffer_counter++)
    {
      const auto& file_buffer = file_buffer_big[buffer_counter];
      const auto& channelset_insts = channelset_insts_big[buffer_counter];
      const auto& valid_images = valid_images_big[buffer_counter];

      if (valid_images)
      {
        fwrite(file_buffer.data(), sizeof(unsigned char), file_buffer.size(), out_file);
        cached_channelset_insts.insert(cached_channelset_insts.end(), channelset_insts.begin(), channelset_insts.end());
      }
    }

    LOG("Consumed %lu examples.", batches[i].second);
  }

  // Write cached channelset instances at the end of file. Remember the start position to be able to update header.
  int64_t cached_instances_start = Platform::ftell64(out_file);
  fwrite(cached_channelset_insts.data(), sizeof(ChannelSetInstance), cached_channelset_insts.size(), out_file);

  // We finished writing samples, now go back and update header.
  header.channelsets_count = header_desc.GetChannelsetDescsCount();
  header.cached_channelset_instances_start_ = cached_instances_start;
  fseek(out_file, 0, SEEK_SET);
  fwrite(&header, sizeof(DsHeader), 1, out_file);
  // Update channelset descriptors as well.
  for (int ic = 0; ic < channelsets.size(); ic++)
  {
    Platform::strcpy_s(channelsets[ic].name, c_max_name_len, header_desc.GetChannelsetDesc(ic)->GetName()->c_str());
  }
  fwrite(channelsets.data(), sizeof(ChannelSet), channelsets.size(), out_file);

  fclose(out_file);

  CHECK(examples.size() >= num_errors_counter, "Number of errors cannot be more than number of examples.");
  LOG("Dataset creation finished successfully: %lu examples in the dataset and %lu errors.\n",
    examples.size() - num_errors_counter, num_errors_counter);
}

// Report to standard output all images specified by config file that cannot be decoded.
// Non-image files are ignored.
void CheckDecoding(const string& config_file_path)
{
  HeaderDesc header_desc = HeaderDesc::ParseHeader(config_file_path);

  vector<vector<string>> examples = ParseExamples(header_desc);

  mutex print_lock;

  // Partition the examples for processing
  const size_t num_threads = 16;
  size_t num_examples = 0;

  // Create a worker that can ingest a set of channelsets
  auto worker = [&](size_t start, size_t end)
  {
    for (size_t ie = start; ie < end; ie++)
    {
      const vector<string>& example = examples[ie];
      for (int ics = 0; ics < header_desc.GetChannelsetDescsCount(); ics++)
      {
        const Compression compression = header_desc.GetChannelsetDesc(ics)->GetCompression();
        if (compression != Compression::Value && compression != Compression::Tensor)
        {
          const string& image_file_path = example[ics];
          const cv::Mat result = DecodeImage(&image_file_path, header_desc.GetChannelsetDecodeFlag(ics));
          print_lock.lock();
          {
            num_examples++;

            if (num_examples % 1000 == 0 || num_examples == examples.size())
            {
              LOG("Consumed %lu examples.\n", num_examples);
            }

            if (result.empty())
            {
              WARNING("Decoding of image %s failed\n", image_file_path.c_str());
            }
          }
          print_lock.unlock();
        }
      }
    }
  };

  // Create a thread pool
  vector<thread> threads(num_threads);
  const size_t batch_size = static_cast<size_t>(ceilf(examples.size() / static_cast<float>(num_threads)));
  size_t start = 0;

  // Schedule threads
  for (size_t thread_counter = 0; thread_counter < num_threads; thread_counter++)
  {
    size_t end = min(start + batch_size, examples.size());

    threads[thread_counter] = thread(worker, start, end);
    start = end;
  }

  // Wait for threads to finish
  for (size_t thread_counter = 0; thread_counter < num_threads; thread_counter++)
  {
    threads[thread_counter].join();
  }
}
