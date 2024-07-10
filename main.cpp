#include <vector>
#include <string>
#include <iostream>
#include <charconv>
#include <span>

#include "json-parsing.h"

template <typename Dimension_T>
struct Image
{
    std::string url;
    Dimension_T x, y;
};

template <typename Dimension_T>
PARTIALLY_SPECIALIZED_JSON(Image<Dimension_T>)
TEMPLATED_OBJECT_PARSER(typename Dimension_T, Image<Dimension_T>, FIELD_PARSER(url) FIELD_PARSER(x) FIELD_PARSER(y));
TEMPLATED_OBJECT_SERIALIZER(typename Dimension_T, Image<Dimension_T>, FIELD_SERIALIZER(url) FIELD_SERIALIZER(x) FIELD_SERIALIZER(y));

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
    Image<uint16_t> image;
    std::vector<Comment> comments;
};

OBJECT_PARSER(Comment, FIELD_PARSER(author) FIELD_PARSER(content) FIELD_PARSER(timestamp));

OBJECT_PARSER(BlogPost, FIELD_PARSER(title) FIELD_PARSER(author) FIELD_PARSER(content) FIELD_PARSER(timestamp) FIELD_PARSER(image) FIELD_PARSER(comments));

OBJECT_SERIALIZER(Comment, FIELD_SERIALIZER(author) FIELD_SERIALIZER(content) FIELD_SERIALIZER(timestamp));
OBJECT_SERIALIZER(BlogPost, FIELD_SERIALIZER(title) FIELD_SERIALIZER(author) FIELD_SERIALIZER(content) FIELD_SERIALIZER(timestamp) FIELD_SERIALIZER(image) FIELD_SERIALIZER(comments));

inline constexpr void line_break(std::vector<char> &json, int indent)
{
    json.push_back('\n');
    for (int i = 0; i < indent; i++)
    {
        json.push_back('\t');
    }
}

inline constexpr std::vector<char> prettify_json(std::vector<char> json)
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
                "url": "https://google.com",
                "x": 800,
                "y": 600
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
}