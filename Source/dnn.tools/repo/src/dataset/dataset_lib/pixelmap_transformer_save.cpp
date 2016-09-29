#include "pixelmap_transformer_save.hpp"
#include "check.hpp"

#include "ds_save_params.hpp"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <string>
#include <fstream>

using namespace std;

// Helper method that converts rgb triplet into integer by placing channel values into different bytes within integer.
static unsigned int BGRtoInt(unsigned char b, unsigned char g, unsigned char r)
{
  return ((256 * b) + g) * 256 + r;
}

// Returns tokens in the delimited string. If delim = ',' then for input
// "first,second,third" output is {"first","second","third"}.
static vector<string> GetTokens(const string &s, char delim)
{
  vector<string> tokens;

  stringstream ss(s);
  string item;
  while (getline(ss, item, delim))
  {
    tokens.push_back(item);
  }

  return tokens;
}

// Get lines from a file.
static vector<string> GetLines(const string& filename)
{
  ifstream fin(filename);
  vector<string> lines;

  CHECK(fin.is_open(), "Input file is not opened in GetLines.");

  string line;

  while (getline(fin, line))
  {
    lines.push_back(line);
  }

  return lines;
}

// returns the RGB -> grayscale mapping form the given file.
static unordered_map<unsigned int, unsigned char> GetMapping(const string& file)
{
  unordered_map<unsigned int, unsigned char> mapping;

  // Take lines from the file.
  vector<string> lines = GetLines(file);

  // Go over lines.
  for (const auto& line : lines)
  {
    // Tokenize line.
    vector<string> tokens = GetTokens(line, ',');
    // We expect exactly 5 comma separated values:
    // class_string, out_value, in_red, in_green, in_blue
    CHECK(tokens.size() == 5, "Invalid token count %u (5 expected)", tokens.size());
    // We are not interested in class string.
    int outInt = std::atoi(tokens[1].c_str());
    int redInt = std::atoi(tokens[2].c_str());
    int greenInt = std::atoi(tokens[3].c_str());
    int blueInt = std::atoi(tokens[4].c_str());
    // Ensure values are within expected range and cast to unsigend char.
    CHECK(outInt <= std::numeric_limits<unsigned char>::max(), "Invalid output value %d in GetMapping: ", outInt);
    CHECK(redInt <= std::numeric_limits<unsigned char>::max(), "Invalid red value %d in GetMapping: ", redInt);
    CHECK(greenInt <= std::numeric_limits<unsigned char>::max(), "Invalid green value %d in GetMapping: ", greenInt);
    CHECK(blueInt <= std::numeric_limits<unsigned char>::max(), "Invalid blue value %d in GetMapping: ", blueInt);
    unsigned char out = static_cast<unsigned char>(outInt);
    unsigned char blue = static_cast<unsigned char>(blueInt);
    unsigned char green = static_cast<unsigned char>(greenInt);
    unsigned char red = static_cast<unsigned char>(redInt);

    // Save mapping.
    mapping[BGRtoInt(blue, green, red)] = out;
  }

  return mapping;
}

PixelMapTransformerSave::PixelMapTransformerSave(const TransformParameterSave& param)
{
  CHECK(param.type() == PixelMapTransformerSave::GetTypeString(), "Invalid type in PixelMapTransformerSave: %s", param.type().c_str());
  CHECK(param.has_pixel_map_param(), "Pixel map parameter is missing in PixelMapTransformerSave.");
  string mapping_file = param.pixel_map_param().mapping_file();
  // Parse mapping file and save pixel map.
  rgb_to_gs_map_ = GetMapping(mapping_file);
}

void PixelMapTransformerSave::Transform(const cv::Mat* in_image, cv::Mat* out_image)
{
  CHECK(in_image->channels() == 3,
    "Input image to PixelMapTransformerSave::Transform must have 3 channels, %d provided: ", in_image->channels());
  out_image->create(in_image->rows, in_image->cols, CV_8UC(1));

  // Go over pixels and for each RGB triplet save mapped value in output image.
  for (int h = 0; h < in_image->rows; h++)
  {
    auto in_image_ptr = in_image->ptr(h);
    auto out_image_ptr = out_image->ptr(h);
    for (int color = 0, w = 0; w < in_image->cols; color += in_image->channels(), w++)
    {
      unsigned int rgb = BGRtoInt(in_image_ptr[color], in_image_ptr[color + 1], in_image_ptr[color + 2]);
      out_image_ptr[w] = rgb_to_gs_map_.at(rgb);
    }
  }
}