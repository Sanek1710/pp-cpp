#pragma once

#define EMPTY
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define CAT_HELPER(x, y) x##y
#define CAT(x, y) CAT_HELPER(x, y)
