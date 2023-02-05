#pragma once

#undef print
#if _DEBUG
#include <stdio.h>
#define print printf
#else
#define print(...)
#endif
