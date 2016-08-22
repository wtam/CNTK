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
      CHECK(false);
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

// Structure that stores config file header blob description.
struct HeaderBlobDesc
{
public:
  HeaderBlobDesc() = default;
  HeaderBlobDesc(HeaderBlobDesc&& other)
  {
    name_ = move(other.name_);
    channelset_descriptors_ = move(other.channelset_descriptors_);
  }

  HeaderBlobDesc(const HeaderBlobDesc& other) = delete;

  HeaderBlobDesc& operator=(const HeaderBlobDesc& other) = delete;

  const string* GetName() const
  {
    return &name_;
  }

  int GetChannelsetDescsCount() const
  {
    return static_cast<int>(channelset_descriptors_.size());
  }

  const HeaderChannelSetDesc* GetChannelsetDesc(int ic) const
  {
    return &channelset_descriptors_[ic];
  }

private:
  string name_;
  vector<HeaderChannelSetDesc> channelset_descriptors_;

  // HeaderDesc performs parsing.
  friend struct HeaderDesc;
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
    blob_descriptors_ = move(other.blob_descriptors_);
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
    header_desc.blob_descriptors_.resize(save_parameters.blob_save_params_size());
    for (int ib = 0; ib < save_parameters.blob_save_params_size(); ib++)
    {
      const BlobSaveParameters& blob_save_param = save_parameters.blob_save_params(ib);
      header_desc.blob_descriptors_[ib].name_ = blob_save_param.name();
      CHECK(header_desc.blob_descriptors_[ib].name_.size() + 1 < c_max_name_len);
      header_desc.blob_descriptors_[ib].channelset_descriptors_.resize(blob_save_param.channelset_save_params_size());
      for (int ic = 0; ic < blob_save_param.channelset_save_params_size(); ic++)
      {
        const ChannelsetSaveParameters& channelset_save_params = blob_save_param.channelset_save_params(ic);
        HeaderChannelSetDesc& channelset_desc = header_desc.blob_descriptors_[ib].channelset_descriptors_[ic];
        channelset_desc.InitFromParams(channelset_save_params);

        CHECK(channelset_desc.GetName()->size() + 1 < c_max_name_len);
      }
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

  int GetBlobDescsCount() const
  {
    return static_cast<int>(blob_descriptors_.size());
  }

  const HeaderBlobDesc* GetBlobDesc(int ib) const
  {
    return &blob_descriptors_[ib];
  }

  int GetChannelsetDecodeFlag(int ib, int ic)
  {
    if (blob_descriptors_[ib].channelset_descriptors_[ic].GetChannels() == 1)
    {
      return CV_LOAD_IMAGE_GRAYSCALE;
    }
    else if (blob_descriptors_[ib].channelset_descriptors_[ic].GetChannels() == 3)
    {
      return CV_LOAD_IMAGE_COLOR;
    }
    else
    {
      CHECK(false);
      return 0;
    }
  }

  const vector<unique_ptr<ITransformerSave>>* GetSaveTransformers(int ib, int ic)
  {
      return blob_descriptors_[ib].channelset_descriptors_[ic].GetTransformers();
  }

  const ChannelsetEncodeParams* GetEncodeParams(int ib, int ic)
  {
    if (blob_descriptors_[ib].channelset_descriptors_[ic].GetCompression() == Compression::Raw)
    {
      return &c_raw_encode_params;
    }
    else if (blob_descriptors_[ib].channelset_descriptors_[ic].GetCompression() == Compression::Jpg)
    {
      return &c_jpg_encode_params;
    }
    else if (blob_descriptors_[ib].channelset_descriptors_[ic].GetCompression() == Compression::Png)
    {
      return &c_png_encode_params;
    }
    else
    {
      CHECK(false);
      return 0;
    }
  }

private:
  vector<HeaderBlobDesc> blob_descriptors_;
  string list_file_path_;
  bool shuffle_;
};

// Config file descriptor for one blob.
struct ExampleBlobDesc
{
public:
  enum class Type { Unknown, Value, Path };

  ExampleBlobDesc(Type t) : type(t)
  {
  }

  void AddPath(const string& path)
  {
    CHECK(type == ExampleBlobDesc::Type::Path);
    paths.push_back(path);
  }

  void AddValue(int value)
  {
    CHECK(type == ExampleBlobDesc::Type::Value);
    CHECK(values.size() == 0);
    values.push_back(value);
  }

  int GetChannelSetsCount() const
  {
    if (type == ExampleBlobDesc::Type::Path)
    {
      return static_cast<int>(paths.size());
    }
    else if (type == ExampleBlobDesc::Type::Value)
    {
      return 1;
    }
    else
    {
      CHECK(false);
      return 0;
    }
  }

  const string* GetPath(int i) const
  {
    CHECK(type == ExampleBlobDesc::Type::Path);
    return &paths[i];
  }

  int GetValue() const
  {
    CHECK(type == ExampleBlobDesc::Type::Value);
    return values[0];
  }

  Type GetType() const
  {
    return type;
  }

private:
  Type type;
  vector<string> paths;
  vector<int> values;
};

// Config file descriptor for one example.
struct ExampleDesc
{
public:
  ExampleDesc()
  {
  }

  ExampleDesc(ExampleDesc&& other)
  {
    blob_descs = move(other.blob_descs);
  }

  ExampleDesc& operator=(ExampleDesc&& other)
  {
    blob_descs = move(other.blob_descs);

    return *this;
  }

  ExampleDesc(const ExampleDesc&) = delete;

  ExampleDesc& operator=(const ExampleDesc&) = delete;

  void StartBlob(ExampleBlobDesc::Type type)
  {
    blob_descs.emplace_back(type);
  }

  void AddPathToCurrentBlob(const string& path)
  {
    blob_descs.back().AddPath(path);
  }

  void AddValueToCurrentBlob(int value)
  {
    blob_descs.back().AddValue(value);
  }

  int GetBlobsDescsCount() const
  {
    return static_cast<int>(blob_descs.size());
  }

  const ExampleBlobDesc* GetBlobDesc(int i) const
  {
    return &blob_descs[i];
  }

private:
  vector<ExampleBlobDesc> blob_descs;
};

// Stores all examples from config file and performs parsing.
struct ExamplesDesc
{
public:
  static ExamplesDesc ParseExamples(const HeaderDesc& header_desc)
  {
    ExamplesDesc examples_desc;

    std::ifstream in_file_stream(*header_desc.GetListfilePath());
    CHECK(in_file_stream.is_open(), "Failed to open " + *header_desc.GetListfilePath());

    string line;
    in_file_stream >> line;
    while (!in_file_stream.eof())
    {
      ExampleDesc example_desc;
      for (int ib = 0; ib < header_desc.GetBlobDescsCount(); ib++)
      {
        const HeaderBlobDesc* blob_desc = header_desc.GetBlobDesc(ib);
        if (blob_desc->GetChannelsetDesc(0)->GetCompression() == Compression::Value)
        {
          example_desc.StartBlob(ExampleBlobDesc::Type::Value);
        }
        else
        {
          example_desc.StartBlob(ExampleBlobDesc::Type::Path);
        }

        for (int ic = 0; ic < blob_desc->GetChannelsetDescsCount(); ic++)
        {
          // We must have more input lines here.
          CHECK(!in_file_stream.eof());
          const HeaderChannelSetDesc* channelset_desc = blob_desc->GetChannelsetDesc(ic);

          if (channelset_desc->GetCompression() == Compression::Value)
          {
            // Convert to int and store.
            int value = std::atoi(line.c_str());
            example_desc.AddValueToCurrentBlob(value);
          }
          else
          {
            example_desc.AddPathToCurrentBlob(line);
          }
          // We stored current line, move to the next line.
          in_file_stream >> line;
        }
      }
      examples_desc.examples_.emplace_back(std::move(example_desc));
    }
    if (header_desc.GetShuffle())
    {
      std::shuffle(examples_desc.examples_.begin(), examples_desc.examples_.end(), default_random_engine(0));
    }
    return examples_desc;
  }

  ExamplesDesc(ExamplesDesc&& other)
  {
    examples_ = move(other.examples_);
  }

  int GetExampleDescsCount() const
  {
    return static_cast<int>(examples_.size());
  }

  const ExampleDesc* GetExampleDesc(int ie) const
  {
    return &examples_[ie];
  }

private:
  ExamplesDesc() {}

  vector<ExampleDesc> examples_;
};

// Serializes value to output file.
void ValueSerialize(FILE* out_file, int value, ChannelSet& channelset_desc, ChannelSetInstance& channelset_inst)
{
  fwrite(&value, sizeof(int), 1, out_file);

  // Value is 1x1x1 blob serialized as int.
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
    "Cannot open image file " + *image_file_path);
  CHECK(in_file != nullptr, "Cannot open image file " + *image_file_path);

  // Determine the size of the input file.
  fseek(in_file, 0, SEEK_END);
  long in_file_size = ftell(in_file);
  fseek(in_file, 0, SEEK_SET);

  // Read the contents of the input file into memory buffer.
  buffer.resize(in_file_size);
  fread(buffer.data(), sizeof(char), in_file_size, in_file);

  // Decompress image to determine image size.
  cv::Mat encoded_image(1, (int)in_file_size, CV_8UC1, buffer.data());
  cv::Mat decoded_image = cv::imdecode(encoded_image, decode_flag);

  fclose(in_file);
  return decoded_image;
};

// Serializes image to output file.
void ImageSerialize(
  FILE* out_file,
  const string* image_file_path,
  int decode_flag,
  const vector<unique_ptr<ITransformerSave>>* transformers_save,
  const ChannelsetEncodeParams* encode_params,
  ChannelSet& channelset_desc,
  ChannelSetInstance& channelset_inst
  )
{
  // Reuse buffer across method calls.
  static vector<unsigned char> recompressed_buffer;

  cv::Mat orig_decoded_image = DecodeImage(image_file_path, decode_flag);
  CHECK(!orig_decoded_image.empty(), "Decoding of image " + *image_file_path + " failed.");

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

  CHECK(decoded_image != nullptr);
  unsigned char* final_data = nullptr;
  int final_data_size = -1;
  CHECK(encode_params->compression != Compression::Unknown && encode_params->compression != Compression::Value);
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

  ExamplesDesc examples_desc = ExamplesDesc::ParseExamples(header_desc);

  // Open output ids file for writing binary.
  FILE* out_file;
  CHECK(Platform::fopen_s(&out_file, out_ds_file_path.c_str(), "wb") == 0,
    "Cannot open dataset file " + out_ds_file_path);
  CHECK(out_file != nullptr, "Cannot open dataset file " + out_ds_file_path);

  // Write header.
  DsHeader header;
  fwrite(&header, sizeof(char), sizeof(header), out_file);
  // Write channelset decriptors.
  vector<Blob> blobs(header_desc.GetBlobDescsCount());
  vector<vector<ChannelSet>> channelsets(header_desc.GetBlobDescsCount());
  for (int ib = 0; ib < header_desc.GetBlobDescsCount(); ib++) {
    const HeaderBlobDesc* blob_desc = header_desc.GetBlobDesc(ib);
    fwrite(&blobs[ib], sizeof(Blob), 1, out_file);
    channelsets[ib].resize(blob_desc->GetChannelsetDescsCount());
    fwrite(channelsets[ib].data(), sizeof(ChannelSet), channelsets[ib].size(), out_file);
  }

  // Prepare channelset sizes vector;
  int total_channelsets_count = 0;
  for (int ib = 0; ib < header_desc.GetBlobDescsCount(); ib++)
  {
    const HeaderBlobDesc* blob_desc = header_desc.GetBlobDesc(ib);
    total_channelsets_count += blob_desc->GetChannelsetDescsCount();
  }

  // We will cache all channelset instances (without memory) at the end of the file to be able to read them in one call
  // and perform chunking and other tasks minimizing disk activity.
  vector<ChannelSetInstance> cached_channelset_insts;

  // Now go over all examples and serialize them.
  for (int ie = 0; ie < examples_desc.GetExampleDescsCount(); ie++)
  {
    const ExampleDesc& exampleDescNew = *examples_desc.GetExampleDesc(ie);
    // Remember example start offset.
    int64_t example_start_offset = Platform::ftell64(out_file);
    // Write example channelset size placeholder.
    vector<ChannelSetInstance> channelset_insts(total_channelsets_count);
    fwrite(channelset_insts.data(), sizeof(ChannelSetInstance), channelset_insts.size(), out_file);

    int curr_channelset = 0;
    for (int ib = 0; ib < exampleDescNew.GetBlobsDescsCount(); ib++)
    {
      const ExampleBlobDesc* blob_desc = exampleDescNew.GetBlobDesc(ib);
      int blob_width = -1;
      int blob_height = -1;
      for (int ics = 0; ics < blob_desc->GetChannelSetsCount(); ics++)
      {
        ChannelSet channelset;
        const Compression compression = header_desc.GetBlobDesc(ib)->GetChannelsetDesc(ics)->GetCompression();
        if (compression == Compression::Value)
        {
          CHECK(blob_desc->GetType() == ExampleBlobDesc::Type::Value);
          ValueSerialize(out_file, blob_desc->GetValue(), channelset, channelset_insts[curr_channelset]);
        }
        else if (compression == Compression::Tensor)
        {
          TensorSerialize(*blob_desc->GetPath(ics), out_file, channelset, channelset_insts[curr_channelset]);
        }
        else
        {
          ImageSerialize(
            out_file,
            blob_desc->GetPath(ics),
            header_desc.GetChannelsetDecodeFlag(ib, ics),
            header_desc.GetSaveTransformers(ib, ics),
            header_desc.GetEncodeParams(ib, ics),
            channelset,
            channelset_insts[curr_channelset]
            );
        }
        // Make sure all channelsest at the same index inside example have same number of channels and compression.
        CHECK(channelsets[ib][ics].channels == -1 || channelsets[ib][ics].channels == channelset.channels);
        CHECK(channelsets[ib][ics].compression == Compression::Unknown || channelsets[ib][ics].compression == channelset.compression);
        channelsets[ib][ics] = channelset;

        // Make sure all channelsets inside blob have same dimensions.
        CHECK(blob_width == -1 || blob_width == channelset_insts[curr_channelset].width);
        CHECK(blob_height == -1 || blob_height == channelset_insts[curr_channelset].height);
        blob_width = channelset_insts[curr_channelset].width;
        blob_height = channelset_insts[curr_channelset].height;

        curr_channelset++;
      }
      // Make sure all blobs have same number of channelsets.
      CHECK(blobs[ib].channelsets_count == -1 || blobs[ib].channelsets_count == blob_desc->GetChannelSetsCount());
      blobs[ib].channelsets_count = blob_desc->GetChannelSetsCount();
    }

    // Go back and write channelsets descriptor.
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
  header.blobs_count_ = header_desc.GetBlobDescsCount();
  header.cached_channelset_instances_start_ = cached_instances_start;
  fseek(out_file, 0, SEEK_SET);
  fwrite(&header, sizeof(DsHeader), 1, out_file);
  // Update blobs descriptors as well.
  for (int ib = 0; ib < header_desc.GetBlobDescsCount(); ib++) {
    const HeaderBlobDesc* header_blob_desc = header_desc.GetBlobDesc(ib);
    Platform::strcpy_s(blobs[ib].name, c_max_name_len, header_blob_desc->GetName()->c_str());
    fwrite(&blobs[ib], sizeof(Blob), 1, out_file);
    for (int ic = 0; ic < channelsets[ib].size(); ic++)
    {
        Platform::strcpy_s(channelsets[ib][ic].name, c_max_name_len, header_blob_desc->GetChannelsetDesc(ic)->GetName()->c_str());
    }
    fwrite(channelsets[ib].data(), sizeof(ChannelSet), channelsets[ib].size(), out_file);
  }

  fclose(out_file);

  printf("Dataset creation finished successfully.\n");
}

// Report to standard output all images specified by config file that cannot be decoded.
// Non-image files are ignored.
void CheckDecoding(const string& config_file_path)
{
  HeaderDesc header_desc = HeaderDesc::ParseHeader(config_file_path);

  ExamplesDesc examples_desc = ExamplesDesc::ParseExamples(header_desc);

  for (int ie = 0; ie < examples_desc.GetExampleDescsCount(); ie++)
  {
    const ExampleDesc& exampleDescNew = *examples_desc.GetExampleDesc(ie);
    for (int ib = 0; ib < exampleDescNew.GetBlobsDescsCount(); ib++)
    {
      const ExampleBlobDesc* blob_desc = exampleDescNew.GetBlobDesc(ib);
      for (int ics = 0; ics < blob_desc->GetChannelSetsCount(); ics++)
      {
        const Compression compression
          = header_desc.GetBlobDesc(ib)->GetChannelsetDesc(ics)->GetCompression();
        if (compression != Compression::Value && compression != Compression::Tensor)
        {
          const string* image_file_path = blob_desc->GetPath(ics);
          if (DecodeImage(image_file_path, header_desc.GetChannelsetDecodeFlag(ib, ics)).empty())
          {
            printf("%s\n", image_file_path->c_str());
            fflush(stdout);
          }
        }
      }
    }
  }
}
