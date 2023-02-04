cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(piconsole C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

pico_sdk_init()

add_subdirectory(libs/FatFS_SD/FatFs_SPI build)

add_executable(piconsole
    "src/LCD.cpp"
    "src/main.cpp"
    "src/SD.cpp"
    "src/OS.cpp"
    "src/program.cpp"
    "src/PICOnsole.cpp"
    "src/sprite.cpp"
)
target_include_directories(piconsole
    PUBLIC
        inc
)
target_compile_definitions(piconsole
    PUBLIC
        _DEBUG=1

        LCD_MODEL=PicoLCD_1_8
        LCD_CS=9
        LCD_CS_ACTIVE_POLARITY=0
        LCD_RST=12
        LCD_SCK=10
        LCD_MOSI=11
        LCD_DC=8
        LCD_BACKLIGHT=13

        SD_SPI=spi0
        SD_CS=5
        SD_CS_ACTIVE_POLARITY=0
        SD_SCK=2
        SD_MOSI=3
        SD_MISO=4
    )
target_compile_options(piconsole PUBLIC -Ofast)

pico_enable_stdio_usb(piconsole 0)
pico_enable_stdio_uart(piconsole 1)

pico_add_extra_outputs(piconsole)

target_link_libraries(piconsole pico_stdlib hardware_pwm hardware_spi hardware_dma FatFs_SPI)

