#include "interfaces/Speaker.h"
#include "OS.h"
#include "hardware/dma.h"
#include "i2s.pio.h"

static_assert(I2S_LRC == I2S_CLK + 1);

void __isr __time_critical_func(I2SSpeaker::i2s_dma_irq_handler)() {
    I2SSpeaker& speaker{ static_cast<I2SSpeaker&>(OS::get().get_speaker()) };
    if (dma_irqn_get_channel_status(0, speaker.dma_channel)) {
        dma_irqn_acknowledge_channel(0, speaker.dma_channel);
        if (speaker.dma_active)
        {
            speaker.dma_active = false;
            speaker.start_dma_transfer();
        }
    }
}

void I2SSpeaker::init()
{
    if (active)
    {
        return;
    }
    const std::uint32_t func = GPIO_FUNC_PIO0;
    gpio_set_function(I2S_DATA, GPIO_FUNC_PIO0);
    gpio_set_function(I2S_CLK, GPIO_FUNC_PIO0);
    gpio_set_function(I2S_LRC, GPIO_FUNC_PIO0);

    hard_assert(!pio_sm_is_claimed(pio0, pio_state_machine));
    pio_sm_claim(pio0, pio_state_machine);

    const std::uint32_t offset{ pio_add_program(pio0, &i2s_program) };

    i2s_program_init(pio0, pio_state_machine, offset, I2S_DATA, I2S_CLK);

    dma_channel = dma_claim_unused_channel(dma_channel);
    dma_channel_config dma_config{ dma_channel_get_default_config(dma_channel) };
    channel_config_set_dreq(&dma_config, DREQ_PIO0_TX0 + pio_state_machine);
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
    dma_channel_configure(dma_channel, &dma_config, &pio0->txf[pio_state_machine], nullptr, 0, false);
    irq_add_shared_handler(DMA_IRQ_0, i2s_dma_irq_handler, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
    dma_irqn_set_channel_enabled(0, dma_channel, true);
    irq_set_enabled(DMA_IRQ_0, true);
    pio_sm_set_enabled(pio0, pio_state_machine, true);
    dma_active = false;
    active = true;
}

void I2SSpeaker::uninit()
{
    if (!active)
    {
        return;
    }

    dma_irqn_set_channel_enabled(0, dma_channel, false);
    irq_remove_handler(DMA_IRQ_0, i2s_dma_irq_handler);
    dma_active = false;
    dma_channel_unclaim(dma_channel);
    pio_sm_set_enabled(pio0, pio_state_machine, false);
    pio_sm_unclaim(pio0, pio_state_machine);

    active = false;
}

void I2SSpeaker::update()
{
    if (!active)
    {
        return;
    }
    if (generator_callback != nullptr)
    {
        std::invoke(generator_callback, get_active_buffer());
    }
    if (!dma_active && get_active_buffer().size() > 0)
    {
        start_dma_transfer();
    }
}

void I2SSpeaker::start_dma_transfer()
{
    if (dma_active)
    {
        return;
    }
    dma_channel_config c{ dma_get_channel_config(dma_channel) };
    channel_config_set_read_increment(&c, true);
    dma_channel_set_config(dma_channel, &c, false);
    const AudioBuffer& buffer{ get_active_buffer() };
    if (buffer.size() == 0)
    {
        dma_active = false;
        return;
    }
    swap_active_buffer();
    dma_active = true;
    dma_channel_transfer_from_buffer_now(dma_channel, buffer.data(), buffer.size());
}

void I2SSpeaker::stop_dma_transfer()
{
    dma_active = false;
}
