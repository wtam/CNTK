// Declarations of the objects that are serialized.
#pragma once

#include <cstdint>
#include <cstring>

const int c_ids_file_version = 3;

enum class Compression : int {
  Unknown = -1, // Used internally, should never be serialized.
  Jpg,          // Used when channelset is byte array which is jpeg compressed image.
  Png,          // Used when channelset is byte array which is png compressed image.
  Raw,          // Used when channelset is byte array which is raw (uncompressed) image.
  Value,        // Used when channelset is single integer value.
  Tensor,       // Used when channelset is a 3D tensor.
};

const int c_max_name_len = 16;

// Describes one particular instance of channelset.
struct ChannelSetInstance
{
  int size;   // Size of the serialized channelset in bytes.
  int width;  // Width of the channelset in pixels.
  int height; // Height of the channelset in pixels.
};

// Contains information shared across all channelsets.
struct ChannelSet
{
  ChannelSet() : channels(-1), compression(Compression::Unknown) { memset(name, 0, c_max_name_len); }

  // Number of channels per channelset.
  int channels;
  // Type of channelset compression.
  Compression compression;
  // Name of the channelset.
  char name[c_max_name_len];
};

// Stores information about serialized header.
struct DsHeader
{
  DsHeader() : ids_file_version_(c_ids_file_version), channelsets_count(0) {}

  // Version of ids file, ensures file and reader version match.
  // !!! Important: As new versions are produced this must remain first field in DsHeader to enable version check.
  int ids_file_version_;
  // Number of channelsets per example in dataset file.
  int channelsets_count;
  // File offset where cached channelset instances (without memory) start.
  int64_t cached_channelset_instances_start_;
};
