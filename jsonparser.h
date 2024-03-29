#ifndef JSONPARSER_H
#define JSONPARSER_H

#include <algorithm>
#include <fstream>
#include <map>
#include <memory>
#include <ostream>
#include <stack>
#include <string>
#include <tuple>
#include <vector>


#define CONFIG_ALLOCATOR
#define CONFIG_STABLE

namespace jsonparser {

typedef enum class _json_token {
  none = 0,
  json_nt_json,
  json_nt_array,
  json_nt_object,
  json_nt_members,
  json_nt_pair,
  json_nt_elements,
  json_nt_value,
  object_starts, // {
  object_ends,   // }
  v_comma,       // ,
  v_pair,        // :
  array_starts,  // [
  array_ends,    // ]
  v_true,        // true
  v_false,       // false
  v_null,        // null
  v_string,      // "(\\(["/bfnrt]|u{Hex}{Hex}{Hex}{Hex}))*"
  v_number,      // \-?(0|[1-9]\d*)(\.\d+)?([Ee][+-]?\d+)?
  eof,
  error,
} json_token;

#ifdef CONFIG_ALLOCATOR
///===-----------------------------------------------------------------------===
///
///               Json Allocator
///
///===-----------------------------------------------------------------------===

template <typename type> class json_allocator {
  size_t capacity;

  using pointer = type *;
  using value_size = /*alignas(alignof(type))*/ unsigned char[sizeof(type)];

  class allocator_node {
  public:
    value_size value;
    constexpr pointer rep() { return reinterpret_cast<pointer>(value); }
  };

  std::vector<std::unique_ptr<allocator_node[]>> alloc;
  int count;

public:
  json_allocator(size_t capacity) : capacity(capacity), count(0) {
    alloc.push_back(std::unique_ptr<allocator_node[]>(
        std::move(new allocator_node[capacity])));
    count = 0;
  }

  ~json_allocator() {
    for (int i = 0; i < alloc.size() - 1; i++)
      for (int j = 0; j < capacity; j++)
        alloc[i][j].rep()->type::~type();
    for (int j = 0; j < count; j++)
      alloc.back()[j].rep()->type::~type();
  }

  template <typename... Args> pointer allocate(Args &&... args) {
    if (count == capacity) {
      alloc.push_back(std::unique_ptr<allocator_node[]>(
          std::move(new allocator_node[capacity])));
      count = 0;
    }
    pointer ptr = alloc.back()[count++].rep();
    new (ptr) type(std::forward<Args>(args)...);
    return ptr;
  }
};
#endif

///===-----------------------------------------------------------------------===
///
///               Json Lexer
///
///===-----------------------------------------------------------------------===

class json_lexer {
  json_token curtok;
  std::string curstr;

  long long file_size;
  long long read_size = 0;
  long long buffer_size;
  long long current_block_size = 0;
  char *buffer;
  char *pointer = nullptr;

  std::ifstream ifs;

  bool appendable = true;

public:
  json_lexer(std::string file_path, long long buffer_size = 1024 * 1024 * 32);
  ~json_lexer();

  bool next();

  inline json_token type() const;
  inline std::string str();

  const char *gbuffer() const;

  std::ifstream &stream() { return ifs; }

  long long filesize() const { return file_size; }
  long long readsize() const { return read_size; }

  long long position() const {
    return read_size - current_block_size + (pointer - buffer);
  }

private:
  void buffer_refresh();
  bool require_refresh();
  char next_ch();
  void prev();
};

///===-----------------------------------------------------------------------===
///
///               Json Parser
///
///===-----------------------------------------------------------------------===

class json_value {
  int type;

public:
  json_value(int type) : type(type) {}

  bool is_object() const { return type == 0; }
  bool is_array() const { return type == 1; }
  bool is_numeric() const { return type == 2; }
  bool is_string() const { return type == 3; }
  bool is_keyword() const { return type == 4; }

  virtual std::ostream &print(std::ostream &os, bool format = false,
                              std::string indent = "") const = 0;
};

#ifndef CONFIG_ALLOCATOR
using jvalue = std::shared_ptr<json_value>;
#else
using jvalue = json_value *;
#endif

class json_object : public json_value {
public:
  json_object() : json_value(0) {}
  std::vector<std::pair<std::string, jvalue>> keyvalue;

  virtual std::ostream &print(std::ostream &os, bool format = false,
                              std::string indent = "") const;
};

class json_array : public json_value {
public:
  json_array() : json_value(1) {}
  std::vector<jvalue> array;

  virtual std::ostream &print(std::ostream &os, bool format = false,
                              std::string indent = "") const;
};

#ifndef CONFIG_ALLOCATOR
using jobject = std::shared_ptr<json_object>;
using jarray = std::shared_ptr<json_array>;
#else
using jobject = json_object *;
using jarray = json_array *;
#endif

class json_numeric : public json_value {
public:
  json_numeric(std::string num) : json_value(2), numstr(std::move(num)) {}
  std::string numstr;

  bool is_integer = false;
  virtual std::ostream &print(std::ostream &os, bool format = false,
                              std::string indent = "") const;
};

class json_string : public json_value {
public:
  json_string(std::string str) : json_value(3), str(std::move(str)) {}
  std::string str;

  virtual std::ostream &print(std::ostream &os, bool format = false,
                              std::string indent = "") const;
};

class json_state : public json_value {
public:
  json_state(json_token token) : json_value(4), type(token) {}
  json_token type;

  virtual std::ostream &print(std::ostream &os, bool format = false,
                              std::string indent = "") const;
};

class json_parser {
  json_lexer lex;
  jvalue _entry;
  bool _skip_literal = false;
  bool _error = false;
  bool _reduce = false;

#ifdef CONFIG_ALLOCATOR
  json_allocator<json_array> jarray_pool;
  json_allocator<json_object> jobject_pool;
  json_allocator<json_string> jstring_pool;
  json_allocator<json_numeric> jnumeric_pool;
  json_allocator<json_state> jstate_pool;
#endif

public:
  json_parser(std::string file_path, size_t pool_capacity = 1024 * 256);

  bool step();
  bool &skip_literal() { return _skip_literal; }
  bool error() const { return _error; }

  long long filesize() const { return lex.filesize(); }
  long long readsize() const { return lex.readsize(); }
  long long position() const { return lex.position(); }

  jvalue entry() { return _entry; }

  bool reduce_before() { return _reduce; }
  jvalue latest_reduce() { return values.top(); }

private:
  std::stack<std::string> contents;
  std::stack<int> stack;
  std::stack<jvalue> values;
  void reduce(int code);
};

} // namespace jsonparser

#endif