#pragma once

#include <iterator>
#include <iostream>

#define REACT_WITH_TOKENIZER_ERROR()                                                    \
    Token errorToken = {Token::Type::Error, &*begin, static_cast<size_t>(end - begin)}; \
    *output++ = errorToken;                                                             \
    return output;

struct Token
{
    enum class Type
    {
        String,
        Integer,
        Float,
        Colon,
        Comma,
        LBrace,
        RBrace,
        LBracket,
        RBracket,
        True,
        False,
        Null,
        End,
        Error
    };

    Type type;
    const char *value;
    size_t length;
};

enum class TokenizerState
{
    None,
    StartingString,
    ReadingString,
    ReadingNumber,
    ReadingNumberAfterDecimalPoint,
    ReadingTrue,
    ReadingFalse,
    ReadingNull
};

constexpr inline bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

constexpr inline bool isWhitespace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

constexpr inline bool isValidDelimiter(char c)
{
    return c == ',' || c == '}' || c == ']' || isWhitespace(c);
}

template <class CharIterator, class TokenIterator>
constexpr TokenIterator tokenize(CharIterator begin, CharIterator end, TokenIterator output);
template <class TokenIterator, typename T>
constexpr TokenIterator parse_tokenstream(TokenIterator begin, TokenIterator end, T &output);

#define FIELD_PARSER(Name)                                  \
    if (key == #Name)                                       \
    {                                                       \
        begin = parse_tokenstream(begin, end, output.Name); \
    }                                                       \
    else

#define FIELD_PARSER_ARRAY(Name)                                          \
    if (key == #Name)                                                     \
    {                                                                     \
        begin = parse_array(begin, end, std::back_inserter(output.Name)); \
    }                                                                     \
    else

// TODO: Handle objects without fields
#define OBJECT_PARSER(ObjectType, Fields)                                                                        \
    template <class TokenIterator>                                                                               \
    inline constexpr TokenIterator parse_tokenstream(TokenIterator begin, TokenIterator end, ObjectType &output) \
    {                                                                                                            \
        if (begin->type == Token::Type::LBrace)                                                                  \
        {                                                                                                        \
            begin++;                                                                                             \
            std::string key;                                                                                     \
            bool is_last;                                                                                        \
            do                                                                                                   \
            {                                                                                                    \
                begin = parse_key(begin, end, key);                                                              \
                Fields                                                                                           \
                {                                                                                                \
                    throw std::runtime_error("Unexpected key in image: " + key);                                 \
                }                                                                                                \
                begin = is_last_in_list(begin, end, is_last);                                                    \
            } while (!is_last);                                                                                  \
            return ++begin;                                                                                      \
        }                                                                                                        \
        throw std::runtime_error("Expected left brace, got " + token_type_to_string(begin->type) + ".");         \
    }

// +-----------------+
// | IMPLEMENTATIONS |
// +-----------------+

inline constexpr std::string token_type_to_string(Token::Type type)
{
    switch (type)
    {
    case Token::Type::String:
        return "String";
    case Token::Type::Integer:
        return "Integer";
    case Token::Type::Float:
        return "Float";
    case Token::Type::Colon:
        return "Colon";
    case Token::Type::Comma:
        return "Comma";
    case Token::Type::LBrace:
        return "LBrace";
    case Token::Type::RBrace:
        return "RBrace";
    case Token::Type::LBracket:
        return "LBracket";
    case Token::Type::RBracket:
        return "RBracket";
    case Token::Type::True:
        return "True";
    case Token::Type::False:
        return "False";
    case Token::Type::Null:
        return "Null";
    case Token::Type::End:
        return "End";
    case Token::Type::Error:
        return "Error";
    default:
        return "Unknown";
    }
}

template <class CharIterator, class TokenIterator>
inline constexpr TokenIterator tokenize(CharIterator begin, CharIterator end, TokenIterator output)
{
    TokenizerState state = TokenizerState::None;
    const char *value = nullptr;
    size_t length = 0;

    while (begin != end)
    {
        switch (state)
        {
        case TokenizerState::None:
            switch (*begin)
            {
            case '{':
                *output++ = {Token::Type::LBrace, nullptr, 0};
                break;
            case '}':
                *output++ = {Token::Type::RBrace, nullptr, 0};
                break;
            case '[':
                *output++ = {Token::Type::LBracket, nullptr, 0};
                break;
            case ']':
                *output++ = {Token::Type::RBracket, nullptr, 0};
                break;
            case ':':
                *output++ = {Token::Type::Colon, nullptr, 0};
                break;
            case ',':
                *output++ = {Token::Type::Comma, nullptr, 0};
                break;
            case '"':
                state = TokenizerState::StartingString;
                length = 0;
                break;
            case 't':
                state = TokenizerState::ReadingTrue;
                break;
            case 'f':
                state = TokenizerState::ReadingFalse;
                break;
            case 'n':
                state = TokenizerState::ReadingNull;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                state = TokenizerState::ReadingNumber;
                value = &*begin;
                length = 1;
                break;
            default:
                if (!isWhitespace(*begin))
                {
                    REACT_WITH_TOKENIZER_ERROR();
                }
                break;
            }
            begin++;
            break; // case TokenizerState::None
        case TokenizerState::StartingString:
            value = &*begin;
            state = TokenizerState::ReadingString;
        case TokenizerState::ReadingString:
            if (*begin == '"')
            {
                Token token = {Token::Type::String, value, length};
                *output++ = token;
                state = TokenizerState::None;
            }
            else
                length++;
            begin++;
            break; // case TokenizerState::ReadingString
        case TokenizerState::ReadingNumber:
            if (isDigit(*begin))
            {
                length++;
                begin++;
            }
            else if (*begin == '.')
            {
                state = TokenizerState::ReadingNumberAfterDecimalPoint;
                length++;
                begin++;
            }
            else if (isValidDelimiter(*begin))
            {
                Token token = {Token::Type::Integer, value, length};
                *output++ = token;
                state = TokenizerState::None;
            }
            else
            {
                REACT_WITH_TOKENIZER_ERROR();
            }
            break; // case TokenizerState::ReadingNumber
        case TokenizerState::ReadingNumberAfterDecimalPoint:
            if (isDigit(*begin))
            {
                length++;
            }
            else if (isValidDelimiter(*begin))
            {
                Token token = {Token::Type::Float, value, length};
                *output++ = token;
                state = TokenizerState::None;
            }
            else
            {
                REACT_WITH_TOKENIZER_ERROR();
            }
            begin++;
            break; // case TokenizerState::ReadingInteger
        case TokenizerState::ReadingTrue:
            if (*begin++ == 'r' && *begin++ == 'u' && *begin++ == 'e')
            {
                *output++ = {Token::Type::True, nullptr, 0};
                state = TokenizerState::None;
            }
            else
            {
                REACT_WITH_TOKENIZER_ERROR();
            }
            break; // case TokenizerState::ReadingTrue
        case TokenizerState::ReadingFalse:
            if (*begin++ == 'a' && *begin++ == 'l' && *begin++ == 's' && *begin++ == 'e')
            {
                *output++ = {Token::Type::False, nullptr, 0};
                state = TokenizerState::None;
            }
            else
            {
                REACT_WITH_TOKENIZER_ERROR();
            }
            break; // case TokenizerState::ReadingFalse
        case TokenizerState::ReadingNull:
            if (*begin++ == 'u' && *begin++ == 'l' && *begin++ == 'l')
            {
                *output++ = {Token::Type::Null, nullptr, 0};
                state = TokenizerState::None;
            }
            else
            {
                REACT_WITH_TOKENIZER_ERROR();
            }
            break; // case TokenizerState::ReadingNull
        default:
            length++;
            break;
        }
    }
    *output++ = {Token::Type::End, nullptr, 0};
    return output;
}

#define PARSE_TOKENSTREAM_IMPL(TYPE, TOKEN_TYPE, PARSER)                                                      \
    template <class TokenIterator>                                                                            \
    inline constexpr TokenIterator parse_tokenstream(TokenIterator begin, TokenIterator end, TYPE &output)    \
    {                                                                                                         \
        if (begin->type == Token::Type::TOKEN_TYPE)                                                           \
        {                                                                                                     \
            output = PARSER(begin->value);                                                                    \
            return ++begin;                                                                                   \
        }                                                                                                     \
                                                                                                              \
        throw std::runtime_error("Expected " #TOKEN_TYPE ", got " + token_type_to_string(begin->type) + "."); \
    }

PARSE_TOKENSTREAM_IMPL(uint8_t, Integer, [](const char *value)
                       { return static_cast<uint8_t>(std::atoi(value)); })
PARSE_TOKENSTREAM_IMPL(uint16_t, Integer, [](const char *value)
                       { return static_cast<uint16_t>(std::atoi(value)); })
PARSE_TOKENSTREAM_IMPL(uint32_t, Integer, [](const char *value)
                       { return static_cast<uint32_t>(std::atoi(value)); })
PARSE_TOKENSTREAM_IMPL(uint64_t, Integer, [](const char *value)
                       { return static_cast<uint64_t>(std::atol(value)); })
PARSE_TOKENSTREAM_IMPL(int8_t, Integer, [](const char *value)
                       { return static_cast<int8_t>(std::atoi(value)); })
PARSE_TOKENSTREAM_IMPL(int16_t, Integer, [](const char *value)
                       { return static_cast<int16_t>(std::atoi(value)); })
PARSE_TOKENSTREAM_IMPL(int32_t, Integer, [](const char *value)
                       { return static_cast<int32_t>(std::atoi(value)); })
PARSE_TOKENSTREAM_IMPL(int64_t, Integer, [](const char *value)
                       { return static_cast<int64_t>(std::atol(value)); })
PARSE_TOKENSTREAM_IMPL(float, Float, [](const char *value)
                       { return std::atof(value); })
PARSE_TOKENSTREAM_IMPL(double, Float, [](const char *value)
                       { return std::atof(value); })

template <class TokenIterator>
inline constexpr TokenIterator parse_tokenstream(TokenIterator begin, TokenIterator end, std::string &output)
{
    if (begin->type == Token::Type::String)
    {
        output = std::string(begin->value, begin->length);
        return ++begin;
    }

    throw std::runtime_error("Expected String, got " + token_type_to_string(begin->type) + ".");
}

template <class TokenIterator>
inline constexpr TokenIterator parse_tokenstream(TokenIterator begin, TokenIterator end, bool &output)
{
    if (begin->type == Token::Type::True)
    {
        output = true;
        return ++begin;
    }
    else if (begin->type == Token::Type::False)
    {
        output = false;
        return ++begin;
    }

    throw std::runtime_error("Expected True or False, got " + token_type_to_string(begin->type) + ".");
}

template <class TokenIterator, class OutputIterator, typename T>
inline constexpr TokenIterator parse_T_array(TokenIterator begin, TokenIterator end, OutputIterator output)
{
    if (begin->type == Token::Type::LBracket)
    {
        begin++;
        while (begin->type != Token::Type::RBracket)
        {
            T parseResult;
            begin = parse_tokenstream(begin, end, parseResult);
            *output++ = parseResult;
            if (begin->type == Token::Type::Comma)
            {
                begin++;
            }
        }
        return ++begin;
    }

    throw std::runtime_error("Expected left bracket, got " + token_type_to_string(begin->type) + ".");
}

template <class TokenIterator, class OutputIterator>
inline constexpr TokenIterator parse_array(TokenIterator begin, TokenIterator end, OutputIterator output)
{
    // TODO: Figure out way to do this for non-std::back_insert_iterator-iterators
    return parse_T_array<TokenIterator, OutputIterator, typename OutputIterator::container_type::value_type>(begin, end, output);
}

template <class TokenIterator>
inline constexpr TokenIterator parse_key(TokenIterator begin, TokenIterator end, std::string &key)
{
    if (begin->type == Token::Type::String)
    {
        key = std::string(begin->value, begin->length);
        if ((++begin)->type == Token::Type::Colon)
        {
            return ++begin;
        }
    }

    throw std::runtime_error("Expected String, got " + token_type_to_string(begin->type) + ".");
}

template <class TokenIterator>
inline constexpr TokenIterator is_last_in_list(TokenIterator begin, TokenIterator end, bool &is_last)
{
    is_last = begin->type != Token::Type::Comma;
    if (!is_last)
        return ++begin;
    return begin;
}