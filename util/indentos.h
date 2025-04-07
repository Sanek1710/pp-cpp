#pragma once

#include <iostream>

class indentos : public std::streambuf {
  std::streambuf* rdbuf;
  bool newline = false;
  std::ostream& os;
  inline static const std::string_view indent = "  ";

 protected:
  virtual int overflow(int ch) {
    if (newline && ch != '\n') rdbuf->sputn(indent.data(), indent.size());
    newline = ch == '\n';
    return rdbuf->sputc(ch);
  }

 public:
  explicit indentos(std::ostream& os, bool newline = true)
      : rdbuf(os.rdbuf()), newline(newline), os(os) {
    os.rdbuf(this);
  }
  virtual ~indentos() { os.rdbuf(rdbuf); }
};
