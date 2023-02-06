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

int main() {
    bi_decl(bi_program_description("Basic OS for the RP2040 intended for launching games\nCreated by Bryce Dixon; https://brycedixon.dev/"));
    
    OS& os{ OS::get() };

    print("&os: 0x%x\n", &os);

    {
        const std::string file_contents{ os.get_sd().read_text_file("test01.txt") };
        print("%s", file_contents.c_str());
    }

    while(os.is_active())
    {
        os.update();
    }
}