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
    lcd.text("Initializing program...", {
      .x = 8, .y = 8,
      .color = color::white<RGB565>(),
      .background = color::black<RGB565>()
    });
    lcd.show();
    return 0;
}

piconsole_program_update
{
    constexpr static RGB565 color{ color::dark_grey<RGB565>() };
    LCD_MODEL &lcd{ os.get_lcd() };
    lcd.fill(color);
    lcd.text("Input Test", {
      .x = 4, .y = 32,
      .padding_x = 16, .padding_y = 4,
      .color = color::white<RGB565>(),
      .background = color::black<RGB565>()
    });
    const InputMap& input{ os.get_input() };
    if (input.get_button_state(Button::A))
    {
       lcd.filled_rectangle(color::green<RGB565>(), 80, 96, 8, 8); 
    }
    else
    {
       lcd.rectangle(color::green<RGB565>(), 80, 96, 8, 8); 
    }
    if (input.get_button_state(Button::B))
    {
       lcd.filled_rectangle(color::red<RGB565>(), 96, 80, 8, 8); 
    }
    else
    {
       lcd.rectangle(color::red<RGB565>(), 96, 80, 8, 8); 
    }
    if (input.get_button_state(Button::X))
    {
       lcd.filled_rectangle(color::blue<RGB565>(), 64, 80, 8, 8); 
    }
    else
    {
       lcd.rectangle(color::blue<RGB565>(), 64, 80, 8, 8); 
    }
    if (input.get_button_state(Button::Y))
    {
       lcd.filled_rectangle(color::yellow<RGB565>(), 80, 64, 8, 8); 
    }
    else
    {
       lcd.rectangle(color::yellow<RGB565>(), 80, 64, 8, 8); 
    }
    if (input.get_button_state(Button::DPad_Down))
    {
       lcd.filled_rectangle(color::grey<RGB565>(), 32, 96, 8, 8); 
    }
    else
    {
       lcd.rectangle(color::grey<RGB565>(), 32, 96, 8, 8); 
    }
    if (input.get_button_state(Button::DPad_Right))
    {
       lcd.filled_rectangle(color::grey<RGB565>(), 48, 80, 8, 8); 
    }
    else
    {
       lcd.rectangle(color::grey<RGB565>(), 48, 80, 8, 8); 
    }
    if (input.get_button_state(Button::DPad_Left))
    {
       lcd.filled_rectangle(color::grey<RGB565>(), 16, 80, 8, 8); 
    }
    else
    {
       lcd.rectangle(color::grey<RGB565>(), 16, 80, 8, 8); 
    }
    if (input.get_button_state(Button::DPad_Up))
    {
       lcd.filled_rectangle(color::grey<RGB565>(), 32, 64, 8, 8); 
    }
    else
    {
       lcd.rectangle(color::grey<RGB565>(), 32, 64, 8, 8); 
    }
    if (input.get_button_state(Button::Start))
    {
       lcd.filled_rectangle(color::black<RGB565>(), 56, 48, 8, 8); 
    }
    else
    {
       lcd.rectangle(color::black<RGB565>(), 56, 48, 8, 8); 
    }
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
