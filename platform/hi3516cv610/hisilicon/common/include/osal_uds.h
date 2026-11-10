/*
 * Copyright (c) 2026 HiSilicon (Shanghai) Technologies Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __OSAL_UDS_H__
#define __OSAL_UDS_H__

#include <sys/socket.h>
#include "ot_osal.h"

#define OSAL_UDS_SOCKET_PATH            "/run/sdk_server/osal_uds_server.sock"
#define OSAL_UDS_MAGIC                  0x4F53414C                  /* 'OSAL' */
#define OSAL_UDS_MAX_DEV_NAME_LEN       48                          /* same with OSAL_MAX_DEV_NAME_LEN */
#define OSAL_UDS_HEARTBEAT_INTERVAL     3                           /* heartbeat interval (seconds) */
#define OSAL_UDS_HEARTBEAT_TIMEOUT      15                          /* heartbeat timeout (seconds) */
#define OSAL_UDS_RETRY_MAX              3                           /* max retry times */
#define OSAL_UDS_RETRY_INTERVAL_US      500000                      /* retry interval (500ms) */
#define OSAL_UDS_RECV_TIMEOUT_SEC       10                          /* recv timeout (seconds) */

#define UDS_TEST_MAGIC_BYTE 0xEF

#define OSAL_UDS_SHM_THRESHOLD      (64 * 1024)                     /* Threshold for using shared memory */
#define OSAL_UDS_SHM_NAME           "osalshm"

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

typedef int (*osal_uds_copy_to_shm_func)(const void *data, size_t data_size, void *shm, size_t shm_size);
typedef int (*osal_uds_copy_to_usr_func)(void *data, size_t data_size, const void *shm, size_t shm_size);
int osal_uds_create_shm(size_t size);
void osal_uds_close_shm(int shmfd);
int osal_uds_copy_to_shm(int shmfd, size_t shm_size, const void *data, size_t data_size,
    osal_uds_copy_to_shm_func copy_to_shm);
int osal_uds_copy_to_usr(int shmfd, size_t shm_size, void *data, size_t data_size,
    osal_uds_copy_to_usr_func copy_to_usr);

void osal_uds_retain_shm(void);
void osal_uds_release_shm(void *shm_addr, size_t shm_size);

/* Message types */
typedef enum {
    OSAL_UDS_OPEN = 1,
    OSAL_UDS_CLOSE,
    OSAL_UDS_READ,
    OSAL_UDS_WRITE,
    OSAL_UDS_IOCTL,
    OSAL_UDS_SELECT,
    OSAL_UDS_HEARTBEAT,
    OSAL_UDS_SERVER_CLOSE = 0x80,
    OSAL_UDS_RESPONSE = 0x100,
#ifdef OSAL_UDS_TEST
    OSAL_UDS_CMD_TEST = 0x200,
    OSAL_UDS_CMD_TEST_WITH_SHM = 0x201,
#endif
    OSAL_UDS_BUFF
} osal_uds_msg_type;

/* Message header */
typedef struct {
    unsigned int magic;          /* magic number */
    osal_uds_msg_type type;      /* message type */
    unsigned int seq;            /* sequence number */
    unsigned int len;            /* data length */
    int ret;                     /* return value (for response) */
} osal_uds_msg_header;

/* Message structure (variable length) */
typedef struct {
    osal_uds_msg_header header;
    char data[0];                /* flexible array for payload */
} osal_uds_msg;

/* Open request */
typedef struct {
    char device_path[OSAL_UDS_MAX_DEV_NAME_LEN];
    int flags;
    int mode;
    pid_t pid;
} osal_uds_open_req;

/* Open response */
typedef struct {
    int virtual_fd;
} osal_uds_open_resp;

/* Read request */
typedef struct {
    int virtual_fd;
    unsigned long count;
} osal_uds_read_req;

/* Write request */
typedef struct {
    int virtual_fd;
    unsigned long count;
    char buf[0];                 /* flexible array for write data */
} osal_uds_write_req;

/* IOCTL request */
typedef struct {
    int virtual_fd;
    unsigned int cmd;
    char arg[0];
} osal_uds_ioctl_req;

/* Shared memory IOCTL request (for big data) */
typedef struct {
    int virtual_fd;
    unsigned int cmd;
    unsigned int data_size;       /* total data size */
    int shm_fd;                   /* shared memory file descriptor (-1 if not used) */
} osal_uds_ioctl_shm_req;

/* Close request */
typedef struct {
    int virtual_fd;
} osal_uds_close_req;

/* Select request */
typedef struct {
    int events;
    int nfds;
    int dev;
    long fds_bytes;
    unsigned long fds_len;
    char fds[0];    /* all fds */
} osal_uds_select_req;

/* Heartbeat request */
typedef struct {
    pid_t pid;
    unsigned int timestamp;
} osal_uds_heartbeat_req;

/* Heartbeat response */
typedef struct {
    pid_t server_pid;
} osal_uds_heartbeat_resp;

/* Select response */
typedef struct {
    int revents;
} osal_uds_select_resp;

#ifdef OSAL_UDS_TEST
typedef struct {
    unsigned int data_size;
} osal_uds_test_req;
#endif

/* Client */
/**
 * @brief Initialize UDS client.
 * @return OSAL_SUCCESS on success, OSAL_FAILURE on failure.
 */
int osal_uds_client_init(void);

/**
 * @brief Exit UDS client and release all resources.
 */
void osal_uds_client_exit(void);

/**
 * @brief Connect to UDS server.
 * @return OSAL_SUCCESS on success, OSAL_FAILURE on failure.
 */
int osal_uds_client_connect(void);

/**
 * @brief Disconnect from UDS server.
 */
void osal_uds_client_disconnect(void);

/**
 * @brief Open virtual device through UDS.
 * @param path Device path.
 * @param flag Open flags.
 * @return Virtual fd on success, -1 on failure.
 */
int osal_uds_client_open(const char *path, int flag, ...);

/**
 * @brief Read data from virtual device through UDS.
 * @param fd Virtual fd.
 * @param buf Buffer to store read data.
 * @param count Bytes to read.
 * @return Bytes read on success, -1 on failure.
 */
int osal_uds_client_read(int fd, void *buf, unsigned long count);

/**
 * @brief Write data to virtual device through UDS.
 * @param fd Virtual fd.
 * @param buf Data to write.
 * @param count Bytes to write.
 * @return Bytes written on success, -1 on failure.
 */
int osal_uds_client_write(int fd, const void *buf, unsigned long count);

/**
 * @brief IOCTL operation on virtual device through UDS.
 * @param fd Virtual fd.
 * @param cmd IOCTL command.
 * @return Result on success, -1 on failure.
 */
int osal_uds_client_ioctl(int fd, unsigned int cmd, ...);

/**
 * @brief IOCTL operation on virtual device through UDS, send data of data_size.
 * @param fd Virtual fd.
 * @param cmd IOCTL command.
 * @param data IOCTL arg.
 * @param data_size len of data.
 * @return Result on success, -1 on failure.
 */
int osal_uds_client_ioctl_ex(int fd, unsigned int cmd, void *data, size_t data_size);

/**
 * @brief IOCTL operation on virtual device through UDS, send by share memory.
 * @param fd Virtual fd.
 * @param cmd IOCTL command.
 * @param shmfd IOCTL share memory fd.
 * @param shm_size len of share memory.
 * @return Result on success, -1 on failure.
 */
int osal_uds_client_ioctl_shm(int fd, unsigned int cmd, int shmfd, size_t shm_size);

/**
 * @brief Close virtual device through UDS.
 * @param fd Virtual fd.
 * @return OSAL_SUCCESS on success, OSAL_FAILURE on failure.
 */
int osal_uds_client_close(int fd);

/**
 * @brief Select on virtual device through UDS.
 * @param nfds Max fd + 1.
 * @param readfds Read fds set.
 * @param writefds Write fds set.
 * @param exceptfds Except fds set.
 * @param timeout Timeout.
 * @return Ready fd count on success, -1 on failure.
 */
int osal_uds_client_select(int nfds, osal_dev_fd_set *readfds, osal_dev_fd_set *writefds, osal_dev_fd_set *exceptfds,
    struct timeval *timeout);

/**
 * @brief Check if client is connected to server.
 * @return 1 if connected, 0 if not.
 */
int osal_uds_client_is_connected(void);


/* Server */
/**
 * @brief Start UDS server.
 * @return OSAL_SUCCESS on success, OSAL_FAILURE on failure.
 */
int osal_uds_server_start(void);

/**
 * @brief Stop UDS server and release all resources.
 */
void osal_uds_server_stop(void);

/**
 * @brief Check if server is running.
 * @return 1 if running, 0 if not.
 */
int osal_uds_is_server_running(void);


#ifdef OSAL_UDS_TEST
int osal_uds_test(void *data, size_t data_size, int copy_to_user);
int osal_uds_test_with_shm(void *data, size_t data_size, int copy_to_user);
#endif

ssize_t osal_uds_recv_all(int sock, void *buf, size_t len);
ssize_t osal_uds_send_all(int sock, const void *buf, size_t len);
int osal_uds_sendmsg_all(int sock, struct msghdr *msg);
int osal_uds_recvmsg_all(int sock, struct msghdr *msg);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif

#endif /* __OSAL_UDS_H__ */
