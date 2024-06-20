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

template <typename TokenStream>
inline constexpr TokenStream parse_tokenstream(TokenStream begin, TokenStream end, Image &image)
{
    if (begin->type == Token::Type::LBrace)
    {
        begin++;
        std::string key;
        bool is_last;
        do
        {
            begin = parse_key(begin, end, key);
            if (key == "url")
            {
                begin = parse_tokenstream(begin, end, image.url);
            }
            else if (key == "x")
            {
                begin = parse_tokenstream(begin, end, image.x);
            }
            else if (key == "y")
            {
                begin = parse_tokenstream(begin, end, image.y);
            }
            else
            {
                throw std::runtime_error("Unexpected key in image: " + key);
            }
            begin = is_last_in_list(begin, end, is_last);
        } while (!is_last);
        if (begin->type == Token::Type::RBrace)
        {
            return ++begin;
        }
        throw std::runtime_error("Expected }, got " + token_type_to_string(begin->type));
    }

    throw std::runtime_error("Expected {, got " + token_type_to_string(begin->type));
}

template <typename TokenStream>
inline constexpr TokenStream parse_tokenstream(TokenStream begin, TokenStream end, Comment &comment)
{
    if (begin->type == Token::Type::LBrace)
    {
        begin++;
        std::string key;
        bool is_last;
        do
        {
            begin = parse_key(begin, end, key);
            if (key == "author")
            {
                begin = parse_tokenstream(begin, end, comment.author);
            }
            else if (key == "content")
            {
                begin = parse_tokenstream(begin, end, comment.content);
            }
            else if (key == "timestamp")
            {
                begin = parse_tokenstream(begin, end, comment.timestamp);
            }
            else
            {
                throw std::runtime_error("Unexpected key in comment: " + key);
            }
            begin = is_last_in_list(begin, end, is_last);
        } while (!is_last);
        if (begin->type == Token::Type::RBrace)
        {
            return ++begin;
        }
        throw std::runtime_error("Expected }, got " + token_type_to_string(begin->type));
    }

    throw std::runtime_error("Expected {, got " + token_type_to_string(begin->type));
}

template <typename TokenStream>
inline constexpr TokenStream parse_tokenstream(TokenStream begin, TokenStream end, BlogPost &blogPost)
{
    if (begin->type == Token::Type::LBrace)
    {
        begin++;
        std::string key;
        bool is_last;
        do
        {
            begin = parse_key(begin, end, key);
            if (key == "title")
            {
                begin = parse_tokenstream(begin, end, blogPost.title);
            }
            else if (key == "author")
            {
                begin = parse_tokenstream(begin, end, blogPost.author);
            }
            else if (key == "content")
            {
                begin = parse_tokenstream(begin, end, blogPost.content);
            }
            else if (key == "timestamp")
            {
                begin = parse_tokenstream(begin, end, blogPost.timestamp);
            }
            else if (key == "image")
            {
                begin = parse_tokenstream(begin, end, blogPost.image);
            }
            else if (key == "comments")
            {
                begin = parse_array(begin, end, std::back_inserter(blogPost.comments));
            }
            else
            {
                throw std::runtime_error("Unexpected key in blog post: " + key);
            }
            begin = is_last_in_list(begin, end, is_last);
        } while (!is_last);
        if (begin->type == Token::Type::RBrace)
        {
            return begin++;
        }
        throw std::runtime_error("Expected }, got " + token_type_to_string(begin->type));
    }

    throw std::runtime_error("Expected {, got " + token_type_to_string(begin->type));
}

int main()
{
    std::string json = R"(
        {
            "author": "Max Walz",
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
    std::string image = R"(
        {
            "url": "https://www.google.com",
            "x": 800,
            "y": 600
        })";
    std::string comment = R"(
        {
            "author": "John Doe",
            "content": "This is a comment",
            "timestamp": 1234567900
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