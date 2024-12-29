#pragma once

#include <algorithm>
#include <charconv>
#include <iterator>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#define NUMBER_DIGIT_COUNT 16

#define REACT_WITH_TOKENIZER_ERROR()                                                                                   \
  Token errorToken = {Token::Type::Error, &*begin, static_cast<size_t>(end - begin)};                                  \
  *output++ = errorToken;                                                                                              \
  return output;

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

// Deserialization
template <class CharIterator, class TokenIterator>
constexpr TokenIterator tokenize(CharIterator begin, CharIterator end, TokenIterator output);

template <typename T> struct json {
  template <typename T_Other> friend struct json;

  template <class Container> static constexpr T deserialize(Container json);
  template <class Container> static constexpr void deserialize(Container json, T &output);
  template <class OutputIterator> static constexpr OutputIterator serialize(T const &object, OutputIterator output);

private:
  template <class TokenIterator>
  static constexpr TokenIterator parse_tokenstream(TokenIterator begin, TokenIterator end, T &output);
};

#define PARTIALLY_SPECIALIZED_JSON(Type)                                                                               \
  struct json<Type> {                                                                                                  \
    template <typename T_Other> friend struct json;                                                                    \
                                                                                                                       \
    template <class Container> static constexpr Type deserialize(Container json) {                                     \
      Type res;                                                                                                        \
      deserialize(json, res);                                                                                          \
      return res;                                                                                                      \
    }                                                                                                                  \
    template <class Container> static constexpr void deserialize(Container json, Type &output) {                       \
      std::vector<Token> tokens{};                                                                                     \
      tokenize(std::begin(json), std::end(json), std::back_inserter(tokens));                                          \
      parse_tokenstream(tokens.begin(), tokens.end(), output);                                                         \
    }                                                                                                                  \
    template <class OutputIterator>                                                                                    \
    static constexpr OutputIterator serialize(Type const &object, OutputIterator output);                              \
                                                                                                                       \
  private:                                                                                                             \
    template <class TokenIterator>                                                                                     \
    static constexpr TokenIterator parse_tokenstream(TokenIterator begin, TokenIterator end, Type &output);            \
  };

#define FIELD_PARSER(Name)                                                                                             \
  if (key == #Name) {                                                                                                  \
    begin = json<decltype(output.Name)>::parse_tokenstream(begin, end, output.Name);                                   \
  } else

#define INHERITANCE_PARSER(ObjectType, InheritingType)                                                                 \
  if (key == #InheritingType) {                                                                                        \
    output = new InheritingType();                                                                                     \
    begin = json<InheritingType>::parse_tokenstream(begin, end, *dynamic_cast<InheritingType *>(output));              \
  } else

// TODO: Handle objects without fields
#define TEMPLATED_OBJECT_PARSER(TemplateArgs, ObjectType, Fields)                                                      \
  template <TemplateArgs>                                                                                              \
  template <class TokenIterator>                                                                                       \
  inline constexpr TokenIterator json<ObjectType>::parse_tokenstream(TokenIterator begin, TokenIterator end,           \
                                                                     ObjectType &output) {                             \
    if (begin->type == Token::Type::LBrace) {                                                                          \
      begin++;                                                                                                         \
      std::string key;                                                                                                 \
      bool is_last;                                                                                                    \
      do {                                                                                                             \
        begin = parse_key(begin, end, key);                                                                            \
        Fields { throw std::runtime_error("Unexpected key in " #ObjectType " : " + key); }                             \
        begin = is_last_in_list(begin, end, is_last);                                                                  \
      } while (!is_last);                                                                                              \
      return ++begin;                                                                                                  \
    }                                                                                                                  \
    throw std::runtime_error("Expected left brace, got " + token_type_to_string(begin->type) + ".");                   \
  }

#define OBJECT_PARSER(ObjectType, Fields) TEMPLATED_OBJECT_PARSER(, ObjectType, Fields)

#define ABSTRACT_OBJECT_PARSER(ObjectType, Fields, Instances) OBJECT_PARSER(ObjectType *, Fields Instances)

#define FIELD_SERIALIZER(field)                                                                                        \
  if (!first)                                                                                                          \
    *output++ = ',';                                                                                                   \
  first = false;                                                                                                       \
  output = json<const char *>::serialize(#field, output);                                                              \
  *output++ = ':';                                                                                                     \
  *output++ = ' ';                                                                                                     \
  output = json<decltype(object.field)>::serialize(object.field, output);
#define INHERITANCE_SERIALIZER(ObjectType, InheritingType)                                                             \
  if (dynamic_cast<InheritingType *>(object)) {                                                                        \
    if (!first)                                                                                                        \
      *output++ = ',';                                                                                                 \
    first = false;                                                                                                     \
    output = json<const char *>::serialize(#InheritingType, output);                                                   \
    *output++ = ':';                                                                                                   \
    *output++ = ' ';                                                                                                   \
    output = json<InheritingType>::serialize(*dynamic_cast<InheritingType *>(object), output);                         \
  }

#define TEMPLATED_OBJECT_SERIALIZER(TemplateArgs, ObjectType, Fields)                                                  \
  template <TemplateArgs>                                                                                              \
  template <class OutputIterator>                                                                                      \
  inline constexpr OutputIterator json<ObjectType>::serialize(ObjectType const &object, OutputIterator output) {       \
    bool first = true;                                                                                                 \
    *output++ = '{';                                                                                                   \
    Fields *output++ = '}';                                                                                            \
                                                                                                                       \
    return output;                                                                                                     \
  }
#define OBJECT_SERIALIZER(ObjectType, Fields) TEMPLATED_OBJECT_SERIALIZER(, ObjectType, Fields)
#define ABSTRACT_OBJECT_SERIALIZER(ObjectType, Fields, Instances) OBJECT_SERIALIZER(ObjectType *, Fields Instances)

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

template <class CharIterator, class TokenIterator>
inline constexpr TokenIterator tokenize(CharIterator begin, CharIterator end, TokenIterator output) {
  TokenizerState state = TokenizerState::None;
  const char *value = nullptr;
  size_t length = 0;

  while (begin != end) {
    switch (state) {
    case TokenizerState::None:
      switch (*begin) {
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
        if (!isWhitespace(*begin)) {
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
      if (*begin == '"') {
        Token token = {Token::Type::String, value, length};
        *output++ = token;
        state = TokenizerState::None;
      } else
        length++;
      begin++;
      break; // case TokenizerState::ReadingString
    case TokenizerState::ReadingNumber:
      if (isDigit(*begin)) {
        length++;
        begin++;
      } else if (*begin == '.') {
        state = TokenizerState::ReadingNumberAfterDecimalPoint;
        length++;
        begin++;
      } else if (isValidDelimiter(*begin)) {
        Token token = {Token::Type::Integer, value, length};
        *output++ = token;
        state = TokenizerState::None;
      } else {
        REACT_WITH_TOKENIZER_ERROR();
      }
      break; // case TokenizerState::ReadingNumber
    case TokenizerState::ReadingNumberAfterDecimalPoint:
      if (isDigit(*begin)) {
        length++;
        begin++;
      } else if (isValidDelimiter(*begin)) {
        Token token = {Token::Type::Float, value, length};
        *output++ = token;
        state = TokenizerState::None;
      } else {
        REACT_WITH_TOKENIZER_ERROR();
      }
      break; // case TokenizerState::ReadingInteger
    case TokenizerState::ReadingTrue:
      if (*begin++ == 'r' && *begin++ == 'u' && *begin++ == 'e') {
        *output++ = {Token::Type::True, nullptr, 0};
        state = TokenizerState::None;
      } else {
        REACT_WITH_TOKENIZER_ERROR();
      }
      break; // case TokenizerState::ReadingTrue
    case TokenizerState::ReadingFalse:
      if (*begin++ == 'a' && *begin++ == 'l' && *begin++ == 's' && *begin++ == 'e') {
        *output++ = {Token::Type::False, nullptr, 0};
        state = TokenizerState::None;
      } else {
        REACT_WITH_TOKENIZER_ERROR();
      }
      break; // case TokenizerState::ReadingFalse
    case TokenizerState::ReadingNull:
      if (*begin++ == 'u' && *begin++ == 'l' && *begin++ == 'l') {
        *output++ = {Token::Type::Null, nullptr, 0};
        state = TokenizerState::None;
      } else {
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

#define JSON_IMPL_PRIMITIVE(PrimitiveType, TokenType, Parser)                                                          \
  template <>                                                                                                          \
  template <class OutputIterator>                                                                                      \
  static constexpr OutputIterator json<PrimitiveType>::serialize(PrimitiveType const &object, OutputIterator output) { \
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
  template <class TokenIterator>                                                                                       \
  inline constexpr TokenIterator json<PrimitiveType>::parse_tokenstream(TokenIterator begin, TokenIterator end,        \
                                                                        PrimitiveType &output) {                       \
    if (begin->type == Token::Type::TokenType) {                                                                       \
      output = Parser;                                                                                                 \
      return ++begin;                                                                                                  \
    }                                                                                                                  \
                                                                                                                       \
    throw std::runtime_error("Expected " #TokenType ", got " + token_type_to_string(begin->type) + ".");               \
  }
JSON_IMPL_PRIMITIVE(uint8_t, Integer, static_cast<uint8_t>(std::atoi(begin->value)))
JSON_IMPL_PRIMITIVE(uint16_t, Integer, static_cast<uint16_t>(std::atoi(begin->value)))
JSON_IMPL_PRIMITIVE(uint32_t, Integer, static_cast<uint32_t>(std::atoi(begin->value)))
JSON_IMPL_PRIMITIVE(uint64_t, Integer, static_cast<uint64_t>(std::atol(begin->value)))
JSON_IMPL_PRIMITIVE(int8_t, Integer, static_cast<int8_t>(std::atoi(begin->value)))
JSON_IMPL_PRIMITIVE(int16_t, Integer, static_cast<int16_t>(std::atoi(begin->value)))
JSON_IMPL_PRIMITIVE(int32_t, Integer, static_cast<int32_t>(std::atoi(begin->value)))
JSON_IMPL_PRIMITIVE(int64_t, Integer, static_cast<int64_t>(std::atol(begin->value)))
JSON_IMPL_PRIMITIVE(float, Float || begin->type == Token::Type::Integer, std::atof(begin->value))
JSON_IMPL_PRIMITIVE(double, Float || begin->type == Token::Type::Integer, std::atof(begin->value))

template <>
template <class TokenIterator>
inline constexpr TokenIterator json<std::string>::parse_tokenstream(TokenIterator begin, TokenIterator end,
                                                                    std::string &output) {
  if (begin->type == Token::Type::String) {
    output = std::string(begin->value, begin->length);
    return ++begin;
  }

  throw std::runtime_error("Expected String, got " + token_type_to_string(begin->type) + ".");
}

template <>
template <class TokenIterator>
inline constexpr TokenIterator json<bool>::parse_tokenstream(TokenIterator begin, TokenIterator end, bool &output) {
  if (begin->type == Token::Type::True) {
    output = true;
    return ++begin;
  } else if (begin->type == Token::Type::False) {
    output = false;
    return ++begin;
  }

  throw std::runtime_error("Expected True or False, got " + token_type_to_string(begin->type) + ".");
}

template <typename T, size_t n> PARTIALLY_SPECIALIZED_JSON(std::array<T COMMA n>);
template <size_t n> PARTIALLY_SPECIALIZED_JSON(std::array<char COMMA n>);

template <size_t n>
template <class TokenIterator>
inline constexpr TokenIterator json<std::array<char, n>>::parse_tokenstream(TokenIterator begin, TokenIterator end,
                                                                            std::array<char, n> &output) {
  if (begin->type == Token::Type::String) {
    memcpy(output.data(), begin->value, std::min(n, begin->length));
    if (begin->length < output.size())
      output[begin->length] = 0;
    return ++begin;
  }

  throw std::runtime_error("Expected String, got " + token_type_to_string(begin->type) + ".");
}

template <typename T, size_t n>
template <class TokenIterator>
inline constexpr TokenIterator json<std::array<T, n>>::parse_tokenstream(TokenIterator begin, TokenIterator end,
                                                                         std::array<T, n> &output) {
  size_t i = 0;
  if (begin->type == Token::Type::LBracket) {
    begin++;
    while (begin->type != Token::Type::RBracket) {
      begin = json<T>::parse_tokenstream(begin, end, output[i++]);
      if (begin->type == Token::Type::Comma) {
        begin++;
        if (i >= n) {
          throw std::runtime_error("Expected at most " + std::to_string(n) + " elements, got more.");
        }
      }
    }
    return ++begin;
  }

  throw std::runtime_error("Expected left bracket, got " + token_type_to_string(begin->type) + ".");
}

template <class T_Array>
template <class TokenIterator>
inline constexpr TokenIterator json<T_Array>::parse_tokenstream(TokenIterator begin, TokenIterator end,
                                                                T_Array &output) {
  auto output_it = std::back_inserter(output); // TODO: Maybe allow specifying the iterator type somehow? Alternatively
                                               // build more convenient back_inserter-like iterator
  if (begin->type == Token::Type::LBracket) {
    begin++;
    while (begin->type != Token::Type::RBracket) {
      typename T_Array::value_type element;
      begin = json<typename T_Array::value_type>::parse_tokenstream(begin, end, element);
      output_it++ = element;
      if (begin->type == Token::Type::Comma) {
        begin++;
      }
    }
    return ++begin;
  }

  throw std::runtime_error("Expected left bracket, got " + token_type_to_string(begin->type) + ".");
}

template <class TokenIterator>
inline constexpr TokenIterator parse_key(TokenIterator begin, TokenIterator end, std::string &key) {
  if (begin->type == Token::Type::String) {
    key = std::string(begin->value, begin->length);
    if ((++begin)->type == Token::Type::Colon) {
      return ++begin;
    }
  }

  throw std::runtime_error("Expected String, got " + token_type_to_string(begin->type) + ".");
}

template <class TokenIterator>
inline constexpr TokenIterator is_last_in_list(TokenIterator begin, TokenIterator end, bool &is_last) {
  is_last = begin->type != Token::Type::Comma;
  if (!is_last)
    return ++begin;
  return begin;
}

template <>
template <class OutputIterator>
inline constexpr OutputIterator json<const char *>::serialize(const char *const &object, OutputIterator output) {
  *output++ = '"';
  output = std::copy(object, object + strlen(object), output);
  *output++ = '"';
  return output;
}

template <typename T> struct ContainerSerializer {
  template <class OutputIterator, class Container>
  inline static constexpr OutputIterator serialize(Container const &container, OutputIterator output);
};

template <>
template <class OutputIterator, class Container>
inline constexpr OutputIterator ContainerSerializer<char>::serialize(Container const &container,
                                                                     OutputIterator output) {
  *output++ = '"';
  output = std::copy(std::begin(container), std::end(container), output);
  *output++ = '"';
  return output;
}
template <typename T>
template <class OutputIterator, class Container>
inline constexpr OutputIterator ContainerSerializer<T>::serialize(Container const &container, OutputIterator output) {
  *output++ = '[';
  bool isFirst = true;
  for (auto element :
       container) // TODO: Profile, consider using references and std::remove_reference, std::remove_const if slow
  {
    if (!isFirst) {
      *output++ = ',';
    }
    isFirst = false;
    output = json<decltype(element)>::serialize(element, output);
  }
  *output++ = ']';
  return output;
}

template <class Container>
template <class OutputIterator>
inline constexpr OutputIterator json<Container>::serialize(Container const &object, OutputIterator output) {
  return ContainerSerializer<typename Container::value_type>::serialize(object, output);
}

template <typename T> template <class Container> inline constexpr T json<T>::deserialize(Container json) {
  T res;
  deserialize(json, res);
  return res;
}

template <typename T> template <class Container> inline constexpr void json<T>::deserialize(Container json, T &output) {
  std::vector<Token> tokens{};
  tokenize(std::begin(json), std::end(json), std::back_inserter(tokens));
  parse_tokenstream(tokens.begin(), tokens.end(), output);
}