cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(piconsole)

pico_sdk_init()

add_executable(piconsole
    "src/LCD.cpp"
    "src/main.cpp"
    "src/OS.cpp"
    "src/path.cpp"
    "src/program.cpp"
    "src/PICOnsole.cpp"
    "src/sprite.cpp"
)
target_include_directories(piconsole
    PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/inc"                          
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
    )
set_property(TARGET piconsole PROPERTY CXX_STANDARD 20)
target_compile_options(piconsole PUBLIC -Ofast)

pico_enable_stdio_usb(piconsole 0)
pico_enable_stdio_uart(piconsole 1)

pico_add_extra_outputs(piconsole)

target_link_libraries(piconsole pico_stdlib hardware_pwm hardware_spi hardware_dma)

