# Inclusion

A simple header-only library for `json` parsing. Include `json-parsing.h` in your project to get access to all features. `pp-foreach.h` is used by `json-parsing.h`, so it should be kept in the same location. 

# Features

`pp-foreach.h` provides a framework for defining recursive macros (up to 256 recursions), and can be used independently of `json-parsing.h`.
`json-parsing` implements iterator-based tokenization and handles `json`-parsing and serialization for primitive types. Additionally, macros are provided that allow conveniently defining the parsing and serializing behaviour for new `c++`-types. As parsing and serialization are realized via a template-based trait, parsing is type-safe. There is support for inheritance and templated types.

# Usage

To parse or serialize a new `c++`-type `T`, an implementation for the struct `json<T>` has to be given. For most use-cases, this can be done by using the predefined macros. An example might look as follows:
```c++
  struct T {
    /* <fields> */
  };

  JSON(T, FIELDS(/* <field names> */))

  int main() {
    T t = json<T>::deserialize(/* <json container> */);
  }
```
For a complete example and feature demonstration, see `main.cpp`.
