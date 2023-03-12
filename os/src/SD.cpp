#include "SD.h"
#include "debug.h"
#include "hardware/structs/scb.h"
#include "hw_config.h"
#include "sd_card.h"
#include "diskio.h"

///////////////////////////
// Hardware Configuration
static void spi_dma_isr();

static spi_t spis[] = {  // One for each SPI.
    {
        .hw_inst = SD_SPI,  // SPI component
        .miso_gpio = SD_MISO,  // GPIO number (not pin number)
        .mosi_gpio = SD_MOSI,
        .sck_gpio = SD_SCK,
        // .baud_rate = 1000 * 1000,
        //.baud_rate = 12500 * 1000,  // The limitation here is SPI slew rate.
        .baud_rate = 25 * 1000 * 1000, // Actual frequency: 20833333. Has
        // worked for me with SanDisk.        
        .set_drive_strength = true,
        .mosi_gpio_drive_strength = GPIO_DRIVE_STRENGTH_2MA,
        .sck_gpio_drive_strength = GPIO_DRIVE_STRENGTH_2MA,


        .dma_isr = spi_dma_isr
    }
};

static sd_card_t sd_cards[] = {  // One for each SD card
    {
        .pcName = "0:",           // Name used to mount device
        .spi = &spis[0],          // Pointer to the SPI driving this card
        .ss_gpio = SD_CS,             // The SPI slave select GPIO for this SD card
        .set_drive_strength = true,
        .ss_gpio_drive_strength = GPIO_DRIVE_STRENGTH_2MA,
        //.use_card_detect = false,        

        // State variables:
        .m_Status = STA_NOINIT
    }
};

size_t sd_get_num() { return count_of(sd_cards); }

sd_card_t *sd_get_by_num(size_t num) {
    if (num <= sd_get_num()) {
        return &sd_cards[num];
    } else {
        return NULL;
    }
}
size_t spi_get_num() { return count_of(spis); }

spi_t *spi_get_by_num(size_t num) {
    if (num <= sd_get_num()) {
        return &spis[num];
    } else {
        return NULL;
    }
}

static void spi_dma_isr() { spi_irq_handler(&spis[0]); }

// Hardware Configuration
///////////////////////////

extern "C" {
    extern void __unhandled_user_irq(void);
}

bool SDCard::init()
{
    if (is_initialized())
    {
        return false;
    }
    // Hack to allow OS to set IRQ handler after launching a program
    ((irq_handler_t *)scb_hw->vtor)[0x1b] = __unhandled_user_irq;
    const int sd_init_result{ sd_init(sd_get_by_num(0)) };
    if (sd_init_result & STA_NOINIT)
    {
        print("SDCard failed to init SD card driver\n");
        return false;
    }

    mount_result = f_mount(&file_system, "0:", 1);
    if (mount_result != FR_OK)
    {
        print("SDCard failed to mount file_system. Err: %d\n", mount_result);
        return false;
    }
    initialized = true;
    return true;
}

bool SDCard::uninit()
{
    if (!is_initialized())
    {
        return false;
    }
    if (is_valid())
    {
        f_unmount("0:");
    }
    sd_deinit_driver();
    initialized = false;
    return true;
}

SDCard::~SDCard()
{
    uninit();
}

FILINFO SDCard::get_file_info(const char* path) const
{
    FILINFO info;
    const FRESULT stat_result{ f_stat(path, &info) };
    if (stat_result != FR_OK)
    {
        print("SDCard failed to get_file_info for path: %s; Err: %d\n", path, stat_result);
    }
    return info;
}

FSIZE_t SDCard::get_file_size(const char* path) const
{
    return get_file_info(path).fsize;
}

std::string SDCard::read_text_file(const char* path) const
{
    std::string contents;
    read_text_file(path, contents);
    return contents;
}

bool SDCard::read_text_file(const char* path, std::string& out_contents) const
{
    FIL file_handle;
    const FRESULT open_result{ f_open(&file_handle, path, FA_READ) };
    if (open_result != FR_OK)
    {
        print("SDCard failed to f_open in read_text_file via path: %s; Err: %d\n", path, open_result);
        return false;
    }
    FSIZE_t file_size{ get_file_size(path) };
    FSIZE_t read_bytes{ 0 };
    out_contents.resize(file_size);
    while (file_size > read_bytes)
    {
        constexpr static unsigned int max_chunk_size{ std::numeric_limits<unsigned int>::max() };
        const FSIZE_t remaining_bytes{ file_size - read_bytes };
        unsigned int chunk_size{ remaining_bytes > max_chunk_size ? max_chunk_size : static_cast<unsigned int>(remaining_bytes) };
        const FRESULT read_result{ f_read(&file_handle, out_contents.data() + read_bytes, chunk_size, &chunk_size) };
        if (read_result != FR_OK)
        {
            print("SDCard failed to f_read in read_text_file via path: %s; Err: %d\n", path, read_result);
            const FRESULT close_result{ f_close(&file_handle) };
            if (close_result != FR_OK)
            {
                print("SDCard failed to f_close in read_text_file via path: %s; Err: %d\n", path, close_result);
            }
            return false;
        }
        read_bytes += chunk_size;
    }
    const FRESULT close_result{ f_close(&file_handle) };
    if (close_result != FR_OK)
    {
        print("SDCard failed to f_close in read_text_file via path: %s; Err: %d\n", path, close_result);
        return false;
    }

    return true;
}

std::vector<std::uint8_t> SDCard::read_binary_file(const char* path) const
{
    const FSIZE_t file_size{ get_file_size(path) };
    std::vector<std::uint8_t> contents(file_size);
    read_binary_file(path, {contents.begin(), contents.end()});
    return contents;
}

bool SDCard::read_binary_file(const char* path, std::span<std::uint8_t> out_buffer) const
{
    FIL file_handle;
    const FRESULT open_result{ f_open(&file_handle, path, FA_READ) };
    if (open_result != FR_OK)
    {
        print("SDCard failed to f_open in read_text_file via path: %s; Err: %d\n", path, open_result);
        return false;
    }
    const FSIZE_t file_size{ std::min<FSIZE_t>(get_file_size(path), out_buffer.size()) };
    FSIZE_t read_bytes{ 0 };
    while (file_size > read_bytes)
    {
        constexpr static unsigned int max_chunk_size{ std::numeric_limits<unsigned int>::max() };
        const FSIZE_t remaining_bytes{ file_size - read_bytes };
        unsigned int chunk_size{ remaining_bytes > max_chunk_size ? max_chunk_size : static_cast<unsigned int>(remaining_bytes) };
        const FRESULT read_result{ f_read(&file_handle, out_buffer.data() + read_bytes, chunk_size, &chunk_size) };
        if (read_result != FR_OK)
        {
            print("SDCard failed to f_read in read_text_file via path: %s; Err: %d\n", path, read_result);
            const FRESULT close_result{ f_close(&file_handle) };
            if (close_result != FR_OK)
            {
                print("SDCard failed to f_close in read_text_file via path: %s; Err: %d\n", path, close_result);
            }
            return false;
        }
        read_bytes += chunk_size;
    }
    const FRESULT close_result{ f_close(&file_handle) };
    if (close_result != FR_OK)
    {
        print("SDCard failed to f_close in read_text_file via path: %s; Err: %d\n", path, close_result);
        return false;
    }

    return true;
}

bool SDCard::write_text_file(const char* path, std::string_view contents)
{
    FIL file_handle;
    const FRESULT open_result{ f_open(&file_handle, path, FA_WRITE | FA_CREATE_ALWAYS) };
    if (open_result != FR_OK)
    {
        print("SDCard failed to f_open in write_text_file via path: %s; Err: %d\n", path, open_result);
        return false;
    }

    const FSIZE_t file_size{ contents.size() };
    FSIZE_t written_bytes{ 0 };
    while (file_size > written_bytes)
    {
        constexpr static unsigned int max_chunk_size{ std::numeric_limits<unsigned int>::max() };
        const FSIZE_t remaining_bytes{ file_size - written_bytes };
        unsigned int chunk_size{ remaining_bytes > max_chunk_size ? max_chunk_size : static_cast<unsigned int>(remaining_bytes) };
        const FRESULT write_result{ f_write(&file_handle, contents.data() + written_bytes, chunk_size, &chunk_size) };
        if (write_result != FR_OK)
        {
            print("SDCard failed to f_write in write_text_file via path: %s; Err: %d\n", path, write_result);
            const FRESULT close_result{ f_close(&file_handle) };
            if (close_result != FR_OK)
            {
                print("SDCard failed to f_close in write_text_file via path: %s; Err: %d\n", path, close_result);
            }
            return false;
        }
        written_bytes += chunk_size;
    }
    const FRESULT close_result{ f_close(&file_handle) };
    if (close_result != FR_OK)
    {
        print("SDCard failed to f_close in write_text_file via path: %s; Err: %d\n", path, close_result);
        return false;
    }

    return true;
}

bool SDCard::write_binary_file(const char* path, std::span<const std::uint8_t> buffer)
{
    FIL file_handle;
    const FRESULT open_result{ f_open(&file_handle, path, FA_WRITE | FA_CREATE_ALWAYS) };
    if (open_result != FR_OK)
    {
        print("SDCard failed to f_open in write_text_file via path: %s; Err: %d\n", path, open_result);
        return false;
    }

    const FSIZE_t file_size{ buffer.size() };
    FSIZE_t written_bytes{ 0 };
    while (file_size > written_bytes)
    {
        constexpr static unsigned int max_chunk_size{ std::numeric_limits<unsigned int>::max() };
        const FSIZE_t remaining_bytes{ file_size - written_bytes };
        unsigned int chunk_size{ remaining_bytes > max_chunk_size ? max_chunk_size : static_cast<unsigned int>(remaining_bytes) };
        const FRESULT write_result{ f_write(&file_handle, buffer.data() + written_bytes, chunk_size, &chunk_size) };
        if (write_result != FR_OK)
        {
            print("SDCard failed to f_write in write_text_file via path: %s; Err: %d\n", path, write_result);
            const FRESULT close_result{ f_close(&file_handle) };
            if (close_result != FR_OK)
            {
                print("SDCard failed to f_close in write_text_file via path: %s; Err: %d\n", path, close_result);
            }
            return false;
        }
        written_bytes += chunk_size;
    }
    const FRESULT close_result{ f_close(&file_handle) };
    if (close_result != FR_OK)
    {
        print("SDCard failed to f_close in write_text_file via path: %s; Err: %d\n", path, close_result);
        return false;
    }

    return true;
}
