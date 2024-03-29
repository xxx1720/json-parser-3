#include "jsonparser.h"
#include <memory.h>
#include <set>
#include <sstream>


///===-----------------------------------------------------------------------===
///
///               Json Lexer
///
///===-----------------------------------------------------------------------===

jsonparser::json_lexer::json_lexer(std::string file_path, long long buffer_size)
    : ifs(file_path), curtok(json_token::none), buffer_size(buffer_size) {
  ifs.seekg(0, std::ios::end);
  file_size = ifs.tellg();
  ifs.seekg(0, std::ios::beg);

  buffer = new char[buffer_size];
  memset(buffer, 0, buffer_size);

  if (!ifs)
    throw std::runtime_error("file not found!");
}

jsonparser::json_lexer::~json_lexer() {
  delete[] buffer;
  ifs.close();
}

bool jsonparser::json_lexer::next() {
  this->curstr = std::string();
  while (true) {
    auto cur = next_ch();
    if (cur == 0) {
      curtok = json_token::eof;
      return true;
    }

    switch (cur) {
    case ' ':
    case '\r':
    case '\n':
    case '\t':
      continue;

    case ',':
      this->curtok = json_token::v_comma;
      this->curstr = std::string(cur, 1);
      break;

    case ':':
      this->curtok = json_token::v_pair;
      this->curstr = std::string(cur, 1);
      break;

    case '{':
      this->curtok = json_token::object_starts;
      this->curstr = std::string(cur, 1);
      break;

    case '}':
      this->curtok = json_token::object_ends;
      this->curstr = std::string(cur, 1);
      break;

    case '[':
      this->curtok = json_token::array_starts;
      this->curstr = std::string(cur, 1);
      break;

    case ']':
      this->curtok = json_token::array_ends;
      this->curstr = std::string(cur, 1);
      break;

    case 't':
    case 'f':
    case 'n': {
      std::stringstream ss;
      while (cur && isalpha(cur)) {
        ss << cur;
        cur = next_ch();
      }
      prev();

      auto s = ss.str();
      if (s == "true")
        this->curtok = json_token::v_true;
      else if (s == "false")
        this->curtok = json_token::v_false;
      else if (s == "nul")
        this->curtok = json_token::v_null;
      else
        return false;

      this->curstr = s;
    } break;

    case '"': {
      std::stringstream ss;

      while (cur = next_ch()) {
        if (cur == '"')
          break;
        if (cur < 0) {
          unsigned char cc = cur;
          int len = 0;
          if ((cc & 0xfc) == 0xfc) {
            len = 6;
          } else if ((cc & 0xf8) == 0xf8) {
            len = 5;
          } else if ((cc & 0xf0) == 0xf0) {
            len = 4;
          } else if ((cc & 0xe0) == 0xe0) {
            len = 3;
          } else if ((cc & 0xc0) == 0xc0) {
            len = 2;
          }
          for (; --len; cur = next_ch())
            ss << cur;
          ss << cur;
          continue;
        }
        ss << cur;
        if (cur == '\\') {
          if (!(cur = next_ch()))
            return false;
          ss << cur;
        }
      }

      curtok = json_token::v_string;
      curstr = ss.str();
    }

    break;

    default: {
      std::stringstream ss;

      if (cur == '-') {
        ss << cur;
        cur = next_ch();
      }

      // [0-9]+
      while (cur && isdigit(cur)) {
        ss << cur;
        cur = next_ch();
      }

      // [0-9]+.[0-9]+
      if (cur && cur == '.') {
        cur = next_ch();
        if (!cur || !isdigit(cur))
          return false;

        while (cur && isdigit(cur)) {
          ss << cur;
          cur = next_ch();
        }
      }

      // [0-9]+[Ee][+-]?[0-9]+
      // [0-9]+.[0-9]+[Ee][+-]?[0-9]+
      if (cur && (cur == 'E' || cur == 'e')) {
        cur = next_ch();

        if (!cur || !(cur == '+' || cur == '-' || isdigit(cur)))
          return false;

        if (cur == '+' || cur == '-') {
          ss << cur;
          cur = next_ch();
        }

        if (!cur || !isdigit(cur))
          return false;

        while (cur && isdigit(cur)) {
          ss << cur;
          cur = next_ch();
        }
      }
      prev();

      curtok = json_token::v_number;
      curstr = ss.str();
    }
    }

    return true;
  }
}

jsonparser::json_token jsonparser::json_lexer::type() const {
  return this->curtok;
}

std::string jsonparser::json_lexer::str() { return this->curstr; }

const char *jsonparser::json_lexer::gbuffer() const { return pointer; }

inline void jsonparser::json_lexer::buffer_refresh() {
  current_block_size = ifs.read(buffer, buffer_size).gcount();
  read_size += current_block_size;
  pointer = buffer;
}

inline bool jsonparser::json_lexer::require_refresh() {
  return pointer == nullptr || buffer + current_block_size == pointer;
}

char jsonparser::json_lexer::next_ch() {
  if (require_refresh()) {
    if (ifs.eof())
      return (char)0;
    buffer_refresh();
  }
  return *pointer++;
}

void jsonparser::json_lexer::prev() { pointer--; }

///===-----------------------------------------------------------------------===
///
///               Json Model
///
///===-----------------------------------------------------------------------===

std::ostream &jsonparser::json_object::print(std::ostream &os, bool format,
                                             std::string indent) const {
  if (!format)
    os << '{';
  else
    os << "{\n";
  for (auto it = keyvalue.rbegin(); it != keyvalue.rend(); ++it) {
    if (!format) {
      os << '\"' << it->first << "\":";
      it->second->print(os);
    } else {
      os << indent << "  \"" << it->first << "\": ";
      it->second->print(os, true, indent + "  ");
    }
    if (std::next(it) != keyvalue.rend()) {
      if (!format)
        os << ',';
      else
        os << ",\n";
    }
  }
  if (!format)
    os << '}';
  else
    os << '\n' << indent << "}";
  return os;
}

std::ostream &jsonparser::json_array::print(std::ostream &os, bool format,
                                            std::string indent) const {
  if (array.size() > 0) {
    if (!format)
      os << '[';
    else
      os << "[\n";
    for (auto it = array.rbegin(); it != array.rend(); ++it) {
      if (!format)
        (*it)->print(os);
      else {
        os << indent << "  ";
        (*it)->print(os, true, indent + "  ");
      }
      if (std::next(it) != array.rend()) {
        if (!format)
          os << ',';
        else
          os << ",\n";
      }
    }
    if (!format)
      os << ']';
    else
      os << '\n' << indent << "]";
  } else {
    os << "[]";
  }
  return os;
}

std::ostream &jsonparser::json_numeric::print(std::ostream &os, bool format,
                                              std::string indent) const {
  os << numstr;
  return os;
}

std::ostream &jsonparser::json_string::print(std::ostream &os, bool format,
                                             std::string indent) const {
  os << '"' << str << '"';
  return os;
}

std::ostream &jsonparser::json_state::print(std::ostream &os, bool format,
                                            std::string indent) const {
  switch (type) {
  case json_token::v_false:
    os << "false";
    break;

  case json_token::v_true:
    os << "true";
    break;

  case json_token::v_null:
    os << "null";
    break;
  }
  return os;
}

///===-----------------------------------------------------------------------===
///
///               Json Parser
///
///===-----------------------------------------------------------------------===

static int goto_table[][20] = {
    {0, 1, 3, 2, 0, 0, 0, 0, 4, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 28},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -2},
    {0, 0, 0, 0, 7, 8, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0},
    {0, 0, 16, 15, 0, 0, 11, 12, 4, 0, 0, 0, 5, 10, 17, 18, 19, 13, 14, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, -5, 0, 0, 0, 0, 0, -5},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, -7, 21, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 22, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, -3, -3, 0, 0, -3, 0, 0, 0, 0, 0, -3},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 23, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 24, 0, 0, -10, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, -12, -12, 0, 0, -12, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, -13, -13, 0, 0, -13, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, -14, -14, 0, 0, -14, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, -15, -15, 0, 0, -15, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, -16, -16, 0, 0, -16, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, -17, -17, 0, 0, -17, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, -18, -18, 0, 0, -18, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, -6, -6, 0, 0, -6, 0, 0, 0, 0, 0, -6},
    {0, 0, 0, 0, 25, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0},
    {0, 0, 16, 15, 0, 0, 0, 26, 4, 0, 0, 0, 5, 0, 17, 18, 19, 13, 14, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, -4, -4, 0, 0, -4, 0, 0, 0, 0, 0, -4},
    {0, 0, 16, 15, 0, 0, 27, 12, 4, 0, 0, 0, 5, 0, 17, 18, 19, 13, 14, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, -8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, -9, -9, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -11, 0, 0, 0, 0, 0, 0},
};

static int production[] = {1, 1, 1, 2, 3, 2, 3, 1, 3, 3,
                           1, 3, 1, 1, 1, 1, 1, 1, 1};

static int group_table[] = {0, 1, 1, 2, 2, 3, 3, 4, 4, 5,
                            6, 6, 7, 7, 7, 7, 7, 7, 7};

#define ACCEPT_INDEX 28

jsonparser::json_parser::json_parser(std::string file_path,
                                     size_t pool_capacity)
    : lex(file_path)
#ifdef CONFIG_ALLOCATOR
      ,
      jarray_pool(pool_capacity), jobject_pool(pool_capacity),
      jstring_pool(pool_capacity), jnumeric_pool(pool_capacity),
      jstate_pool(pool_capacity)
#endif
{
}

bool jsonparser::json_parser::step() {
  if (!_reduce && !lex.next())
    return false;

  _reduce = false;

  if (stack.empty())
    stack.push(0);

  bool require_reduce = false;
  int code = goto_table[stack.top()][(int)lex.type()];

  if (code == ACCEPT_INDEX) {
    // End of json format
    _entry = values.top();
    values.pop();
    return false;
  } else if (code > 0) {
    // Shift
    stack.push(code);
    contents.push(lex.str());
  } else if (code < 0) {
    // Reduce
    reduce(code);
    _reduce = true;
  } else {
    // Panic mode
    this->_error = true;
    return false;
  }

  return true;
}

void jsonparser::json_parser::reduce(int code) {
  int reduce_production = -code;

  // Reduce Stack
  for (int i = 0; i < production[reduce_production]; i++) {
    stack.pop();
  }

  stack.push(goto_table[stack.top()][group_table[reduce_production]]);

  //   0:         S' -> JSON
  //   1:       JSON -> OBJECT
  //   2:       JSON -> ARRAY
  //   3:      ARRAY -> [ ]
  //   4:      ARRAY -> [ ELEMENTS ]
  //   5:     OBJECT -> { }
  //   6:     OBJECT -> { MEMBERS }
  //   7:    MEMBERS -> PAIR
  //   8:    MEMBERS -> PAIR , MEMBERS
  //   9:       PAIR -> v_string : VALUE
  //  10:   ELEMENTS -> VALUE
  //  11:   ELEMENTS -> VALUE , ELEMENTS
  //  12:      VALUE -> v_string
  //  13:      VALUE -> v_number
  //  14:      VALUE -> OBJECT
  //  15:      VALUE -> ARRAY
  //  16:      VALUE -> true
  //  17:      VALUE -> false
  //  18:      VALUE -> null

  switch (reduce_production) {
    // case 0:
    // case 1:
    // case 2:

  case 3:
    contents.pop();
    contents.pop();
#ifndef CONFIG_ALLOCATOR
    values.push(jarray(new json_array));
#else
    values.push(jarray(jarray_pool.allocate()));
#endif
    break;

  case 4:
    contents.pop();
    contents.pop();
    break;

  case 5:
    contents.pop();
    contents.pop();
#ifndef CONFIG_ALLOCATOR
    values.push(jobject(new json_object()));
#else
    values.push(jobject(jobject_pool.allocate()));
#endif
    break;

  case 6:
    contents.pop();
    contents.pop();
    break;

  case 7: {
#ifndef CONFIG_ALLOCATOR
    auto jo = jobject(new json_object());
#else
    auto jo = jobject(jobject_pool.allocate());
#endif
    if (!(_skip_literal && values.top()->is_string()))
      jo->keyvalue.push_back({std::move(contents.top()), values.top()});
#ifndef CONFIG_ALLOCATOR
    else

      jo->keyvalue.push_back({std::move(contents.top()),
                              std::shared_ptr<json_string>(
                                  new json_string(std::move(std::string())))});
#else
    else
      jo->keyvalue.push_back({std::move(contents.top()),
                              jstring_pool.allocate(std::move(std::string()))});
#endif
    values.pop();
    values.push(jo);
    contents.pop();
  } break;

  case 8: {
    contents.pop();
    auto jo = values.top();
    values.pop();
    if (!(_skip_literal && values.top()->is_string()))
      ((json_object *)&*jo)
          ->keyvalue.push_back({std::move(contents.top()), values.top()});
#ifndef CONFIG_ALLOCATOR
    else
      ((json_object *)&*jo)
          ->keyvalue.push_back({std::move(contents.top()),
                                std::shared_ptr<json_string>(new json_string(
                                    std::move(std::string())))});
#else
    else
      ((json_object *)&*jo)
          ->keyvalue.push_back(
              {std::move(contents.top()),
               jstring_pool.allocate(std::move(std::string()))});
#endif
    values.pop();
    values.push(jo);
    contents.pop();
  } break;

  case 9:
    contents.pop();
    break;

  case 10: {
#ifndef CONFIG_ALLOCATOR
    auto ja = jarray(new json_array());
#else
    auto ja = jarray(jarray_pool.allocate());
#endif
    if (!(_skip_literal && values.top()->is_string()))
      ja->array.push_back(values.top());
    values.pop();
    values.push(ja);
  } break;

  case 11: {
    auto ja = values.top();
    values.pop();
    if (!(_skip_literal && values.top()->is_string()))
      ((json_array *)&*ja)->array.push_back(values.top());
    values.pop();
    values.push(ja);
    contents.pop();
  } break;

  case 12:
#ifndef CONFIG_ALLOCATOR
    values.push(std::shared_ptr<json_string>(new json_string(contents.top())));
#else
    values.push(jstring_pool.allocate(std::move(contents.top())));
#endif
    contents.pop();
    break;

  case 13:
#ifndef CONFIG_ALLOCATOR
  {
    auto numeric =
        std::shared_ptr<json_numeric>(new json_numeric(contents.top()));
#ifdef CONFIG_CHECK_INTEGER
    if (!contents.top().Contains('.'))
      numeric->is_integer = true;
#endif
    values.push(numeric);
  }
#else
    values.push(jnumeric_pool.allocate(std::move(contents.top())));
#endif
    contents.pop();
    break;

    // case 14:
    // case 15:

  case 16:
#ifndef CONFIG_ALLOCATOR
    values.push(std::shared_ptr<json_state>(
        new json_state(jsonparser::json_token::v_true)));
#else
    values.push(jstate_pool.allocate(jsonparser::json_token::v_true));
#endif
    contents.pop();
    break;

  case 17:
#ifndef CONFIG_ALLOCATOR
    values.push(std::shared_ptr<json_state>(
        new json_state(jsonparser::json_token::v_false)));
#else
    values.push(jstate_pool.allocate(jsonparser::json_token::v_false));
#endif
    contents.pop();
    break;

  case 18:
#ifndef CONFIG_ALLOCATOR
    values.push(std::shared_ptr<json_state>(
        new json_state(jsonparser::json_token::v_null)));
#else
    values.push(jstate_pool.allocate(jsonparser::json_token::v_null));
#endif
    contents.pop();
    break;
  }
}