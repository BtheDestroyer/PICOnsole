#pragma once
#include "PICOnsole_defines.h"
#include "pico/assert.h"
#include "pico/platform.h"
#include <array>
#include <span>
#include <cstdint>

using AudioSample = std::int16_t;

struct AudioFrame {
    AudioSample left, right;
};

class AudioBuffer {
public:
    GETTER std::size_t size() const
    {
        return current_size;
    }

    void push_back(AudioFrame frame)
    {
        hard_assert(!full());
        internal_buffer[end_offset] = frame;
        if (++end_offset == internal_buffer.size())
        {
            end_offset = 0;
        }
        ++current_size;
    }

    void push_back(std::span<AudioFrame> frames)
    {
        hard_assert(frames.size() + size() >= internal_buffer.size());
        for (std::size_t i{ 0 }; i < frames.size(); ++i)
        {
            internal_buffer[end_offset] = frames[i];
            if (++end_offset == internal_buffer.size())
            {
                end_offset = 0;
            }
        }
        current_size += frames.size();
    }

    GETTER AudioFrame pop_front()
    {
        hard_assert(size() > 0);
        const AudioFrame front{ internal_buffer[start_offset] };
        if (++start_offset == internal_buffer.size())
        {
            start_offset = 0;
        }
        --current_size;
        return front;
    }

    GETTER bool full() const { return size() == internal_buffer.size(); }

    void reset()
    {
        start_offset = 0;
        end_offset = 0;
        current_size = 0;
    }

    GETTER AudioFrame& front() { return internal_buffer[start_offset]; }
    GETTER const AudioFrame& front() const { return internal_buffer[start_offset]; }
    GETTER AudioFrame* data() { return internal_buffer.data(); }
    GETTER const AudioFrame* data() const { return internal_buffer.data(); }

private:
    std::array<AudioFrame, 512> internal_buffer;
    std::size_t start_offset{ 0 };
    std::size_t end_offset{ 0 };
    std::size_t current_size{ 0 };
};

// Generator callbacks are expected to add frames to the buffer until AudioBuffer::full() returns true
using audio_generator_callback_t = void(*)(AudioBuffer& buffer);

class Speaker {
public:
    GETTER PICONSOLE_MEMBER_FUNC bool is_valid() const { return false; };
    GETTER PICONSOLE_MEMBER_FUNC bool is_active() const { return false; };
    PICONSOLE_MEMBER_FUNC void init() {};
    PICONSOLE_MEMBER_FUNC void uninit() {};
    PICONSOLE_MEMBER_FUNC void update() {};
    PICONSOLE_MEMBER_FUNC void set_audio_generator(audio_generator_callback_t callback) { generator_callback = callback; };

protected:
    audio_generator_callback_t generator_callback{ nullptr };
};

class I2SSpeaker : public Speaker {
public:
    GETTER PICONSOLE_MEMBER_FUNC bool is_valid() const { return true; };
    GETTER PICONSOLE_MEMBER_FUNC bool is_active() const { return active; };

    PICONSOLE_MEMBER_FUNC void init();
    PICONSOLE_MEMBER_FUNC void uninit();
    PICONSOLE_MEMBER_FUNC void update();
    PICONSOLE_MEMBER_FUNC void start_dma_transfer();
    PICONSOLE_MEMBER_FUNC void stop_dma_transfer();

private:
    static void __isr __time_critical_func(i2s_dma_irq_handler)();
    GETTER PICONSOLE_MEMBER_FUNC AudioBuffer& get_active_buffer() { return buffers[active_buffer]; }
    GETTER PICONSOLE_MEMBER_FUNC const AudioBuffer& get_active_buffer() const { return buffers[active_buffer]; }
    PICONSOLE_MEMBER_FUNC void swap_active_buffer()
    {
        active_buffer = (active_buffer + 1) % buffers.size();
        get_active_buffer().reset();
    }

    std::uint8_t pio_state_machine{ 0 };
    std::uint8_t dma_channel{ 0 };
    std::uint32_t frequency{ 0 };
    std::array<AudioBuffer, 2> buffers;
    std::size_t active_buffer{ 0 };
    bool dma_active{ false };
    bool active{ false };
};
