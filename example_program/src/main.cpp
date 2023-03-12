#include "PICOnsole.h"
#include "pico/stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif

piconsole_program_init
{
    puts("Good job :D");
    constexpr static RGB565 color{ color::cyan<RGB565>() };
    LCD_MODEL &lcd{ os.get_lcd() };
    puts("Filling LCD");
    lcd.fill(color);
    puts("Showing LCD");
    lcd.show();
    puts("All done");
    return 0;
}

piconsole_program_update
{
    puts("We updating!");
    constexpr static RGB565 color{ color::green<RGB565>() };
    LCD_MODEL &lcd{ os.get_lcd() };
    lcd.fill(color);
    lcd.show();
}

#ifdef __cplusplus
}
#endif

int main()
{
    puts("pre-stdio_init_all");
    stdio_init_all();
    puts("program main()");
    OS& os{ OS::get() };
    if (os.is_initialized())
    {
        puts("OS already initialized!");
        os.uninit(false);
    }
    os.init();
    puts("_piconsole_program_init(os)");
    _piconsole_program_init(os);
    while (true)
    {
        _piconsole_program_update(os);
        os.update();
    }
}
