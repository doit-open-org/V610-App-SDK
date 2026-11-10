/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 HiSpark community.
 */

#ifndef LVGL_WIDGETS_SERIAL_ENCODER_H
#define LVGL_WIDGETS_SERIAL_ENCODER_H

#include <stdbool.h>
#include <termios.h>

#include "lvgl.h"

typedef struct {
    int fileDescriptor;
    lv_indev_t *inputDevice;
    lv_group_t *group;
    unsigned char escapeState;
    bool releasePending;
    bool attributesSaved;
    struct termios savedAttributes;
} SerialEncoder;

int SerialEncoderOpen(SerialEncoder *encoder, const char *devicePath);
lv_indev_t *SerialEncoderGetInput(const SerialEncoder *encoder);
void SerialEncoderClose(SerialEncoder *encoder);

#endif
