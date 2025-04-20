#define VA(x, ...)  a + __VA_ARGS__
VA(a)
VA(a, )
VA(a, b)
VA(a, b, c, d, e, f ,g)
