
#define SQLITE_ATOMIC_INTRINSICS 1

#define CTIMEOPT_VAL_(opt) #opt
#define CTIMEOPT_VAL(opt) CTIMEOPT_VAL_(opt)

#ifdef SQLITE_ATOMIC_INTRINSICS
"ATOMIC_INTRINSICS=" CTIMEOPT_VAL(SQLITE_ATOMIC_INTRINSICS),
#endif

#define CAT1(x, y) x###y

CAT1(L, CAT1(y, z))

#define cb 12
#define M1(x, y) fromM1(x + y, x ## y)
#define M2(x, y) fromM2(x + y, x ## y)
#define CM2(x, y) fromCM2(#x, #y)

fromM1(C + fromM2(c + b, cb)
//         |

M1(C, M2(c, b))
//2 4         e
//      8 a  d

#define OPEN (
#define CLOSE )
#define MACRO(arg) #arg
#define CHAIN(a, b, c, d) a b c d
CHAIN(MACRO, OPEN, c, CLOSE)
  