#pragma once

#include "pp-foreach.h"

#include <charconv>
#include <concepts>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>

// Test for different compilers' include guards to find out whether special treatment should occur
#ifdef _GLIBCXX_ARRAY // GCC
#define __JSON_ARRAYS
#endif
#ifdef _ARRAY_ // MSVC
#define __JSON_ARRAYS
#endif

#ifdef __CLANG_STDINT_H // Clang
#define __JSON_INTTYPES
#endif

#define NUMBER_DIGIT_COUNT 16

#define REACT_WITH_TOKENIZER_ERROR()                                                                                   \
  currentToken = {Token::Type::Error, &*cursor, 10};                                                                   \
  return *this;

// Allow separation of template arguments in macro
#define COMMA ,

struct Token {
  enum class Type {
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

template <typename TS>
concept TokenStream = requires(TS &stream, TS const &const_stream, TS const &other) {
  { const_stream == other } -> std::convertible_to<bool>;
  { *stream } -> std::convertible_to<Token &>;
  { stream->type } -> std::convertible_to<Token::Type>;
  { stream->value } -> std::convertible_to<const char *>;
  { stream->length } -> std::convertible_to<size_t>;
  { stream = other } -> std::convertible_to<TS &>;
};

enum class TokenizerState {
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

template <typename T_Container>
concept Span = requires(T_Container &c) {
  c.begin();
  c.end();
};

template <typename T> struct json {
  template <Span Container> static constexpr T deserialize(Container json);
  template <Span Container> static constexpr void deserialize(Container json, T &output);
  template <std::output_iterator<char> OutputIterator>
  static constexpr OutputIterator serialize(T const &object, OutputIterator output);

  template <TokenStream StreamType> static constexpr StreamType parse_tokenstream(StreamType &stream, T &output);
};

template <class Container>
concept is_container = requires() { typename Container::value_type; };

template <is_container T_Container> struct container_json {
  template <TokenStream StreamType>
  static constexpr StreamType parse_tokenstream(StreamType &stream, T_Container &output);
};

#define PARTIALLY_SPECIALIZED_JSON(Type)                                                                               \
  struct json<Type> {                                                                                                  \
    template <typename T_Other> friend struct json;                                                                    \
                                                                                                                       \
    template <is_container Container> static constexpr Type deserialize(Container json) {                              \
      Type res;                                                                                                        \
      deserialize(json, res);                                                                                          \
      return res;                                                                                                      \
    }                                                                                                                  \
    template <is_container Container> static constexpr void deserialize(Container json, Type &output) {                \
      parse_tokenstream(Tokenizer(std::begin(json)), Tokenizer(std::end(json)), output);                               \
    }                                                                                                                  \
    template <std::output_iterator<char> OutputIterator>                                                               \
    static constexpr OutputIterator serialize(Type const &object, OutputIterator output);                              \
                                                                                                                       \
    template <TokenStream StreamType> static constexpr StreamType parse_tokenstream(StreamType &stream, Type &output); \
  };

// Needs to be its own function because if constexpr compiles undiscarded branches unless it switches on one of the
// template parameters
template <typename T, TokenStream StreamType> inline constexpr StreamType parse_field(StreamType &stream, T &field) {
  if constexpr (is_container<T>) {
    return container_json<T>::parse_tokenstream(stream, field);
  } else {
    return json<T>::parse_tokenstream(stream, field);
  }
}

#define FIELD_PARSER(Name)                                                                                             \
  if (key == #Name) {                                                                                                  \
    stream = parse_field(stream, output.Name);                                                                         \
  } else

#define POINTER_FIELD_PARSER(Name)                                                                                     \
  if (key == #Name) {                                                                                                  \
    stream = parse_field(stream, output->Name);                                                                        \
  } else

#define INHERITANCE_PARSER(InheritingType)                                                                             \
  if (key == #InheritingType) {                                                                                        \
    output = new InheritingType();                                                                                     \
    stream = json<InheritingType>::parse_tokenstream(stream, *dynamic_cast<InheritingType *>(output));                 \
  } else

#define PARSE_ENUM_VALUE(Value)                                                                                        \
  if (value == #Value) {                                                                                               \
    output = Value;                                                                                                    \
  } else

#define PARSE_FIELDS(...) FOR_EACH(FIELD_PARSER, __VA_ARGS__)
#define PARSE_POINTER_FIELDS(...) FOR_EACH(POINTER_FIELD_PARSER, __VA_ARGS__)
#define PARSE_SUBTYPES(...) FOR_EACH(INHERITANCE_PARSER, __VA_ARGS__)

#define PARSE_ENUM_VALUES(...) FOR_EACH(PARSE_ENUM_VALUE, __VA_ARGS__)

#define __UNEXPECTED_FIELD_ERROR(...) throw std::runtime_error("Unexpected key in " #__VA_ARGS__ " : " + key);
#define __UNEXPECTED_VALUE_ERROR(...) throw std::runtime_error("Unexpected value in " #__VA_ARGS__ " : " + value);
#define __OBJECT_NAME_FOR_ERROR_EXPANDED(...) #__VA_ARGS__
#define __OBJECT_NAME_FOR_ERROR(...) __OBJECT_NAME_FOR_ERROR_EXPANDED(__VA_ARGS__)

#define TEMPLATED_OBJECT_PARSER(TemplateArgs, ObjectType, ...)                                                         \
  template <TemplateArgs>                                                                                              \
  template <TokenStream StreamType>                                                                                    \
  inline constexpr StreamType json<ObjectType>::parse_tokenstream(StreamType &stream, ObjectType &output) {            \
    if (stream->type == Token::Type::LBrace) {                                                                         \
      stream++;                                                                                                        \
      if (stream->type == Token::Type::RBrace) {                                                                       \
        return ++stream;                                                                                               \
      }                                                                                                                \
      std::string key;                                                                                                 \
      bool is_last;                                                                                                    \
      do {                                                                                                             \
        stream = parse_key(stream, key);                                                                               \
        __VA_ARGS__{__UNEXPECTED_FIELD_ERROR(ObjectType)} stream = is_last_in_list(stream, is_last);                   \
      } while (!is_last);                                                                                              \
      if (stream->type == Token::Type::RBrace) {                                                                       \
        return ++stream;                                                                                               \
      }                                                                                                                \
      throw std::runtime_error("Expected right brace, got " + token_type_to_string(stream->type) +                     \
                               " while parsing " __OBJECT_NAME_FOR_ERROR(ObjectType) "!");                             \
    }                                                                                                                  \
    throw std::runtime_error("Expected left brace, got " + token_type_to_string(stream->type) +                        \
                             " while parsing " __OBJECT_NAME_FOR_ERROR(ObjectType) "!");                               \
  }

#define OBJECT_PARSER(ObjectType, ...) TEMPLATED_OBJECT_PARSER(, ObjectType, __VA_ARGS__)

#define ENUM_PARSER(EnumType, ...)                                                                                     \
  template <>                                                                                                          \
  template <TokenStream StreamType>                                                                                    \
  inline constexpr StreamType json<EnumType>::parse_tokenstream(StreamType &stream, EnumType &output) {                \
    if (stream->type == Token::Type::String) {                                                                         \
      std::string value = std::string(stream->value, stream->length);                                                  \
      __VA_ARGS__ { __UNEXPECTED_VALUE_ERROR(ObjectType) }                                                             \
      return ++stream;                                                                                                 \
    } else if (stream->type == Token::Type::Integer) {                                                                 \
      output = static_cast<EnumType>(std::atoi(stream->value));                                                        \
    }                                                                                                                  \
    throw std::runtime_error("Expected string or integer, got " + token_type_to_string(stream->type) +                 \
                             "while parsing " __OBJECT_NAME_FOR_ERROR(EnumType) "!");                                  \
  }

struct ContainerSerializer {
  template <std::output_iterator<char> OutputIterator, is_container Container>
  inline static constexpr OutputIterator serialize(Container const &container, OutputIterator output);
};

template <typename T, std::output_iterator<char> OutputIterator>
OutputIterator serialize_field(T const &field, OutputIterator output) {
  if constexpr (is_container<T>) {
    return ContainerSerializer::serialize(field, output);
  } else {
    return json<T>::serialize(field, output);
  }
}

#define FIELD_SERIALIZER(field)                                                                                        \
  if (!first)                                                                                                          \
    *output++ = ',';                                                                                                   \
  first = false;                                                                                                       \
  output = json<const char *>::serialize(#field, output);                                                              \
  *output++ = ':';                                                                                                     \
  *output++ = ' ';                                                                                                     \
  output = serialize_field(object.field, output);

#define POINTER_FIELD_SERIALIZER(field)                                                                                \
  if (!first)                                                                                                          \
    *output++ = ',';                                                                                                   \
  first = false;                                                                                                       \
  output = json<const char *>::serialize(#field, output);                                                              \
  *output++ = ':';                                                                                                     \
  *output++ = ' ';                                                                                                     \
  output = serialize_field(object->field, output);

#define INHERITANCE_SERIALIZER(InheritingType)                                                                         \
  if (dynamic_cast<InheritingType *>(object)) {                                                                        \
    if (!first)                                                                                                        \
      *output++ = ',';                                                                                                 \
    first = false;                                                                                                     \
    output = json<const char *>::serialize(#InheritingType, output);                                                   \
    *output++ = ':';                                                                                                   \
    *output++ = ' ';                                                                                                   \
    output = json<InheritingType>::serialize(*dynamic_cast<InheritingType *>(object), output);                         \
  }

#define SERIALIZE_FIELDS(...) FOR_EACH(FIELD_SERIALIZER, __VA_ARGS__)
#define SERIALIZE_POINTER_FIELDS(...) FOR_EACH(POINTER_FIELD_SERIALIZER, __VA_ARGS__)
#define SERIALIZE_SUBTYPES(...) FOR_EACH(INHERITANCE_SERIALIZER, __PROTECT(__VA_ARGS__))

#define TEMPLATED_OBJECT_SERIALIZER(TemplateArgs, ObjectType, ...)                                                     \
  template <TemplateArgs>                                                                                              \
  template <std::output_iterator<char> OutputIterator>                                                                 \
  inline constexpr OutputIterator json<ObjectType>::serialize(ObjectType const &object, OutputIterator output) {       \
    bool first = true;                                                                                                 \
    *output++ = '{';                                                                                                   \
    __VA_ARGS__                                                                                                        \
    *output++ = '}';                                                                                                   \
                                                                                                                       \
    return output;                                                                                                     \
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

#define TEMPLATED_JSON(TemplateArgs, ObjectType, ...)                                                                  \
  TEMPLATED_OBJECT_PARSER(__PROTECT(TemplateArgs),                                                                     \
                          __PROTECT(ObjectType) __VA_OPT__(, __FOR_PARSING(__PROTECT(__VA_ARGS__))))                   \
  TEMPLATED_OBJECT_SERIALIZER(__PROTECT(TemplateArgs),                                                                 \
                              __PROTECT(ObjectType) __VA_OPT__(, __FOR_SERIALIZING(__PROTECT(__VA_ARGS__))))

#define JSON(ObjectType, ...)                                                                                          \
  TEMPLATED_JSON(TEMPLATE_ARGS(), __PROTECT(ObjectType) __VA_OPT__(, __PROTECT(__VA_ARGS__)))

#define JSON_ENUM(EnumType, ...)                                                                                       \
  ENUM_PARSER(EnumType __VA_OPT__(, PARSE_ENUM_VALUES(__VA_ARGS__))) // TODO: Implement serializer

// +-----------------+
// | IMPLEMENTATIONS |
// +-----------------+

// Deserialization

inline constexpr std::string token_type_to_string(Token::Type type) {
  switch (type) {
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

#define JSON_IMPL_PRIMITIVE(PrimitiveType, TokenType, Parser)                                                          \
  template <>                                                                                                          \
  template <std::output_iterator<char> OutputIterator>                                                                 \
  inline constexpr OutputIterator json<PrimitiveType>::serialize(PrimitiveType const &object, OutputIterator output) { \
    char num_str[NUMBER_DIGIT_COUNT] = {0};                                                                            \
    std::to_chars(num_str, num_str + NUMBER_DIGIT_COUNT, object);                                                      \
    char *fst = num_str;                                                                                               \
    while (*fst) {                                                                                                     \
      *output++ = *fst++;                                                                                              \
    }                                                                                                                  \
    return output;                                                                                                     \
  }                                                                                                                    \
                                                                                                                       \
  template <>                                                                                                          \
  template <TokenStream StreamType>                                                                                    \
  inline constexpr StreamType json<PrimitiveType>::parse_tokenstream(StreamType &stream, PrimitiveType &output) {      \
    if (stream->type == Token::Type::TokenType) {                                                                      \
      output = Parser;                                                                                                 \
      return ++stream;                                                                                                 \
    }                                                                                                                  \
                                                                                                                       \
    throw std::runtime_error("Expected " #TokenType ", got " + token_type_to_string(stream->type) + "!");              \
  }

#ifdef __JSON_INTTYPES
JSON_IMPL_PRIMITIVE(uint8_t, Integer, static_cast<uint8_t>(std::atoi(stream->value)))
JSON_IMPL_PRIMITIVE(uint16_t, Integer, static_cast<uint16_t>(std::atoi(stream->value)))
JSON_IMPL_PRIMITIVE(uint32_t, Integer, static_cast<uint32_t>(std::atoi(stream->value)))
JSON_IMPL_PRIMITIVE(uint64_t, Integer, static_cast<uint64_t>(std::atol(stream->value)))
JSON_IMPL_PRIMITIVE(int8_t, Integer, static_cast<int8_t>(std::atoi(stream->value)))
JSON_IMPL_PRIMITIVE(int16_t, Integer, static_cast<int16_t>(std::atoi(stream->value)))
JSON_IMPL_PRIMITIVE(int32_t, Integer, static_cast<int32_t>(std::atoi(stream->value)))
JSON_IMPL_PRIMITIVE(int64_t, Integer, static_cast<int64_t>(std::atol(stream->value)))
#else
JSON_IMPL_PRIMITIVE(char, Integer, static_cast<char>(std::atoi(stream->value)))
JSON_IMPL_PRIMITIVE(short, Integer, static_cast<short>(std::atoi(stream->value)))
JSON_IMPL_PRIMITIVE(int, Integer, static_cast<int>(std::atoi(stream->value)))
JSON_IMPL_PRIMITIVE(long, Integer, static_cast<long>(std::atol(stream->value)))
JSON_IMPL_PRIMITIVE(long long, Integer, static_cast<long>(std::atol(stream->value)))
JSON_IMPL_PRIMITIVE(unsigned char, Integer, static_cast<unsigned char>(std::atoi(stream->value)))
JSON_IMPL_PRIMITIVE(unsigned short, Integer, static_cast<unsigned short>(std::atoi(stream->value)))
JSON_IMPL_PRIMITIVE(unsigned int, Integer, static_cast<unsigned int>(std::atoi(stream->value)))
JSON_IMPL_PRIMITIVE(unsigned long, Integer, static_cast<unsigned long>(std::atol(stream->value)))
JSON_IMPL_PRIMITIVE(unsigned long long, Integer, static_cast<unsigned long>(std::atol(stream->value)))
#endif
JSON_IMPL_PRIMITIVE(float, Float || stream->type == Token::Type::Integer, static_cast<float>(std::atof(stream->value)))
JSON_IMPL_PRIMITIVE(double, Float || stream->type == Token::Type::Integer, std::atof(stream->value))

template <>
template <TokenStream StreamType>
inline constexpr StreamType json<std::string>::parse_tokenstream(StreamType &stream, std::string &output) {
  if (stream->type == Token::Type::String) {
    output = std::string(stream->value, stream->length);
    return ++stream;
  }

  throw std::runtime_error("Expected String, got " + token_type_to_string(stream->type) + "!");
}

template <>
template <TokenStream StreamType>
inline constexpr StreamType json<bool>::parse_tokenstream(StreamType &stream, bool &output) {
  if (stream->type == Token::Type::True) {
    output = true;
    return ++stream;
  } else if (stream->type == Token::Type::False) {
    output = false;
    return ++stream;
  }

  throw std::runtime_error("Expected True or False, got " + token_type_to_string(stream->type) + "!");
}

#ifdef __JSON_ARRAYS
template <typename T, size_t n> PARTIALLY_SPECIALIZED_JSON(std::array<T COMMA n>);
template <size_t n> PARTIALLY_SPECIALIZED_JSON(std::array<char COMMA n>);

template <size_t n> struct container_json<std::array<char, n>> {
  template <TokenStream StreamType>
  inline constexpr StreamType parse_tokenstream(StreamType &stream, std::array<char, n> &output) {
    if (stream->type == Token::Type::String) {
      memcpy(output.data(), stream->value, std::min(n, stream->length));
      if (stream->length < output.size())
        output[stream->length] = 0;
      return ++stream;
    }

    throw std::runtime_error("Expected String, got " + token_type_to_string(stream->type) + "!");
  }
};
#endif

template <class T_Iterator, typename T>
concept ContainerInserter = requires(T_Iterator &it, T const &value) { *(it++) = value; };

template <TokenStream StreamType, typename T, ContainerInserter<T> T_It>
inline constexpr StreamType parse_tokenstream_insertion(StreamType &stream, T_It &output_it) {
  if (stream->type == Token::Type::LBracket) {
    stream++;
    while (stream->type != Token::Type::RBracket) {
      T value;
      stream = parse_field(stream, value);
      *(output_it++) = value;
      if (stream->type == Token::Type::Comma) {
        stream++;
      }
    }
    return ++stream;
  }
  throw std::runtime_error("Expected '[', got " + token_type_to_string(stream->type) + "!");
}

#ifdef __JSON_ARRAYS

template <typename T, size_t n> class ArrayInserter {
  size_t index;
  std::array<T, n> &referencedArray;

public:
  ArrayInserter(ArrayInserter<T, n> const &other) : index(other.index), referencedArray(other.referencedArray) {}
  ArrayInserter(std::array<T, n> &reference) : index(0), referencedArray(reference) {}

  inline constexpr T &operator*() { return referencedArray[index]; }
  inline constexpr ArrayInserter operator++() {
    index++;
    return {*this};
  }
  inline constexpr ArrayInserter operator++(int) {
    auto tmp = ArrayInserter(*this);
    index++;
    return tmp;
  }
};

static_assert(ContainerInserter<ArrayInserter<int, 8>, int>, "Simple int array asserter does not work properly!");

template <typename T, size_t n> struct container_json<std::array<T, n>> {
  template <TokenStream StreamType>
  static constexpr StreamType parse_tokenstream(StreamType &stream, std::array<T, n> &output) {
    auto it = ArrayInserter<T, n>(output);
    return parse_tokenstream_insertion<StreamType, T, ArrayInserter<T, n>>(stream, it);
  }
};

#endif

template <>
template <TokenStream StreamType>
inline constexpr StreamType container_json<std::string>::parse_tokenstream(StreamType &stream, std::string &output) {
  if (stream->type == Token::Type::String) {
    output = std::string(stream->value, stream->length);
    return ++stream;
  }

  throw std::runtime_error("Expected String, got " + token_type_to_string(stream->type) + "!");
}

template <class T_Container>
concept has_back_inserter = requires(T_Container &c, typename T_Container::value_type const &v) { c.push_back(v); };

template <is_container T_Container>
template <TokenStream StreamType>
inline constexpr StreamType container_json<T_Container>::parse_tokenstream(StreamType &stream, T_Container &output) {
  if constexpr (has_back_inserter<T_Container>) {
    auto it = std::back_inserter(output);
    return parse_tokenstream_insertion<StreamType, typename T_Container::value_type, decltype(it)>(stream, it);
  } else {
    static_assert(false, "Tried to parse container type without valid handling!");
  }
}

template <TokenStream StreamType> inline constexpr StreamType parse_key(StreamType &stream, std::string &key) {
  if (stream->type == Token::Type::String) {
    key = std::string(stream->value, stream->length);
    if ((++stream)->type == Token::Type::Colon) {
      return ++stream;
    }
  }

  throw std::runtime_error("Expected String, got " + token_type_to_string(stream->type) + "!");
}

template <TokenStream StreamType> inline constexpr StreamType is_last_in_list(StreamType &stream, bool &is_last) {
  is_last = stream->type != Token::Type::Comma;
  if (!is_last)
    return ++stream;
  return stream;
}

template <>
template <std::output_iterator<char> OutputIterator>
inline constexpr OutputIterator json<const char *>::serialize(const char *const &object, OutputIterator output) {
  *output++ = '"';
  output = std::copy(object, object + strlen(object), output);
  *output++ = '"';
  return output;
}

template <std::output_iterator<char> OutputIterator, is_container Container>
inline constexpr OutputIterator ContainerSerializer::serialize(Container const &container, OutputIterator output) {
  if constexpr (std::is_same<typename Container::value_type, char>::value) {
    *output++ = '"';
    output = std::copy(std::begin(container), std::end(container), output);
    *output++ = '"';
    return output;
  }

  *output++ = '[';
  bool isFirst = true;
  for (auto const &element : container) {
    if (!isFirst) {
      *output++ = ',';
    }
    isFirst = false;
    output = serialize_field(element, output);
  }
  *output++ = ']';
  return output;
}

template <typename T> template <Span Container> inline constexpr T json<T>::deserialize(Container json) {
  T res;
  deserialize(json, res);
  return res;
}

template <class CharIterator> class Tokenizer {
  CharIterator cursor;
  CharIterator end;
  Token currentToken;

  Tokenizer(CharIterator const &begin, CharIterator const &end, Token initialToken)
      : cursor(begin), end(end), currentToken(initialToken) {}

public:
  Tokenizer(CharIterator const &begin, CharIterator const &end) : cursor(begin), end(end), currentToken() {}

  inline bool operator==(const Tokenizer<CharIterator> &other) const { return cursor == other.cursor; }
  inline Token &operator*() { return currentToken; }
  inline Token *operator->() { return &currentToken; }
  inline Tokenizer<CharIterator> &operator=(Tokenizer<CharIterator> const &other) {
    cursor = other.cursor;
    currentToken = other.currentToken;
    return *this;
  }

  inline Tokenizer<CharIterator> operator++(int) {
    auto it = cursor;
    auto it_e = cursor;
    auto tok = currentToken;
    ++*this;
    return Tokenizer<CharIterator>(it, it_e, tok);
  }

  inline Tokenizer<CharIterator> &operator++() {
    TokenizerState state = TokenizerState::None;
    const char *value = nullptr;
    size_t length = 0;

    while (cursor != end) {
      switch (state) {
      case TokenizerState::None:
        switch (*cursor) {
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
          if (!isWhitespace(*cursor)) {
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
        if (*cursor++ == '"') {
          currentToken = {Token::Type::String, value, length};
          return *this;
        } else
          length++;
        break; // case TokenizerState::ReadingString
      case TokenizerState::ReadingNumber:
        if (isDigit(*cursor)) {
          length++;
          cursor++;
        } else if (*cursor == '.') {
          state = TokenizerState::ReadingNumberAfterDecimalPoint;
          length++;
          cursor++;
        } else if (isValidDelimiter(*cursor)) {
          currentToken = {Token::Type::Integer, value, length};
          return *this;
        } else {
          REACT_WITH_TOKENIZER_ERROR();
        }
        break; // case TokenizerState::ReadingNumber
      case TokenizerState::ReadingNumberAfterDecimalPoint:
        if (isDigit(*cursor)) {
          length++;
          cursor++;
        } else if (isValidDelimiter(*cursor)) {
          currentToken = {Token::Type::Float, value, length};
          return *this;
        } else {
          REACT_WITH_TOKENIZER_ERROR();
        }
        break; // case TokenizerState::ReadingInteger
      case TokenizerState::ReadingTrue:
        if (*cursor++ == 'r' && *cursor++ == 'u' && *cursor++ == 'e') {
          currentToken = {Token::Type::True, nullptr, 0};
          return *this;
        } else {
          REACT_WITH_TOKENIZER_ERROR();
        }
        break; // case TokenizerState::ReadingTrue
      case TokenizerState::ReadingFalse:
        if (*cursor++ == 'a' && *cursor++ == 'l' && *cursor++ == 's' && *cursor++ == 'e') {
          currentToken = {Token::Type::False, nullptr, 0};
          return *this;
        } else {
          REACT_WITH_TOKENIZER_ERROR();
        }
        break; // case TokenizerState::ReadingFalse
      case TokenizerState::ReadingNull:
        if (*cursor++ == 'u' && *cursor++ == 'l' && *cursor++ == 'l') {
          currentToken = {Token::Type::Null, nullptr, 0};
          return *this;
        } else {
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

template <typename T> template <Span Container> inline constexpr void json<T>::deserialize(Container json, T &output) {
  auto tok = Tokenizer(std::begin(json), std::end(json));
  parse_tokenstream(++tok, output);
}