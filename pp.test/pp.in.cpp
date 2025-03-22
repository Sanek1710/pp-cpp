
#define CNSTINBOOL  constexpr\
 inline  bool     
#define farg(arg) farg(arg)
#define f() f()

#define NAMED_VA(named...)  some(named)
#define UNNAMED_VA(...)  some(__VA_ARGS__)
#define INCNAMED_VA(named...)  some(__VA_ARGS__)
#define INCUNNAMED_VA(...)  some(named)

#define STR_AFTER_TOKEN(x)   aa #x
#define TWO_WORDS()   aa bb

#define abeta(beta) alpha    ##   beta    ##    gamma

#define SELF1(x, y)  (x, y)
#define SELF2(x, y)  x, y
#define CAT1(x, y)  x##y
#define STR1(x)     #x
#define CATSTR1(x, y) x#y
#define CATSTR2(x, y) x###y 
#define CATSTR3(x, y) x ## # y
#define CATSTR4(x, y) # x ## # y
#define CATSTR5(x, y) x ## ## y

// SELF1(x, y)   f2 ($0, $1)
// CAT1(x, y)    f2 $0_$1_
// STR1(x)       f1 $0s
// CATSTR1(x, y) f2 $0 $1s
// CATSTR2(x, y) f2 $0_$1s
// CATSTR3(x, y) f2 $0_$1s
// CATSTR4(x, y) f2 $0s$1s
// CATSTR5(x, y) f2 $0_$1_

SELF1(x, y)     // (x, y)
CAT1(x, y)      // xy
STR1(x)         // "x"
CATSTR1(L,2)_a; // L "2" _a;
CATSTR2(L,2)_a; // L"2" _a;
CATSTR3(L,2)_a; // L"2" _a;
CATSTR4(L,2)_a; // "L""2" _a;
CATSTR5(L,2)_a; // L2 _a;

CATSTR1(L,2) // L "2"
CATSTR2(L,2) // L"2"
CATSTR3(L,2) // L"2"
CATSTR4(L,2) // "L""2"
CATSTR5(L,2) // L2





CNSTINBOOL is_space/*bigbigibigbigibigibgb*/(char c) {  //
  return c == ' '/*
  
  */ || '\t' <= c && c <= '\r';
}

CNSTINBOOL is_digit(char c) {
  switch (c) {
    case '0' ... '9':
      return true;
    default:
      break;
  }
  return false;
}

