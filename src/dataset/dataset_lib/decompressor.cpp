#include "decompressor.hpp"
#include "dataset.hpp"
#include "check.hpp"
#include "tensor.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

// Performs actual decompression of compressed images.
void DecompressCompressed(const char* in, int in_size, int out_channels, int out_height, int out_width, float* out, float* work)
{
  cv::Mat encodedImage(1, in_size, CV_8UC1, (void*)in);
  cv::Mat decodedImage(out_height, out_width, CV_8UC(out_channels), work);
  cv::imdecode(encodedImage, cv::IMREAD_UNCHANGED, &decodedImage);

  cv::Mat decodedImageF(out_height, out_width, CV_32FC(out_channels), out);
  decodedImage.convertTo(decodedImageF, CV_32FC3);
}

// Unpacks raw (uncompressed) image.
void DecompressRaw(const char* in, int in_size, int out_channels, int out_height, int out_width, float* out, float* /*work*/)
{
  CHECK(in_size == out_height * out_width * out_channels,
    "In size differs from out dimensions in raw decompression. InSize=%d Height=%d Width=%d Channels=%d",
    in_size, out_height, out_width, out_channels);
  cv::Mat in_image(out_height, out_width, CV_8UC(out_channels), (void*)in);
  cv::Mat out_image(out_height, out_width, CV_32FC(out_channels), out);
  in_image.convertTo(out_image, CV_32FC3);
}

// Unpacks values based input.
void DecompressValue(const char* in, int in_size, int out_channels, int out_height, int out_width, float* out, float* /*work*/)
{
  CHECK(in_size == sizeof(int), "Input size %d for value decompress is not equal to sizeof(int).", in_size);
  CHECK(out_channels == 1, "Out channels %d for value decompress is not equal to 1.", out_channels);
  CHECK(out_height == 1, "Out height %d for value decompress is not equal to 1.", out_height);
  CHECK(out_width == 1, "Out width %d for value decompress is not equal to 1.", out_width);
  // Input is int convert it to float.
  out[0] = static_cast<float>(reinterpret_cast<const int*>(in)[0]);
}

// Performs decompression of the input image and stores result in out buffer.
void Decompress(const char* in, int in_size, int out_channels, int out_height, int out_width, float* out, float* work, Compression compression)
{
  switch (compression)
  {
  case Compression::Jpg:
    DecompressCompressed(in, in_size, out_channels, out_height, out_width, out, work);
    break;
  case Compression::Tensor:
    DecompressTensor(in, in_size, out_channels, out_height, out_width, out);
    break;
  case Compression::Png:
    DecompressCompressed(in, in_size, out_channels, out_height, out_width, out, work);
    break;
  case Compression::Raw:
    DecompressRaw(in, in_size, out_channels, out_height, out_width, out, work);
    break;
  case Compression::Value:
    DecompressValue(in, in_size, out_channels, out_height, out_width, out, work);
    break;
  }
}
