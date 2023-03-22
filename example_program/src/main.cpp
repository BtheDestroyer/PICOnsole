#include "PICOnsole.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"

#ifdef __cplusplus
extern "C" {
#endif

piconsole_program_init
{
    constexpr static RGB565 color{ color::cyan<RGB565>() };
    LCD_MODEL &lcd{ os.get_lcd() };
    lcd.fill(color);
    lcd.show();
    return 0;
}

piconsole_program_update
{
    constexpr static RGB565 color{ color::green<RGB565>() };
    LCD_MODEL &lcd{ os.get_lcd() };
    lcd.fill(color);
    lcd.show();
}

#ifdef __cplusplus
}
#endif

int __attribute__((section(".piconsole.program.main"))) main()
{
    multicore_fifo_push_blocking(FIFOCodes::program_launch_success);
    OS& os{ OS::get() };
    _piconsole_program_init(os);
    while (true)
    {
        const std::uint32_t fifo{ multicore_fifo_pop_blocking() };
        if (fifo == FIFOCodes::os_updated)
        {
            _piconsole_program_update(os);
        }
    }
}
