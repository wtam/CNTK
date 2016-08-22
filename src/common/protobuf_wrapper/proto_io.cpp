#pragma warning(push, 0)

#include "proto_io.hpp"
#include "check.hpp"

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>

#ifdef _MSC_VER
#include <io.h>
#else
#include <unistd.h>
#endif
#include <fcntl.h>

#include <string>
#include <memory>

using google::protobuf::Message;
using google::protobuf::io::FileInputStream;

void ReadProtoFromTextFile(const char* filename, Message* proto) {
  int fd = open(filename, O_RDONLY);
  CHECK(fd != -1, std::string("File not found: ") + filename);
  std::unique_ptr<FileInputStream> input(new FileInputStream(fd));
  CHECK(google::protobuf::TextFormat::Parse(input.get(), proto));
  close(fd);
}

#pragma warning(pop)
