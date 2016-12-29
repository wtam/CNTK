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
using google::protobuf::io::FileOutputStream;

void ReadProtoFromTextFile(const char* filename, Message* proto) {
  int fd = open(filename, O_RDONLY);
  CHECK(fd != -1, "Proto file not found: %s", filename);
  std::unique_ptr<FileInputStream> input(new FileInputStream(fd));
  CHECK(google::protobuf::TextFormat::Parse(input.get(), proto),
    "Parsing proto file %s failed.", filename);
  close(fd);
}

void WriteProtoToTextFile(const Message& proto, const std::string& filename) {
    int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    {
        std::unique_ptr<FileOutputStream> output(new FileOutputStream(fd));
        CHECK(google::protobuf::TextFormat::Print(proto, output.get()),
            "Writing proto file %s failed.", filename.c_str());
    }
    close(fd);
}

#pragma warning(pop)
