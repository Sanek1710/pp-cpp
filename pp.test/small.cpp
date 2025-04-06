#define CATH(a, b) a##b
#define CAT(a, b) CATH(a, b)

CAT(CAT(a, b), c)
