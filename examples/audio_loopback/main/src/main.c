/*
 * Play the microphone back through the speaker with the media engine.
 *
 *   audio_loopback [seconds] [mic channel]
 *
 * Captures 16 kHz mono PCM from IN1 and writes it straight to the audio
 * output. AEC is off, otherwise it would cancel the played-back sound.
 * The mic channel is 0 (ADCL, default) or 1 (ADCR). Prints the input peak
 * once per second.
 *
 * Run it from a login shell: the MPP libraries rely on the environment set
 * up by /etc/profile.d/sdk_runtime.sh, and sdk_server must be running.
 */
#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "media_engine/media.h"

static volatile sig_atomic_t g_stop;

static void on_signal(int signo)
{
    (void)signo;
    g_stop = 1;
}

static double now_seconds(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char **argv)
{
    int seconds = argc > 1 ? atoi(argv[1]) : 10;
    unsigned int channel = argc > 2 ? (unsigned int)strtoul(argv[2], NULL, 0) : 0;
    media_config config;
    media_context *media = NULL;
    media_status status;
    int16_t samples[160];
    unsigned int frames = 0;
    int peak = 0;
    double start;
    double report;
    int result = 1;

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    media_config_init(&config);
    media_audio_input_config_init(&config.audio_input);
    media_audio_output_config_init(&config.audio_output);
    config.audio_input.physical_channel = channel;
    config.audio_input.enable_aec = false;

    status = media_engine_open(&config, &media);
    if (status != MEDIA_OK) {
        fprintf(stderr, "media_engine_open: %s\n", media_status_string(status));
        return 1;
    }
    status = media_audio_output_open(media);
    if (status != MEDIA_OK) {
        fprintf(stderr, "media_audio_output_open: %s\n", media_status_string(status));
        goto out;
    }
    status = media_audio_input_open(media);
    if (status != MEDIA_OK) {
        fprintf(stderr, "media_audio_input_open: %s\n", media_status_string(status));
        goto close_output;
    }
    printf("loopback %d s, mic channel %u -> speaker, Ctrl-C to stop\n", seconds, channel);
    fflush(stdout);

    start = now_seconds();
    report = start + 1.0;
    while (!g_stop && now_seconds() - start < (double)seconds) {
        size_t count;
        uint64_t pts;

        status = media_audio_input_read(media, samples, 160, 1000, &count, &pts);
        if (status == MEDIA_ERR_TIMEOUT || status == MEDIA_AGAIN) {
            continue;
        }
        if (status != MEDIA_OK) {
            fprintf(stderr, "media_audio_input_read: %s\n", media_status_string(status));
            break;
        }
        status = media_audio_output_write(media, samples, count, 1000);
        if (status != MEDIA_OK) {
            fprintf(stderr, "media_audio_output_write: %s\n", media_status_string(status));
            break;
        }
        frames++;
        for (size_t i = 0; i < count; i++) {
            int value = abs(samples[i]);

            if (value > peak) {
                peak = value;
            }
        }
        if (now_seconds() >= report) {
            printf("frames %u, peak %d\n", frames, peak);
            fflush(stdout);
            peak = 0;
            report += 1.0;
        }
    }

    result = frames > 0 ? 0 : 1;
    media_audio_input_close(media);
close_output:
    media_audio_output_close(media);
out:
    media_engine_close(media);
    return result;
}
