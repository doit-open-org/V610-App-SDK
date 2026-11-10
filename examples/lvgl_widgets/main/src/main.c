/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 HiSpark community.
 */

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "hifw_config.h"
#include "lvgl.h"
#include "demos/lv_demos.h"
#include "serial_encoder.h"

#define DISPLAY_REFRESH_PERIOD_MS 33U
#define INPUT_READ_PERIOD_MS 10U
#define DEMO_DATA_PERIOD_MS 1000U
#define MAIN_LOOP_MAX_SLEEP_MS 10U
#define BASE_TIMER_CAPACITY 16U

static volatile sig_atomic_t g_running = 1;

static void HandleSignal(int signalNumber)
{
    (void)signalNumber;
    g_running = 0;
}

static int InstallSignalHandlers(void)
{
    struct sigaction action = {
        .sa_handler = HandleSignal,
    };

    if (sigemptyset(&action.sa_mask) != 0) {
        return -errno;
    }
    if (sigaction(SIGINT, &action, NULL) != 0 ||
        sigaction(SIGTERM, &action, NULL) != 0) {
        return -errno;
    }
    return 0;
}

static uint32_t MonotonicMilliseconds(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
        return 0U;
    }
    return (uint32_t)((uint64_t)now.tv_sec * 1000U +
        (uint64_t)now.tv_nsec / 1000000U);
}

static void SleepMilliseconds(uint32_t milliseconds)
{
    struct timespec delay = {
        .tv_sec = (time_t)(milliseconds / 1000U),
        .tv_nsec = (long)(milliseconds % 1000U) * 1000000L,
    };

    while (nanosleep(&delay, &delay) != 0 && errno == EINTR && g_running != 0) {
    }
}

static size_t CaptureTimers(lv_timer_t **timers, size_t capacity)
{
    lv_timer_t *timer = NULL;
    size_t count = 0U;

    while (count < capacity && (timer = lv_timer_get_next(timer)) != NULL) {
        timers[count++] = timer;
    }
    return count;
}

static int IsBaseTimer(lv_timer_t *timer, lv_timer_t *const *baseTimers,
    size_t baseTimerCount)
{
    size_t index;

    for (index = 0U; index < baseTimerCount; ++index) {
        if (baseTimers[index] == timer) {
            return 1;
        }
    }
    return 0;
}

static void OptimizeDemoForSpi(lv_display_t *display, lv_indev_t *inputDevice,
    lv_timer_t *const *baseTimers, size_t baseTimerCount)
{
    lv_timer_t *displayTimer = lv_display_get_refr_timer(display);
    lv_timer_t *inputTimer = lv_indev_get_read_timer(inputDevice);
    lv_timer_t *timer = NULL;

    lv_timer_set_period(displayTimer, DISPLAY_REFRESH_PERIOD_MS);
    lv_timer_set_period(inputTimer, INPUT_READ_PERIOD_MS);

    /* The stock widgets demo starts four always-running Analytics animations. */
    lv_anim_delete_all();

    while ((timer = lv_timer_get_next(timer)) != NULL) {
        if (!IsBaseTimer(timer, baseTimers, baseTimerCount) &&
            !lv_timer_get_paused(timer)) {
            lv_timer_set_period(timer, DEMO_DATA_PERIOD_MS);
        }
    }
}

int main(void)
{
    lv_display_t *display;
    lv_indev_t *inputDevice;
    SerialEncoder serialEncoder = {
        .fileDescriptor = -1,
    };
    bool touchInputActive;
    lv_timer_t *baseTimers[BASE_TIMER_CAPACITY];
    size_t baseTimerCount;
    int result;

    result = InstallSignalHandlers();
    if (result != 0) {
        fprintf(stderr, "lvgl_widgets: signal setup failed: %s\n",
            strerror(-result));
        return 1;
    }

    lv_init();
    lv_tick_set_cb(MonotonicMilliseconds);

    display = lv_linux_drm_create();
    if (display == NULL) {
        fprintf(stderr, "lvgl_widgets: DRM display creation failed\n");
        lv_deinit();
        return 1;
    }
    if (lv_linux_drm_set_file(display,
        CONFIG_LVGL_WIDGETS_DRM_DEVICE, -1) != LV_RESULT_OK) {
        fprintf(stderr, "lvgl_widgets: failed to open %s\n",
            CONFIG_LVGL_WIDGETS_DRM_DEVICE);
        lv_display_delete(display);
        lv_deinit();
        return 1;
    }
    lv_display_set_default(display);

    inputDevice = lv_evdev_create(LV_INDEV_TYPE_POINTER,
        CONFIG_LVGL_WIDGETS_INPUT_DEVICE);
    touchInputActive = inputDevice != NULL;
    if (touchInputActive) {
        printf("lvgl_widgets: touch input: %s\n",
            CONFIG_LVGL_WIDGETS_INPUT_DEVICE);
    } else {
        result = SerialEncoderOpen(&serialEncoder,
            CONFIG_LVGL_WIDGETS_SERIAL_DEVICE);
        if (result != 0) {
            fprintf(stderr, "lvgl_widgets: input unavailable: %s, %s\n",
                CONFIG_LVGL_WIDGETS_INPUT_DEVICE,
                CONFIG_LVGL_WIDGETS_SERIAL_DEVICE);
            lv_display_delete(display);
            lv_deinit();
            return 1;
        }
        inputDevice = SerialEncoderGetInput(&serialEncoder);
        printf("lvgl_widgets: serial input: %s\n",
            CONFIG_LVGL_WIDGETS_SERIAL_DEVICE);
        printf("keys: PgUp/Up=previous, PgDn/Down=next, "
            "Enter/Space=select, Ctrl-C=quit\n");
    }
    lv_indev_set_display(inputDevice, display);

    baseTimerCount = CaptureTimers(baseTimers, BASE_TIMER_CAPACITY);
    lv_demo_widgets();
    OptimizeDemoForSpi(display, inputDevice, baseTimers, baseTimerCount);
    while (g_running != 0) {
        uint32_t delay = lv_timer_handler();

        if (delay == LV_NO_TIMER_READY || delay > MAIN_LOOP_MAX_SLEEP_MS) {
            delay = MAIN_LOOP_MAX_SLEEP_MS;
        } else if (delay == 0U) {
            delay = 1U;
        }
        SleepMilliseconds(delay);
    }

    if (touchInputActive) {
        lv_evdev_delete(inputDevice);
    } else {
        SerialEncoderClose(&serialEncoder);
    }
    lv_display_delete(display);
    lv_deinit();
    return 0;
}
