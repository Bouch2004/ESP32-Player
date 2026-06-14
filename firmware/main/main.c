#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "usb_device_uac.h"
#include "driver/gpio.h"
#include "driver/touch_pad.h"

#define MINIMP3_IMPLEMENTATION
#include "minimp3.h"

static const char *TAG = "usb_mp3";

#define TOUCH_THRESHOLD  15000
#define NUM_SOUNDS       3
#define TAP_WINDOW_TICKS 25

extern const uint8_t sound1_mp3_start[] asm("_binary_sound1_mp3_start");
extern const uint8_t sound1_mp3_end[]   asm("_binary_sound1_mp3_end");
extern const uint8_t sound2_mp3_start[] asm("_binary_sound2_mp3_start");
extern const uint8_t sound2_mp3_end[]   asm("_binary_sound2_mp3_end");
extern const uint8_t sound3_mp3_start[] asm("_binary_sound3_mp3_start");
extern const uint8_t sound3_mp3_end[]   asm("_binary_sound3_mp3_end");

static int16_t *pcm_data[NUM_SOUNDS]        = {NULL, NULL, NULL};
static size_t   pcm_total_bytes[NUM_SOUNDS] = {0, 0, 0};

static volatile float pcm_play_pos_f = 0.0f;
static volatile int   current_sound  = 0;
static volatile bool  playing        = false;

static uint32_t t1_baseline = 0;
static uint32_t t2_baseline = 0;

static esp_err_t uac_device_output_cb(uint8_t *buf, size_t len, void *arg)
{
    return ESP_OK;
}

static esp_err_t uac_device_input_cb(uint8_t *buf, size_t len,
                                      size_t *bytes_read, void *arg)
{
    int16_t *out         = (int16_t *)buf;
    int      snd         = current_sound;
    int      num_frames  = (int)(len / 4);
    int      total_frames = (int)(pcm_total_bytes[snd] / 4);

    if (playing && pcm_data[snd]) {
        for (int i = 0; i < num_frames; i++) {
            int pos = (int)pcm_play_pos_f;
            if (pos < total_frames) {
                out[i * 2]     = pcm_data[snd][pos * 2];
                out[i * 2 + 1] = pcm_data[snd][pos * 2 + 1];
                pcm_play_pos_f += 1.0f;
            } else {
                out[i * 2]     = 0;
                out[i * 2 + 1] = 0;
                playing        = false;
            }
        }
    } else {
        memset(buf, 0, len);
    }

    *bytes_read = len;
    return ESP_OK;
}

static void uac_device_set_mute_cb(uint32_t mute, void *arg)    {}
static void uac_device_set_volume_cb(uint32_t volume, void *arg) {}

static void trigger_sound(int snd)
{
    current_sound  = snd;
    pcm_play_pos_f = 0.0f;
    playing        = true;
}

static void input_poll_task(void *pvParameters)
{
    static bool last_boot = false;
    static bool last_t1   = false;
    static bool last_t2   = false;

    static int pending_sound = -1;
    static int tap_timer     = 0;

    while (1) {
        bool boot_pressed = (gpio_get_level(0) == 0);

        uint32_t t1_raw = 0, t2_raw = 0;
        touch_pad_read_raw_data(TOUCH_PAD_NUM1, &t1_raw);
        touch_pad_read_raw_data(TOUCH_PAD_NUM2, &t2_raw);

        bool t1 = (t1_raw > (t1_baseline + TOUCH_THRESHOLD));
        bool t2 = (t2_raw > (t2_baseline + TOUCH_THRESHOLD));

        bool tapped[NUM_SOUNDS] = {
            boot_pressed && !last_boot,
            t1           && !last_t1,
            t2           && !last_t2
        };

        for (int btn = 0; btn < NUM_SOUNDS; btn++) {
            if (!tapped[btn]) continue;

            if (!playing) {
                trigger_sound(btn);
                pending_sound = -1;
                tap_timer     = 0;
            } else if (btn == current_sound) {
                trigger_sound(btn);
                pending_sound = -1;
                tap_timer     = 0;
            } else {
                if (pending_sound == btn && tap_timer > 0) {
                    trigger_sound(btn);
                    pending_sound = -1;
                    tap_timer     = 0;
                } else {
                    pending_sound = btn;
                    tap_timer     = TAP_WINDOW_TICKS;
                }
            }
        }

        if (tap_timer > 0) {
            tap_timer--;
            if (tap_timer == 0) {
                pending_sound = -1;
            }
        }

        last_boot = boot_pressed;
        last_t1   = t1;
        last_t2   = t2;

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void decode_one(int idx, const uint8_t *mp3_start, const uint8_t *mp3_end)
{
    static mp3dec_t     mp3d;
    mp3dec_frame_info_t info;
    static int16_t      frame_buf[MINIMP3_MAX_SAMPLES_PER_FRAME * 2];

    size_t mp3_size   = (size_t)(mp3_end - mp3_start);
    size_t alloc_size = mp3_size * 8;

    pcm_data[idx] = heap_caps_malloc(alloc_size, MALLOC_CAP_SPIRAM);
    if (!pcm_data[idx]) {
        ESP_LOGE(TAG, "PSRAM malloc failed for sound %d (need %zu bytes)", idx + 1, alloc_size);
        return;
    }

    mp3dec_init(&mp3d);

    const uint8_t *p             = mp3_start;
    int16_t       *out           = pcm_data[idx];
    pcm_total_bytes[idx]         = 0;

    while (p < mp3_end) {
        int samples = mp3dec_decode_frame(&mp3d, p, mp3_end - p, frame_buf, &info);
        if (samples > 0 && info.frame_bytes > 0) {
            size_t bytes = (size_t)samples * (size_t)info.channels * sizeof(int16_t);
            if (pcm_total_bytes[idx] + bytes > alloc_size) {
                ESP_LOGW(TAG, "Sound %d buffer full at %zu bytes — truncating", idx + 1, pcm_total_bytes[idx]);
                break;
            }
            memcpy(out, frame_buf, bytes);
            out                  += samples * info.channels;
            pcm_total_bytes[idx] += bytes;
            p                    += info.frame_bytes;
            vTaskDelay(pdMS_TO_TICKS(1));
        } else if (info.frame_bytes > 0) {
            p += info.frame_bytes;
            vTaskDelay(pdMS_TO_TICKS(1));
        } else {
            break;
        }
    }

    ESP_LOGI(TAG, "Sound %d: %zu bytes | %d Hz | %d ch",
             idx + 1, pcm_total_bytes[idx], info.hz, info.channels);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Booting MP3 USB Audio — 3 Sound Engine");

    gpio_config_t io_conf = {
        .pin_bit_mask  = (1ULL << 0),
        .mode          = GPIO_MODE_INPUT,
        .pull_up_en    = GPIO_PULLUP_ENABLE,
        .pull_down_en  = GPIO_PULLDOWN_DISABLE,
        .intr_type     = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    touch_pad_init();
    touch_pad_config(TOUCH_PAD_NUM1);
    touch_pad_config(TOUCH_PAD_NUM2);

    touch_pad_denoise_t denoise = {
        .grade     = TOUCH_PAD_DENOISE_BIT4,
        .cap_level = TOUCH_PAD_DENOISE_CAP_L4,
    };
    touch_pad_denoise_set_config(&denoise);
    touch_pad_denoise_enable();
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    touch_pad_fsm_start();

    vTaskDelay(pdMS_TO_TICKS(100));
    touch_pad_read_raw_data(TOUCH_PAD_NUM1, &t1_baseline);
    touch_pad_read_raw_data(TOUCH_PAD_NUM2, &t2_baseline);
    ESP_LOGI(TAG, "Touch baselines T1:%" PRIu32 " T2:%" PRIu32,
             t1_baseline, t2_baseline);

    decode_one(0, sound1_mp3_start, sound1_mp3_end);
    decode_one(1, sound2_mp3_start, sound2_mp3_end);
    decode_one(2, sound3_mp3_start, sound3_mp3_end);

    bool any_loaded = false;
    for (int i = 0; i < NUM_SOUNDS; i++) {
        if (pcm_data[i] && pcm_total_bytes[i] > 0) any_loaded = true;
    }
    if (!any_loaded) {
        ESP_LOGE(TAG, "No sounds decoded — halting");
        return;
    }

    xTaskCreatePinnedToCore(input_poll_task, "input_poll",
                            4096, NULL, 5, NULL, 1);

    uac_device_config_t config = {
        .output_cb     = uac_device_output_cb,
        .input_cb      = uac_device_input_cb,
        .set_mute_cb   = uac_device_set_mute_cb,
        .set_volume_cb = uac_device_set_volume_cb,
        .cb_ctx        = NULL,
    };
    ESP_ERROR_CHECK(uac_device_init(&config));
    ESP_LOGI(TAG, "Ready. BOOT=Sound1 | Touch1=Sound2 | Touch2=Sound3");
}
