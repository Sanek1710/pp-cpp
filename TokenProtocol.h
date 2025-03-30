#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

#include "Cursor.h"
#include "Token.h"

template <class T>
class TokenReader {
 public:
  Token read_token() { return impl()->read_token(); }
  bool eof() const { return impl()->eof(); }
  inline T* impl() const { return static_cast<T*>(this); }
};

template <class T>
class TokenWriter {
 public:
  void write_token(const Token& token) { return impl()->write_token(token); }
  inline T* impl() const { return static_cast<T*>(this); }
};

template <typename TokeniserT>
class TokenizerReader : public TokenReader<TokenizerReader<TokeniserT>> {
 public:
  TokenizerReader(std::string_view src) : tkz(src) {}
  Token read_token() { return token = tkz.read_token(); }
  bool eof() const { return tkz.eof(); }
  const Token& last() const { return token; }

 private:
  TokeniserT tkz;
  Token token;
};

using TokenList = std::vector<Token>;

class TokenListReader : public TokenReader<TokenList> {
 public:
  TokenListReader(const TokenList& tokens)
      : tokens(tokens), idx(0), iend(tokens.size()) {}
  TokenListReader(const TokenList& tokens, size_t ibegin)
      : tokens(tokens),
        idx(std::clamp(ibegin, 0UL, tokens.size())),
        iend(tokens.size()) {}
  TokenListReader(const TokenList& tokens, size_t ibegin, size_t iend)
      : tokens(tokens),
        idx(std::clamp(ibegin, 0UL, tokens.size())),
        iend(std::clamp(ibegin, ibegin, tokens.size())) {}

  Token read_token() {
    if (idx == iend) return Token{.tag = tag::eof};
    return tokens[idx++];
  }
  bool eof() const { return idx == iend; }

 private:
  const TokenList& tokens;
  size_t idx;
  size_t iend;
};

class TokenListWriter : public TokenReader<TokenList> {
 public:
  TokenListWriter(TokenList& tokens) : tokens(tokens) {}
  void write_token(const Token& token) { tokens.push_back(token); }
  size_t size() const { return tokens.size(); }

 private:
  TokenList& tokens;
};
