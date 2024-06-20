#include "json-parsing.h"

#include <vector>

void print(Token t)
{
    std::cout << "[";
    if (t.value != nullptr)
    {
        for (size_t i = 0; i < t.length; i++)
        {
            std::cout << t.value[i];
        }
    }

    std::string typeName = token_type_to_string(t.type);

    if (t.value)
    {
        std::cout << ": " << typeName << "] ";
    }
    else
    {
        std::cout << typeName << "] ";
    }
}
struct Image
{
    std::string url;
    int x, y;
};
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
    Image image;
    std::vector<Comment> comments;
};

OBJECT_PARSER(Image, FIELD_PARSER(url) FIELD_PARSER(x) FIELD_PARSER(y));

OBJECT_PARSER(Comment, FIELD_PARSER(author) FIELD_PARSER(content) FIELD_PARSER(timestamp));

OBJECT_PARSER(BlogPost, FIELD_PARSER(title) FIELD_PARSER(author) FIELD_PARSER(content) FIELD_PARSER(timestamp) FIELD_PARSER(image) FIELD_PARSER_ARRAY(comments));

int main()
{
    std::string json = R"(
        {
            "author": "Jane Doe",
            "image": {
                "url": "https://www.google.com",
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
    std::vector<Token> tokens{};

    tokenize(json.begin(), json.end(), std::back_inserter(tokens));
    for (auto t : tokens)
    {
        print(t);
    }

    BlogPost post{};
    parse_tokenstream(tokens.begin(), tokens.end(), post);
}