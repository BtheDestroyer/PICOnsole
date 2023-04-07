/* 
    This code originates from the Getting started with Raspberry Pi Pico document
    https://datasheets.raspberrypi.org/pico/getting-started-with-pico.pdf
    CC BY-ND Raspberry Pi (Trading) Ltd
*/

#include "PICOnsole.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "hardware/dma.h"
#include "pico/binary_info.h"
#include "gfx/typefaces/ascii_5px.h"

#include <functional>

using us = std::uint64_t;
static void benchmark(std::function<void()> tested_function, std::size_t count)
{
    const std::uint64_t start{ time_us_64() };
    for (std::size_t i{0}; i < count; ++i)
    {
        std::invoke(tested_function);
    }
    const std::uint64_t end{ time_us_64() };
    const std::uint64_t delta{ end - start };
    const float average{ static_cast<float>(delta) / static_cast<float>(count) };
    print("Completed in %lluus! Average of %.0fus\n", delta, average);
}

__attribute__((section(".piconsole.os.os"))) OS os;

void audio_demo(AudioBuffer& buffer)
{
    static AudioSample i{ 0 };
    while (!buffer.full())
    {
        i += 10;
        if (i > 1000)
        {
            i -= 2000;
        }
        buffer.push_back(AudioFrame{ i, i });
    }
}

int os_main()
{
    bi_decl(bi_program_description("Basic OS for the RP2040 intended for launching games\nCreated by Bryce Dixon; https://brycedixon.dev/"));

    OS& os{ OS::get() };
    os.init();
    print("OS main()");
    print("&os: 0x%x\n", &os);
    
    os.get_lcd().text("Loading Program:\n/programs/boot.elf", {
        .x = 8, .y = 16,
        .color = color::white<RGB565>(), .background = color::black<RGB565>()
    });
    os.get_lcd().show();
    
    {
        const std::string file_contents{ os.get_sd().read_text_file("test.txt") };
        print("%s", file_contents.c_str());
    }

    os.load_program("/programs/boot.elf");

    //os.get_vibrator().start(1.0f, 500);
    //os.get_speaker().set_audio_generator(audio_demo);

    while(os.is_active())
    {
        os.update();
    }
}
