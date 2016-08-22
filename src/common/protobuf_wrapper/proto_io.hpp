#pragma once

namespace google {
  namespace protobuf {
    class Message;
  }
}

// Parses given prototxt file.
void ReadProtoFromTextFile(const char* filename, google::protobuf::Message* proto);