#include "check.hpp"
#include "dataset.hpp"
#include "proto_io.hpp"
#include "platform.hpp"
#include "tensor.hpp"
#include "transformer_save.hpp"

#include "ds_save_params.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <memory>
#include <random>
#include <string>
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

  std::ifstream in_file_stream(*header_desc.GetListfilePath());
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
    std::shuffle(examples.begin(), examples.end(), default_random_engine(0));
  }
  return examples;
}

// Serializes value to output file.
// Parameters:
//  [in]  value_string      String representing integer value to be serialized.
//  [out] out_file          File handle.
//  [out] channelset_desc   Channelset descriptor, function will update number of channels and used compression.
//  [out] channelset_inst   Channelset instance, function will update dimensions and byte size.
void ValueSerialize(const string& value_string, FILE* out_file, ChannelSet& channelset_desc, ChannelSetInstance& channelset_inst)
{
  // Value is given as string, convert it to integer.
  int value = std::atoi(value_string.c_str());
  fwrite(&value, sizeof(int), 1, out_file);

  // Value is 1x1x1 channelset serialized as int.
  channelset_desc.channels = 1;
  channelset_desc.compression = Compression::Value;

  channelset_inst.size = sizeof(int);
  channelset_inst.width = 1;
  channelset_inst.height = 1;
}

cv::Mat DecodeImage(const string* image_file_path, int decode_flag)
{
  // Reuse buffers across method calls.
  static vector<unsigned char> buffer;

  // Open input file for reading binary.
  FILE* in_file;
  CHECK(Platform::fopen_s(&in_file, image_file_path->c_str(), "rb") == 0,
    "Cannot open image file %s", image_file_path->c_str());
  CHECK(in_file != nullptr, "Cannot open image file %s", image_file_path->c_str());

  // Determine the size of the input file.
  fseek(in_file, 0, SEEK_END);
  long in_file_size = ftell(in_file);
  fseek(in_file, 0, SEEK_SET);

  // Read the contents of the input file into memory buffer.
  buffer.resize(in_file_size);
  CHECK(fread(buffer.data(), sizeof(char), in_file_size, in_file) == in_file_size,
    "Reading image %s for decoding failed", image_file_path->c_str());

  // Decompress image to determine image size.
  cv::Mat encoded_image(1, (int)in_file_size, CV_8UC1, buffer.data());
  cv::Mat decoded_image = cv::imdecode(encoded_image, decode_flag);

  fclose(in_file);
  return decoded_image;
};

// Serializes image to output file.
// Parameters:
//  [in]  image_file_path   Path to the image to be serialzied.
//  [in]  decode_flag       Flag that instructs how image decoding is performed (essentially gs or color).
//  [in]  transformers_save Array of save transformers to be applied to image before serializing.
//  [in]  encode_params     Defines how image is encoded before serializing.
//  [out]  out_file         File handle.
//  [out]  channelset_desc  Channelset descriptor, function will update number of channels and used compression.
//  [out]  channelset_inst  Channelset instance, function will update dimensions and byte size.
void ImageSerialize(
  const string& image_file_path,
  int decode_flag,
  const vector<unique_ptr<ITransformerSave>>* transformers_save,
  const ChannelsetEncodeParams* encode_params,
  FILE* out_file,
  ChannelSet& channelset_desc,
  ChannelSetInstance& channelset_inst
  )
{
  // Reuse buffer across method calls.
  static vector<unsigned char> recompressed_buffer;

  cv::Mat orig_decoded_image = DecodeImage(&image_file_path, decode_flag);
  CHECK(!orig_decoded_image.empty(), "Decoding of image %s failed.", image_file_path.c_str());

  // Store current image of interest.
  cv::Mat* decoded_image = &orig_decoded_image;

  // Apply save transforms.
  static cv::Mat transformed_image_in;
  static cv::Mat transformed_image_out;
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

  // Write final data to output file.
  fwrite(final_data, sizeof(char), final_data_size, out_file);
};

void MakeDataset(const string& config_file_path, const string& out_ds_file_path)
{
  HeaderDesc header_desc = HeaderDesc::ParseHeader(config_file_path);

  vector<vector<string>> examples = ParseExamples(header_desc);

  // Open output ids file for writing binary.
  FILE* out_file;
  CHECK(Platform::fopen_s(&out_file, out_ds_file_path.c_str(), "wb") == 0,
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

  // Now go over all examples and serialize them.
  for (size_t ie = 0; ie < examples.size(); ie++)
  {
    const vector<string>& currExample = examples[ie];
    // Remember example start offset.
    int64_t example_start_offset = Platform::ftell64(out_file);
    // Write example channelset size placeholder.
    vector<ChannelSetInstance> channelset_insts(header_desc.GetChannelsetDescsCount());
    fwrite(channelset_insts.data(), sizeof(ChannelSetInstance), channelset_insts.size(), out_file);

    for (int ics = 0; ics < header_desc.GetChannelsetDescsCount(); ics++)
    {
      ChannelSet channelset;
      const Compression compression = header_desc.GetChannelsetDesc(ics)->GetCompression();
      const string& currExampleCurrChannelset = currExample[ics];
      // Perform serialization. Each serialization method fill serialize just value/image/tensor and update channelset and
      // channelset instance (which will not be serialized).
      // This way we can check if channelset descriptions are consistent accross dataset during creation and serialize
      // (actually update, since placeholder is written ant the beginning) them at the very end of serialization.
      // Similarly, for channelset instances we will update them once all channelset memory gets serialized.
      if (compression == Compression::Value)
      {
        ValueSerialize(currExampleCurrChannelset, /*out*/out_file, /*out*/channelset, /*out*/channelset_insts[ics]);
      }
      else if (compression == Compression::Tensor)
      {
        TensorSerialize(currExampleCurrChannelset, /*out*/out_file, channelset, /*out*/channelset_insts[ics]);
      }
      else
      {
        ImageSerialize(
          currExampleCurrChannelset,
          header_desc.GetChannelsetDecodeFlag(ics),
          header_desc.GetSaveTransformers(ics),
          header_desc.GetEncodeParams(ics),
          /*out*/out_file,
          /*out*/channelset,
          /*out*/channelset_insts[ics]
          );
      }
      // Make sure all channelsest at the same index inside example have same number of channels and compression.
      CHECK(channelsets[ics].channels == -1 || channelsets[ics].channels == channelset.channels,
        "Number of channels in channelset is not consistent across samples %d != %d.", channelsets[ics].channels, channelset.channels);
      CHECK(channelsets[ics].compression == Compression::Unknown || channelsets[ics].compression == channelset.compression,
        "Compression in channelset is not consistent across samples.");
      // Update channelset descriptor.
      channelsets[ics] = channelset;
    }

    // Go back and write channelsets instances.
    int64_t example_end_offset = Platform::ftell64(out_file);
    Platform::fseek64(out_file, example_start_offset, SEEK_SET);
    fwrite(channelset_insts.data(), sizeof(ChannelSetInstance), channelset_insts.size(), out_file);

    // Copy serialized channelset instances to the cached array.
    cached_channelset_insts.insert(cached_channelset_insts.end(), channelset_insts.begin(), channelset_insts.end());

    // Go to the end to write next example.
    Platform::fseek64(out_file, example_end_offset, SEEK_SET);
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

  printf("Dataset creation finished successfully.\n");
}

// Report to standard output all images specified by config file that cannot be decoded.
// Non-image files are ignored.
void CheckDecoding(const string& config_file_path)
{
  HeaderDesc header_desc = HeaderDesc::ParseHeader(config_file_path);

  vector<vector<string>> examples = ParseExamples(header_desc);

  for (size_t ie = 0; ie < examples.size(); ie++)
  {
    const vector<string>& example = examples[ie];
    for (int ics = 0; ics < header_desc.GetChannelsetDescsCount(); ics++)
    {
      const Compression compression = header_desc.GetChannelsetDesc(ics)->GetCompression();
      if (compression != Compression::Value && compression != Compression::Tensor)
      {
        const string& image_file_path = example[ics];
        if (DecodeImage(&image_file_path, header_desc.GetChannelsetDecodeFlag(ics)).empty())
        {
          printf("%s\n", image_file_path.c_str());
          fflush(stdout);
        }
      }
    }
  }
}
