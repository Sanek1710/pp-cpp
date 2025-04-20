




































(x, y)
CAT1(x, y)y
"x"
L "2"       _a;
L"2"        _a;
L"2"        _a;
"L""2"      _a;
L2          _a;

L "2"
L"2"
L"2"
"L""2"
L2


constexpr auto s ="Hello worldie ( a ) U \"some text\" wow"


    ;


constexpr inline bool is_space              (char c) {
  return c == ' '

     || '\t' <= c && c <= '\r';
}

constexpr inline bool is_digit(char c){
  switch (c) {
    case '0' ... '9':
      return true;
    default:
      break;
  }
  return false;
}