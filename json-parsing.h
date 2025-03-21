#pragma once

#include "pp-foreach.h"

#include <charconv>
#include <iterator>
#include <stdexcept>
#include <string>
#include <type_traits>

#define NUMBER_DIGIT_COUNT 16

#define REACT_WITH_TOKENIZER_ERROR()                 \
  currentToken = {Token::Type::Error, &*cursor, 10}; \
  return *this;

// Allow separation of template arguments in macro
#define COMMA ,

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

constexpr inline bool isDigit(char c) { return c >= '0' && c <= '9'; }

constexpr inline bool isWhitespace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

constexpr inline bool isValidDelimiter(char c) { return c == ',' || c == '}' || c == ']' || isWhitespace(c); }

// Deserialization

template <typename T>
struct json
{
  template <typename T_Other>
  friend struct json;

  template <class Container>
  static constexpr T deserialize(Container json);
  template <class Container>
  static constexpr void deserialize(Container json, T &output);
  template <class OutputIterator>
  static constexpr OutputIterator serialize(T const &object, OutputIterator output);

private:
  template <class TokenStream>
  static constexpr TokenStream parse_tokenstream(TokenStream &stream, T &output);
};

#define PARTIALLY_SPECIALIZED_JSON(Type)                                                  \
  struct json<Type>                                                                       \
  {                                                                                       \
    template <typename T_Other>                                                           \
    friend struct json;                                                                   \
                                                                                          \
    template <class Container>                                                            \
    static constexpr Type deserialize(Container json)                                     \
    {                                                                                     \
      Type res;                                                                           \
      deserialize(json, res);                                                             \
      return res;                                                                         \
    }                                                                                     \
    template <class Container>                                                            \
    static constexpr void deserialize(Container json, Type &output)                       \
    {                                                                                     \
      parse_tokenstream(Tokenizer(std::begin(json)), Tokenizer(std::end(json)), output);  \
    }                                                                                     \
    template <class OutputIterator>                                                       \
    static constexpr OutputIterator serialize(Type const &object, OutputIterator output); \
                                                                                          \
  private:                                                                                \
    template <class TokenStream>                                                          \
    static constexpr TokenStream parse_tokenstream(TokenStream &stream, Type &output);    \
  };

#define FIELD_PARSER(Name)                                                        \
  if (key == #Name)                                                               \
  {                                                                               \
    stream = json<decltype(output.Name)>::parse_tokenstream(stream, output.Name); \
  }                                                                               \
  else

#define POINTER_FIELD_PARSER(Name)                                                  \
  if (key == #Name)                                                                 \
  {                                                                                 \
    stream = json<decltype(output->Name)>::parse_tokenstream(stream, output->Name); \
  }                                                                                 \
  else

#define INHERITANCE_PARSER(InheritingType)                                                             \
  if (key == #InheritingType)                                                                          \
  {                                                                                                    \
    output = new InheritingType();                                                                     \
    stream = json<InheritingType>::parse_tokenstream(stream, *dynamic_cast<InheritingType *>(output)); \
  }                                                                                                    \
  else

#define PARSE_FIELDS(...) FOR_EACH(FIELD_PARSER, __VA_ARGS__)
#define PARSE_POINTER_FIELDS(...) FOR_EACH(POINTER_FIELD_PARSER, __VA_ARGS__)
#define PARSE_SUBTYPES(...) FOR_EACH(INHERITANCE_PARSER, __VA_ARGS__)

#define __UNEXPECTED_FIELD_ERROR(...) throw std::runtime_error("Unexpected key in " #__VA_ARGS__ " : " + key);

#define TEMPLATED_OBJECT_PARSER(TemplateArgs, ObjectType, ...)                                        \
  template <TemplateArgs>                                                                             \
  template <class TokenStream>                                                                        \
  inline constexpr TokenStream json<ObjectType>::parse_tokenstream(TokenStream &stream,               \
                                                                   ObjectType &output)                \
  {                                                                                                   \
    if (stream->type == Token::Type::LBrace)                                                          \
    {                                                                                                 \
      stream++;                                                                                       \
      std::string key;                                                                                \
      bool is_last;                                                                                   \
      do                                                                                              \
      {                                                                                               \
        stream = parse_key(stream, key);                                                              \
        __VA_ARGS__{__UNEXPECTED_FIELD_ERROR(ObjectType)} stream = is_last_in_list(stream, is_last);  \
      } while (!is_last);                                                                             \
      return ++stream;                                                                                \
    }                                                                                                 \
    throw std::runtime_error("Expected left brace, got " + token_type_to_string(stream->type) + "."); \
  }

#define OBJECT_PARSER(ObjectType, ...) TEMPLATED_OBJECT_PARSER(, ObjectType, __VA_ARGS__)

#define FIELD_SERIALIZER(field)                           \
  if (!first)                                             \
    *output++ = ',';                                      \
  first = false;                                          \
  output = json<const char *>::serialize(#field, output); \
  *output++ = ':';                                        \
  *output++ = ' ';                                        \
  output = json<decltype(object.field)>::serialize(object.field, output);

#define POINTER_FIELD_SERIALIZER(field)                   \
  if (!first)                                             \
    *output++ = ',';                                      \
  first = false;                                          \
  output = json<const char *>::serialize(#field, output); \
  *output++ = ':';                                        \
  *output++ = ' ';                                        \
  output = json<decltype(object->field)>::serialize(object->field, output);

#define INHERITANCE_SERIALIZER(InheritingType)                                                 \
  if (dynamic_cast<InheritingType *>(object))                                                  \
  {                                                                                            \
    if (!first)                                                                                \
      *output++ = ',';                                                                         \
    first = false;                                                                             \
    output = json<const char *>::serialize(#InheritingType, output);                           \
    *output++ = ':';                                                                           \
    *output++ = ' ';                                                                           \
    output = json<InheritingType>::serialize(*dynamic_cast<InheritingType *>(object), output); \
  }

#define SERIALIZE_FIELDS(...) FOR_EACH(FIELD_SERIALIZER, __VA_ARGS__)
#define SERIALIZE_POINTER_FIELDS(...) FOR_EACH(POINTER_FIELD_SERIALIZER, __VA_ARGS__)
#define SERIALIZE_SUBTYPES(...) FOR_EACH(INHERITANCE_SERIALIZER, __PROTECT(__VA_ARGS__))

#define TEMPLATED_OBJECT_SERIALIZER(TemplateArgs, ObjectType, ...)                                             \
  template <TemplateArgs>                                                                                      \
  template <class OutputIterator>                                                                              \
  inline constexpr OutputIterator json<ObjectType>::serialize(ObjectType const &object, OutputIterator output) \
  {                                                                                                            \
    bool first = true;                                                                                         \
    *output++ = '{';                                                                                           \
    __VA_ARGS__                                                                                                \
    *output++ = '}';                                                                                           \
                                                                                                               \
    return output;                                                                                             \
  }

#define OBJECT_SERIALIZER(ObjectType, ...) TEMPLATED_OBJECT_SERIALIZER(, ObjectType, __VA_ARGS__)

#define __ONCE(Macro, Arg) Macro(Arg)
#define __UP_TO_TWICE(Macro, Arg, ...) Macro(Arg) __VA_OPT__(__ONCE(Macro, __VA_ARGS__))

#define __CAT(a, b) a##b
#define __CONCAT(a, b) __CAT(a, b)
#define __CONCAT_FOR_PARSING(a) __CONCAT(PARSE_, __PROTECT(a))
#define __FOR_PARSING(...) __UP_TO_TWICE(__CONCAT_FOR_PARSING, __VA_ARGS__)
#define __CONCAT_FOR_SERIALIZING(a) __CONCAT(SERIALIZE_, __PROTECT(a))
#define __FOR_SERIALIZING(...) __UP_TO_TWICE(__CONCAT_FOR_SERIALIZING, __VA_ARGS__)

#define TEMPLATE_ARGS(...) __VA_ARGS__

#define TEMPLATED_JSON(TemplateArgs, ObjectType, ...)                                                            \
  TEMPLATED_OBJECT_PARSER(__PROTECT(TemplateArgs), __PROTECT(ObjectType), __FOR_PARSING(__PROTECT(__VA_ARGS__))) \
  TEMPLATED_OBJECT_SERIALIZER(__PROTECT(TemplateArgs), __PROTECT(ObjectType), __FOR_SERIALIZING(__PROTECT(__VA_ARGS__)))

#define JSON(ObjectType, ...) TEMPLATED_JSON(TEMPLATE_ARGS(), __PROTECT(ObjectType), __PROTECT(__VA_ARGS__))

// +-----------------+
// | IMPLEMENTATIONS |
// +-----------------+

// Deserialization

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

#define JSON_IMPL_PRIMITIVE(PrimitiveType, TokenType, Parser)                                                        \
  template <>                                                                                                        \
  template <class OutputIterator>                                                                                    \
  static constexpr OutputIterator json<PrimitiveType>::serialize(PrimitiveType const &object, OutputIterator output) \
  {                                                                                                                  \
    char num_str[NUMBER_DIGIT_COUNT] = {0};                                                                          \
    std::to_chars(num_str, num_str + NUMBER_DIGIT_COUNT, object);                                                    \
    char *fst = num_str;                                                                                             \
    while (*fst)                                                                                                     \
    {                                                                                                                \
      *output++ = *fst++;                                                                                            \
    }                                                                                                                \
    return output;                                                                                                   \
  }                                                                                                                  \
                                                                                                                     \
  template <>                                                                                                        \
  template <class TokenStream>                                                                                       \
  inline constexpr TokenStream json<PrimitiveType>::parse_tokenstream(TokenStream &stream, PrimitiveType &output)    \
  {                                                                                                                  \
    if (stream->type == Token::Type::TokenType)                                                                      \
    {                                                                                                                \
      output = Parser;                                                                                               \
      return ++stream;                                                                                               \
    }                                                                                                                \
                                                                                                                     \
    throw std::runtime_error("Expected " #TokenType ", got " + token_type_to_string(stream->type) + ".");            \
  }

JSON_IMPL_PRIMITIVE(uint8_t, Integer, static_cast<uint8_t>(std::atoi(stream->value)))
JSON_IMPL_PRIMITIVE(uint16_t, Integer, static_cast<uint16_t>(std::atoi(stream->value)))
JSON_IMPL_PRIMITIVE(uint32_t, Integer, static_cast<uint32_t>(std::atoi(stream->value)))
JSON_IMPL_PRIMITIVE(uint64_t, Integer, static_cast<uint64_t>(std::atol(stream->value)))
JSON_IMPL_PRIMITIVE(int8_t, Integer, static_cast<int8_t>(std::atoi(stream->value)))
JSON_IMPL_PRIMITIVE(int16_t, Integer, static_cast<int16_t>(std::atoi(stream->value)))
JSON_IMPL_PRIMITIVE(int32_t, Integer, static_cast<int32_t>(std::atoi(stream->value)))
JSON_IMPL_PRIMITIVE(int64_t, Integer, static_cast<int64_t>(std::atol(stream->value)))
JSON_IMPL_PRIMITIVE(float, Float || stream->type == Token::Type::Integer, std::atof(stream->value))
JSON_IMPL_PRIMITIVE(double, Float || stream->type == Token::Type::Integer, std::atof(stream->value))

template <>
template <class TokenStream>
inline constexpr TokenStream json<std::string>::parse_tokenstream(TokenStream &stream, std::string &output)
{
  if (stream->type == Token::Type::String)
  {
    output = std::string(stream->value, stream->length);
    return ++stream;
  }

  throw std::runtime_error("Expected String, got " + token_type_to_string(stream->type) + ".");
}

template <>
template <class TokenStream>
inline constexpr TokenStream json<bool>::parse_tokenstream(TokenStream &stream, bool &output)
{
  if (stream->type == Token::Type::True)
  {
    output = true;
    return ++stream;
  }
  else if (stream->type == Token::Type::False)
  {
    output = false;
    return ++stream;
  }

  throw std::runtime_error("Expected True or False, got " + token_type_to_string(stream->type) + ".");
}

template <typename T, size_t n>
PARTIALLY_SPECIALIZED_JSON(std::array<T COMMA n>);
template <size_t n>
PARTIALLY_SPECIALIZED_JSON(std::array<char COMMA n>);

template <size_t n>
template <class TokenStream>
inline constexpr TokenStream json<std::array<char, n>>::parse_tokenstream(TokenStream &stream,
                                                                          std::array<char, n> &output)
{
  if (stream->type == Token::Type::String)
  {
    memcpy(output.data(), stream->value, std::min(n, stream->length));
    if (stream->length < output.size())
      output[stream->length] = 0;
    return ++stream;
  }

  throw std::runtime_error("Expected String, got " + token_type_to_string(stream->type) + ".");
}

template <typename T, size_t n>
template <class TokenStream>
inline constexpr TokenStream json<std::array<T, n>>::parse_tokenstream(TokenStream &stream,
                                                                       std::array<T, n> &output)
{
  size_t i = 0;
  if (stream->type == Token::Type::LBracket)
  {
    stream++;
    while (stream->type != Token::Type::RBracket)
    {
      stream = json<T>::parse_tokenstream(stream, output[i++]);
      if (stream->type == Token::Type::Comma)
      {
        stream++;
        if (i >= n)
        {
          throw std::runtime_error("Expected at most " + std::to_string(n) + " elements, got more.");
        }
      }
    }
    return ++stream;
  }

  throw std::runtime_error("Expected left bracket, got " + token_type_to_string(stream->type) + ".");
}

template <class T_Array>
template <class TokenStream>
inline constexpr TokenStream json<T_Array>::parse_tokenstream(TokenStream &stream, T_Array &output)
{
  auto output_it = std::back_inserter(output); // TODO: Maybe allow specifying the iterator type somehow? Alternatively
                                               // build more convenient back_inserter-like iterator
  if (stream->type == Token::Type::LBracket)
  {
    stream++;
    while (stream->type != Token::Type::RBracket)
    {
      typename T_Array::value_type element;
      stream = json<typename T_Array::value_type>::parse_tokenstream(stream, element);
      output_it++ = element;
      if (stream->type == Token::Type::Comma)
      {
        stream++;
      }
    }
    return ++stream;
  }

  throw std::runtime_error("Expected left bracket, got " + token_type_to_string(stream->type) + ".");
}

template <class TokenStream>
inline constexpr TokenStream parse_key(TokenStream &stream, std::string &key)
{
  if (stream->type == Token::Type::String)
  {
    key = std::string(stream->value, stream->length);
    if ((++stream)->type == Token::Type::Colon)
    {
      return ++stream;
    }
  }

  throw std::runtime_error("Expected String, got " + token_type_to_string(stream->type) + ".");
}

template <class TokenStream>
inline constexpr TokenStream is_last_in_list(TokenStream &stream, bool &is_last)
{
  is_last = stream->type != Token::Type::Comma;
  if (!is_last)
    return ++stream;
  return stream;
}

template <>
template <class OutputIterator>
inline constexpr OutputIterator json<const char *>::serialize(const char *const &object, OutputIterator output)
{
  *output++ = '"';
  output = std::copy(object, object + strlen(object), output);
  *output++ = '"';
  return output;
}

template <typename T>
struct ContainerSerializer
{
  template <class OutputIterator, class Container>
  inline static constexpr OutputIterator serialize(Container const &container, OutputIterator output);
};

template <>
template <class OutputIterator, class Container>
inline constexpr OutputIterator ContainerSerializer<char>::serialize(Container const &container,
                                                                     OutputIterator output)
{
  *output++ = '"';
  output = std::copy(std::begin(container), std::end(container), output);
  *output++ = '"';
  return output;
}

template <typename T>
template <class OutputIterator, class Container>
inline constexpr OutputIterator ContainerSerializer<T>::serialize(Container const &container, OutputIterator output)
{
  *output++ = '[';
  bool isFirst = true;
  for (auto const &element :
       container) // TODO: Profile, consider using references and std::remove_reference, std::remove_const if slow
  {
    if (!isFirst)
    {
      *output++ = ',';
    }
    isFirst = false;
    output = json<typename Container::value_type>::serialize(element, output);
  }
  *output++ = ']';
  return output;
}

// I do not understand why this is necessary, the general implementation below **should** cover this, but for some reason it does not, so here we are!
template <typename T, uint64_t n>
template <class OutputIterator>
inline constexpr OutputIterator json<std::array<T, n>>::serialize(std::array<T, n> const &object, OutputIterator output)
{
  return ContainerSerializer<T>::serialize(object, output);
}

template <class Container>
template <class OutputIterator>
inline constexpr OutputIterator json<Container>::serialize(Container const &object, OutputIterator output)
{
  return ContainerSerializer<typename Container::value_type>::serialize(object, output);
}

template <typename T>
template <class Container>
inline constexpr T json<T>::deserialize(Container json)
{
  T res;
  deserialize(json, res);
  return res;
}

template <class CharIterator>
class Tokenizer
{
  CharIterator cursor;
  CharIterator end;
  Token currentToken;

  Tokenizer(CharIterator const &begin, CharIterator const &end, Token initialToken) : cursor(begin), end(end), currentToken(initialToken) {}

public:
  Tokenizer(CharIterator const &begin, CharIterator const &end) : cursor(begin), end(end), currentToken() {}

  inline bool operator==(const Tokenizer<CharIterator> &other) const { return cursor == other.cursor; }

  inline Token &operator*() const { return currentToken; }
  inline Token *operator->() { return &currentToken; }
  inline Tokenizer<CharIterator> &operator=(Tokenizer<CharIterator> const &other)
  {
    cursor = other.cursor;
    currentToken = other.currentToken;
    return *this;
  }

  inline Tokenizer<CharIterator> const &operator++(int)
  {
    auto it = cursor;
    auto it_e = cursor;
    auto tok = currentToken;
    ++*this;
    return Tokenizer<CharIterator>(it, it_e, tok);
  }

  inline Tokenizer<CharIterator> &operator++()
  {
    TokenizerState state = TokenizerState::None;
    const char *value = nullptr;
    size_t length = 0;

    while (cursor != end)
    {
      switch (state)
      {
      case TokenizerState::None:
        switch (*cursor)
        {
        case '{':
          currentToken = {Token::Type::LBrace, nullptr, 0};
          ++cursor;
          return *this;
        case '}':
          currentToken = {Token::Type::RBrace, nullptr, 0};
          ++cursor;
          return *this;
        case '[':
          currentToken = {Token::Type::LBracket, nullptr, 0};
          ++cursor;
          return *this;
        case ']':
          currentToken = {Token::Type::RBracket, nullptr, 0};
          ++cursor;
          return *this;
        case ':':
          currentToken = {Token::Type::Colon, nullptr, 0};
          ++cursor;
          return *this;
        case ',':
          currentToken = {Token::Type::Comma, nullptr, 0};
          ++cursor;
          return *this;
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
        case '-':
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
          value = &*cursor;
          length = 1;
          break;
        default:
          if (!isWhitespace(*cursor))
          {
            REACT_WITH_TOKENIZER_ERROR();
          }
          break;
        }
        cursor++;
        break; // case TokenizerState::None
      case TokenizerState::StartingString:
        value = &*cursor;
        state = TokenizerState::ReadingString;
      case TokenizerState::ReadingString:
        if (*cursor++ == '"')
        {
          currentToken = {Token::Type::String, value, length};
          return *this;
        }
        else
          length++;
        break; // case TokenizerState::ReadingString
      case TokenizerState::ReadingNumber:
        if (isDigit(*cursor))
        {
          length++;
          cursor++;
        }
        else if (*cursor == '.')
        {
          state = TokenizerState::ReadingNumberAfterDecimalPoint;
          length++;
          cursor++;
        }
        else if (isValidDelimiter(*cursor))
        {
          currentToken = {Token::Type::Integer, value, length};
          return *this;
        }
        else
        {
          REACT_WITH_TOKENIZER_ERROR();
        }
        break; // case TokenizerState::ReadingNumber
      case TokenizerState::ReadingNumberAfterDecimalPoint:
        if (isDigit(*cursor))
        {
          length++;
          cursor++;
        }
        else if (isValidDelimiter(*cursor))
        {
          currentToken = {Token::Type::Float, value, length};
          return *this;
        }
        else
        {
          REACT_WITH_TOKENIZER_ERROR();
        }
        break; // case TokenizerState::ReadingInteger
      case TokenizerState::ReadingTrue:
        if (*cursor++ == 'r' && *cursor++ == 'u' && *cursor++ == 'e')
        {
          currentToken = {Token::Type::True, nullptr, 0};
          return *this;
        }
        else
        {
          REACT_WITH_TOKENIZER_ERROR();
        }
        break; // case TokenizerState::ReadingTrue
      case TokenizerState::ReadingFalse:
        if (*cursor++ == 'a' && *cursor++ == 'l' && *cursor++ == 's' && *cursor++ == 'e')
        {
          currentToken = {Token::Type::False, nullptr, 0};
          return *this;
        }
        else
        {
          REACT_WITH_TOKENIZER_ERROR();
        }
        break; // case TokenizerState::ReadingFalse
      case TokenizerState::ReadingNull:
        if (*cursor++ == 'u' && *cursor++ == 'l' && *cursor++ == 'l')
        {
          currentToken = {Token::Type::Null, nullptr, 0};
          return *this;
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
    currentToken = {Token::Type::End, nullptr, 0};
    return *this;
  }
};

template <typename T>
template <class Container>
inline constexpr void json<T>::deserialize(Container json, T &output)
{
  parse_tokenstream(++Tokenizer(std::begin(json), std::end(json)), output);
}