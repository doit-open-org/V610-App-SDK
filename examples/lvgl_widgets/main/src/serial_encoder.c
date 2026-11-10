/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 HiSpark community.
 */

#include "serial_encoder.h"

#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static int ConfigureTerminal(SerialEncoder *encoder)
{
    struct termios attributes;

    if (tcgetattr(encoder->fileDescriptor, &attributes) != 0) {
        return -errno;
    }
    encoder->savedAttributes = attributes;
    encoder->attributesSaved = true;

    attributes.c_lflag &= (tcflag_t)~(ECHO | ICANON);
    attributes.c_cc[VMIN] = 0;
    attributes.c_cc[VTIME] = 0;
    if (tcsetattr(encoder->fileDescriptor, TCSANOW, &attributes) != 0) {
        encoder->attributesSaved = false;
        return -errno;
    }
    return 0;
}

static bool DecodeByte(SerialEncoder *encoder, unsigned char value,
    lv_indev_data_t *data)
{
    if (encoder->escapeState == 1U) {
        encoder->escapeState = value == '[' ? 2U : 0U;
        return false;
    }
    if (encoder->escapeState == 2U) {
        if (value == 'A') {
            encoder->escapeState = 0U;
            data->enc_diff = -1;
            return true;
        }
        if (value == 'B') {
            encoder->escapeState = 0U;
            data->enc_diff = 1;
            return true;
        }
        if (value == '5') {
            encoder->escapeState = 3U;
        } else if (value == '6') {
            encoder->escapeState = 4U;
        } else {
            encoder->escapeState = 0U;
        }
        return false;
    }
    if (encoder->escapeState == 3U || encoder->escapeState == 4U) {
        unsigned char direction = encoder->escapeState;

        encoder->escapeState = 0U;
        if (value == '~') {
            data->enc_diff = direction == 3U ? -1 : 1;
            return true;
        }
        return false;
    }

    switch (value) {
        case 0x1BU:
            encoder->escapeState = 1U;
            return false;
        case ' ':
        case '\r':
        case '\n':
            data->key = LV_KEY_ENTER;
            data->state = LV_INDEV_STATE_PRESSED;
            encoder->releasePending = true;
            return true;
        default:
            return false;
    }
}

static void ReadEncoder(lv_indev_t *inputDevice, lv_indev_data_t *data)
{
    SerialEncoder *encoder = lv_indev_get_user_data(inputDevice);

    data->enc_diff = 0;
    data->key = LV_KEY_ENTER;
    data->state = LV_INDEV_STATE_RELEASED;
    if (encoder->releasePending) {
        encoder->releasePending = false;
        return;
    }

    while (true) {
        unsigned char value;
        ssize_t bytesRead = read(encoder->fileDescriptor, &value, 1U);

        if (bytesRead == 1) {
            if (DecodeByte(encoder, value, data)) {
                return;
            }
            continue;
        }
        if (bytesRead < 0 && errno == EINTR) {
            continue;
        }
        return;
    }
}

int SerialEncoderOpen(SerialEncoder *encoder, const char *devicePath)
{
    int result;

    if (encoder == NULL || devicePath == NULL || devicePath[0] == '\0') {
        return -EINVAL;
    }

    (void)memset(encoder, 0, sizeof(*encoder));
    encoder->fileDescriptor = -1;
    encoder->fileDescriptor = open(devicePath,
        O_RDONLY | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (encoder->fileDescriptor < 0) {
        return -errno;
    }

    result = ConfigureTerminal(encoder);
    if (result != 0) {
        SerialEncoderClose(encoder);
        return result;
    }

    encoder->group = lv_group_create();
    encoder->inputDevice = lv_indev_create();
    if (encoder->group == NULL || encoder->inputDevice == NULL) {
        SerialEncoderClose(encoder);
        return -ENOMEM;
    }

    lv_group_set_default(encoder->group);
    lv_indev_set_type(encoder->inputDevice, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(encoder->inputDevice, ReadEncoder);
    lv_indev_set_user_data(encoder->inputDevice, encoder);
    lv_indev_set_group(encoder->inputDevice, encoder->group);
    return 0;
}

lv_indev_t *SerialEncoderGetInput(const SerialEncoder *encoder)
{
    return encoder == NULL ? NULL : encoder->inputDevice;
}

void SerialEncoderClose(SerialEncoder *encoder)
{
    if (encoder == NULL) {
        return;
    }
    if (encoder->inputDevice != NULL) {
        lv_indev_delete(encoder->inputDevice);
        encoder->inputDevice = NULL;
    }
    if (encoder->group != NULL) {
        if (lv_group_get_default() == encoder->group) {
            lv_group_set_default(NULL);
        }
        lv_group_delete(encoder->group);
        encoder->group = NULL;
    }
    if (encoder->fileDescriptor >= 0) {
        if (encoder->attributesSaved) {
            (void)tcsetattr(encoder->fileDescriptor, TCSANOW,
                &encoder->savedAttributes);
            encoder->attributesSaved = false;
        }
        (void)close(encoder->fileDescriptor);
        encoder->fileDescriptor = -1;
    }
}
