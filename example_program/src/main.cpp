#include "PICOnsole.h"

#ifdef __cplusplus
extern "C" {
#endif

piconsole_program_init
{
    puts("Good job :D");
    OS::get().get_lcd().fill(color::cyan<RGB565>());
    return true;
}

piconsole_program_update
{
    puts("We updating!");
    OS::get().get_lcd().fill(color::green<RGB565>());
}

#ifdef __cplusplus
}
#endif
