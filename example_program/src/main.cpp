#include "PICOnsole.h"

#ifdef __cplusplus
extern "C" {
#endif

void piconsole_program_init()
{
    puts("Good job :D");
    OS::get().get_lcd().fill(color::cyan<RGB565>());
}

void piconsole_program_update()
{
    puts("We updating!");
    OS::get().get_lcd().fill(color::green<RGB565>());
}

#ifdef __cplusplus
}
#endif

int main()
{
    puts("Don't run this directly! Launch it from PICOnsole OS!");
}
