#include <charconv>
#include <iostream>
#include <span>
#include <string>
#include <vector>

#include "json-parsing.h"

struct Image
{
  uint8_t colourFormat;
  virtual uint32_t getAveragePixelValue() = 0;
};

struct StoredImage : public Image
{
  std::vector<uint8_t> data;
  uint16_t x, y;

  uint32_t getAveragePixelValue() override
  {
    uint32_t sum = 0;
    for (uint8_t pixel : data)
    {
      sum += pixel;
    }
    return sum / data.size();
  }
};

JSON(StoredImage, FIELDS(data, x, y));

template <typename Dimension_T>
struct RemoteImage : public Image
{
  std::string url;
  Dimension_T x, y;

  uint32_t getAveragePixelValue() override { return 0x99999999; }
};

template <typename Dimension_T>
PARTIALLY_SPECIALIZED_JSON(RemoteImage<Dimension_T>)
TEMPLATED_JSON(typename Dimension_T, RemoteImage<Dimension_T>, FIELDS(url, x, y));
JSON(Image *, POINTER_FIELDS(colourFormat), SUBTYPES(StoredImage, RemoteImage<uint16_t>));

struct Comment
{
  std::string author;
  std::string content;
  uint64_t timestamp;
};
struct BlogPost
{
  std::string title;
  std::string author;
  std::string content;
  uint64_t timestamp;
  Image *image;
  std::vector<Comment> comments;
};

JSON(Comment, FIELDS(author, content, timestamp))
JSON(BlogPost, FIELDS(title, author, content, timestamp, image, comments));

inline constexpr void line_break(std::vector<char> &json, int indent)
{
  json.push_back('\n');
  for (int i = 0; i < indent; i++)
  {
    json.push_back('\t');
  }
}

inline std::vector<char> prettify_json(std::vector<char> const &json)
{
  std::vector<char> pretty_json{};
  int indent = 0;
  for (int i = 0; i < json.size(); i++)
  {
    char c = json[i];
    if (c == '}' || c == ']')
    {
      line_break(pretty_json, --indent);
      pretty_json.push_back(c);
      if (i < json.size() - 1 && json[i + 1] != ',')
        line_break(pretty_json, indent);
      continue;
    }

    pretty_json.push_back(c);
    if (c == '{' || c == '[')
    {
      line_break(pretty_json, ++indent);
    }
    else if (c == ',')
    {
      line_break(pretty_json, indent);
    }
  }
  return pretty_json;
}

int main()
{
  std::string json_string = R"(
        {
            "author": "Jane Doe",
            "image": {

                "RemoteImage<uint16_t>": {
                    "url": "https://google.com",
                    "x": 800,
                    "y": 600
                }
            },
            "content": "This is a blog post",
            "timestamp": 1234567890,
            "title": "My first blog post",
            "comments": [
                {
                    "author": "John Doe",
                    "content": "This is a comment",
                    "timestamp": 1234567900
                },
                {
                    "author": "Jane Doe",
                    "content": "This is another comment",
                    "timestamp": 1234567941
                }
            ]
            })";
  BlogPost post = json<BlogPost>::deserialize<std::string>(json_string);

  std::vector<char> out_json{};
  json<BlogPost>::serialize(post, std::back_inserter(out_json));
  out_json = prettify_json(out_json);

  std::string out_json_str(out_json.begin(), out_json.end());

  std::cout << out_json_str << std::endl;

  StoredImage stored_image{};
  stored_image.data = {0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00};
  stored_image.x = 2;
  stored_image.y = 3;

  BlogPost post2{.title = "My second blog post",
                 .author = "John Doe",
                 .content = "This is a blog post with a stored image",
                 .timestamp = 123456800,
                 .image = &stored_image,
                 .comments = {Comment{.author = "John Doe", .content = "I also made a post!", .timestamp = 1234567900},
                              Comment{.author = "Jane Doe",
                                      .content = "This is the best post ever!\nEdit: Ooops, forgot to switch accounts",
                                      .timestamp = 1234567941}}};

  out_json = {};
  json<BlogPost>::serialize(post2, std::back_inserter(out_json));
  out_json = prettify_json(out_json);

  std::cout << std::string(out_json.begin(), out_json.end()) << std::endl;

  BlogPost parsedPost2 = json<BlogPost>::deserialize<std::vector<char>>(out_json);

  out_json = {};
  json<BlogPost>::serialize(parsedPost2, std::back_inserter(out_json));

  out_json = prettify_json(out_json);
  std::cout << std::string(out_json.begin(), out_json.end()) << std::endl;

  return 0;
}