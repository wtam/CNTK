#pragma once

#include <string>

namespace google {
  namespace protobuf {
    class Message;
  }
}

// Parses given prototxt file.
void ReadProtoFromTextFile(const char* filename, google::protobuf::Message* proto);

// Writes proto to given txt file.
void WriteProtoToTextFile(const google::protobuf::Message& proto, const std::string& filename);