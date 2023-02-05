#include "PICOnsole.h"

#ifdef __cplusplus
extern "C" {
#endif

void init_program(OS* os)
{
    puts("Good job :D");
}

#ifdef __cplusplus
}
#endif

int main()
{
    puts("Don't run this directly! Launch it from PICOnsole OS!");
}
