/*
 * Record H.264 from the GC4023 camera with the media engine.
 *
 *   camera_record [seconds] [output file] [mipi]
 *
 * Captures 1280x720 at 25 fps, writes the elementary stream to the output
 * file and saves one JPEG next to it. mipi selects the Cove-610 camera
 * connector: 1 (default, I2C0) or 2 (I2C1).
 *
 * Run it from a login shell: the MPP libraries rely on the environment set
 * up by /etc/profile.d/sdk_runtime.sh, and sdk_server must be running.
 */
#define _POSIX_C_SOURCE 200809L

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    int seconds = argc > 1 ? atoi(argv[1]) : 5;
    const char *path = argc > 2 ? argv[2] : "/tmp/camera.h264";
    int mipi = argc > 3 ? atoi(argv[3]) : 1;
    char jpeg_path[512];
    media_config config;
    media_context *media = NULL;
    media_status status;
    FILE *file;
    size_t bytes = 0;
    unsigned int packets = 0;
    unsigned int key_frames = 0;
    double start;
    int result = 1;

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    media_config_init(&config);
    media_video_config_init(&config.video);
    if (mipi != 1 && mipi != 2) {
        fprintf(stderr, "mipi must be 1 or 2\n");
        return 1;
    }
    /* The defaults of cameras[1] are MIPI-2: connector 1 on I2C1. */
    if (mipi == 2) {
        config.video.cameras[0] = config.video.cameras[1];
    }
    config.video.streams[0].output = MEDIA_VIDEO_OUTPUT_H264;
    config.video.streams[0].width = 1280;
    config.video.streams[0].height = 720;
    config.video.streams[0].frame_rate = 25;
    config.video.streams[0].bit_rate_kbps = 2000;
    config.video.streams[0].gop = 50;

    file = fopen(path, "wb");
    if (file == NULL) {
        perror(path);
        return 1;
    }

    status = media_engine_open(&config, &media);
    if (status != MEDIA_OK) {
        fprintf(stderr, "media_engine_open: %s\n", media_status_string(status));
        fclose(file);
        return 1;
    }
    status = media_video_open(media);
    if (status != MEDIA_OK) {
        fprintf(stderr, "media_video_open: %s\n", media_status_string(status));
        goto out;
    }
    printf("recording %d s of 1280x720 H.264 from MIPI-%d to %s\n", seconds, mipi, path);
    fflush(stdout);

    start = now_seconds();
    while (!g_stop && now_seconds() - start < (double)seconds) {
        media_video_packet packet;

        status = media_video_read_packet(media, 0, 1000, &packet);
        if (status == MEDIA_ERR_TIMEOUT || status == MEDIA_AGAIN) {
            continue;
        }
        if (status != MEDIA_OK) {
            fprintf(stderr, "media_video_read_packet: %s\n", media_status_string(status));
            break;
        }
        if (fwrite(packet.data, 1, packet.size, file) != packet.size) {
            perror("fwrite");
            media_video_packet_release(&packet);
            break;
        }
        bytes += packet.size;
        packets++;
        key_frames += packet.key_frame ? 1U : 0U;
        media_video_packet_release(&packet);
    }

    {
        double elapsed = now_seconds() - start;

        printf("%u packets (%u key frames), %zu bytes in %.1f s: %.1f fps, %.0f kbit/s\n",
               packets, key_frames, bytes, elapsed,
               elapsed > 0.0 ? packets / elapsed : 0.0,
               elapsed > 0.0 ? bytes * 8.0 / 1000.0 / elapsed : 0.0);
    }

    snprintf(jpeg_path, sizeof(jpeg_path), "%s.jpg", path);
    status = media_video_save_jpeg(media, 0, jpeg_path, 2000);
    printf("snapshot %s: %s\n", jpeg_path, media_status_string(status));

    result = packets > 0 ? 0 : 1;
    media_video_close(media);
out:
    media_engine_close(media);
    fclose(file);
    return result;
}
