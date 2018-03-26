/*
 * This file is part of libsamsung-ipc.
 *
 * Copyright (C) 2010-2011 Joerie de Gram <j.de.gram@gmail.com>
 * Copyright (C) 2011 Simon Busch <morphis@gravedo.de>
 * Copyright (C) 2013-2014 Paul Kocialkowsk <contact@paulk.fr>
 *
 * libsamsung-ipc is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * libsamsung-ipc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libsamsung-ipc.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdbool.h>
#include <termios.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <asm/types.h>
#include <sys/utsname.h>

#include <samsung-ipc.h>

#include <ipc.h>
#include <ipc_devices.h>

int ipc_device_detect(void)
{
    char buffer[4096] = { 0 };
    struct utsname utsname;
    char *name = NULL;
    char *board_name = NULL;
    char *kernel_version = NULL;
    char *line, *p, *c;
    int index = -1;
    int fd = -1;
    size_t length;
    int rc;
    int i;

#ifdef IPC_DEVICE_NAME
    name = strdup(IPC_DEVICE_NAME);
#endif

#ifdef IPC_DEVICE_BOARD_NAME
    board_name = strdup(IPC_DEVICE_BOARD_NAME);
#else
    // Read board name from cpuinfo

    fd = open("/proc/cpuinfo", O_RDONLY);
    if (fd < 0)
        goto error;

    length = read(fd, &buffer, sizeof(buffer));

    close(fd);
    fd = -1;

    line = strtok(buffer, "\n");
    while (line != NULL) {
        if (strncmp(line, "Hardware", 9) == 9) {
            p = line + 11;
            c = p;

            while (*c != '\n' && *c != '\0') {
                *c = tolower(*c);
                c++;
            }

            *c = '\0';

            board_name = strdup(p);
            break;
        }

        line = strtok(NULL, "\n");
    }
#endif

#ifdef IPC_DEVICE_KERNEL_VERSION
    kernel_version = strdup(IPC_DEVICE_KERNEL_VERSION);
#else
    memset(&utsname, 0, sizeof(utsname));

    uname(&utsname);

    kernel_version = strdup(utsname.release);
#endif

    if (name == NULL && board_name == NULL)
        goto error;

    for (i = 0; i < (int) ipc_devices_count; i++) {
        // Eliminate index if neither name nor board name can be checked
        if (ipc_devices[i].name == NULL && ipc_devices[i].board_name == NULL)
            continue;

        // Eliminate index if the name doesn't match
        if (name != NULL && ipc_devices[i].name != NULL && strcmp(name, ipc_devices[i].name) != 0)
            continue;

        // Eliminate index if the board name doesn't match
        if (board_name != NULL && ipc_devices[i].board_name != NULL && strcmp(board_name, ipc_devices[i].board_name) != 0)
            continue;

        // Keep index but don't break yet since we may have a better match with kernel version
        index = i;

        if (kernel_version == NULL || ipc_devices[i].kernel_version == NULL)
            continue;

        if (kernel_version != NULL && ipc_devices[i].kernel_version != NULL && strcmp(kernel_version, ipc_devices[i].kernel_version) != 0)
            continue;

        // Everything matches this particular index
        break;
    }

    goto complete;

error:
    index = -1;

complete:
    if (name != NULL)
        free(name);

    if (board_name != NULL)
        free(board_name);

    if (kernel_version != NULL)
        free(kernel_version);

    if (fd >= 0)
        close(fd);

    return index;
}

struct ipc_client *ipc_client_create(int type)
{
    struct ipc_client *client = NULL;
    unsigned int device_index;
    int rc;

    if (type < 0 || type > IPC_CLIENT_TYPE_RFS)
        return NULL;

    rc = 10; //ipc_device_detect();
    if (rc < 0)
        goto error;

    device_index = (unsigned int) rc;
    if (device_index > ipc_devices_count)
        goto error;

    client = (struct ipc_client *) calloc(1, sizeof(struct ipc_client));
    client->type = type;

    switch (type) {
        case IPC_CLIENT_TYPE_RFS:
            client->ops = ipc_devices[device_index].rfs_ops;
            break;
        case IPC_CLIENT_TYPE_FMT:
            client->ops = ipc_devices[device_index].fmt_ops;
            break;
        default:
            goto error;
    }

    client->gprs_specs = ipc_devices[device_index].gprs_specs;
    client->nv_data_specs = ipc_devices[device_index].nv_data_specs;

    // Handlers can be modified
    client->handlers = (struct ipc_client_handlers *) calloc(1, sizeof(struct ipc_client_handlers));

    if (ipc_devices[device_index].handlers != NULL)
        memcpy(client->handlers, ipc_devices[device_index].handlers, sizeof(struct ipc_client_handlers));

    goto complete;

error:
    if (client != NULL) {
        free(client);
        client = NULL;
    }

complete:
    return client;
}

int ipc_client_destroy(struct ipc_client *client)
{
    if (client == NULL)
        return -1;

    if (client->handlers != NULL)
        free(client->handlers);

    memset(client, 0, sizeof(struct ipc_client));
    free(client);

    return 0;
}

int ipc_client_transport_handlers_register(struct ipc_client *client,
    int (*open)(void *transport_data, int type),
    int (*close)(void *transport_data),
    int (*read)(void *transport_data, void *data, size_t size),
    int (*write)(void *transport_data, const void *data, size_t size),
    int (*poll)(void *transport_data, struct ipc_poll_fds *fds, struct timeval *timeout),
    void *transport_data)
{
    if (client == NULL || client->handlers == NULL)
        return -1;

    if (read != NULL)
        client->handlers->read = read;
    if (write != NULL)
        client->handlers->write = write;
    if (poll != NULL)
        client->handlers->poll = poll;
    if (open != NULL)
        client->handlers->open = open;
    if (close != NULL)
        client->handlers->close = close;
    if (transport_data != NULL)
        client->handlers->transport_data = transport_data;

    return 0;
}

int ipc_client_power_handlers_register(struct ipc_client *client,
    int (*power_on)(void *power_data), int (*power_off)(void *power_data),
    void *power_data)
{
    if (client == NULL || client->handlers == NULL)
        return -1;

    if (power_on != NULL)
        client->handlers->power_on = power_on;
    if (power_off != NULL)
        client->handlers->power_off = power_off;
    if (power_data != NULL)
        client->handlers->power_data = power_data;

    return 0;
}

int ipc_client_gprs_handlers_register(struct ipc_client *client,
    int (*gprs_activate)(void *gprs_data, unsigned int cid),
    int (*gprs_deactivate)(void *gprs_data, unsigned int cid), void *gprs_data)
{
    if (client == NULL || client->handlers == NULL)
        return -1;

    if (gprs_activate != NULL)
        client->handlers->gprs_activate = gprs_activate;
    if (gprs_deactivate != NULL)
        client->handlers->gprs_deactivate = gprs_deactivate;
    if (gprs_data != NULL)
        client->handlers->gprs_data = gprs_data;

    return 0;
}

void ipc_client_log(struct ipc_client *client, const char *message, ...)
{
    char buffer[4096];
    va_list args;

    if (client == NULL || client->log_callback == NULL || message == NULL)
        return;

    va_start(args, message);
    vsnprintf((char *) &buffer, sizeof(buffer), message, args);
    client->log_callback(client->log_data, buffer);
    va_end(args);
}

int ipc_client_log_callback_register(struct ipc_client *client,
    void (*log_callback)(void *log_data, const char *message), void *log_data)
{
    if (client == NULL)
        return -1;

    client->log_callback = log_callback;
    client->log_data = log_data;

    return 0;
}

int ipc_client_boot(struct ipc_client *client)
{
    if (client == NULL || client->ops == NULL || client->ops->boot == NULL)
        return -1;

    return client->ops->boot(client);
}

int ipc_client_send(struct ipc_client *client, unsigned char mseq,
    unsigned short command, unsigned char type, const void *data, size_t size)
{
    struct ipc_message message;

    if (client == NULL || client->ops == NULL || client->ops->send == NULL)
        return -1;

    memset(&message, 0, sizeof(message));
    message.mseq = mseq;
    message.aseq = 0xff;
    message.command = command;
    message.type = type;
    message.data = (void *) data;
    message.size = size;

    return client->ops->send(client, &message);
}

int ipc_client_recv(struct ipc_client *client, struct ipc_message *message)
{
    if (client == NULL || client->ops == NULL || client->ops->recv == NULL || message == NULL)
        return -1;

    return client->ops->recv(client, message);
}

int ipc_client_open(struct ipc_client *client)
{
    if (client == NULL || client->handlers == NULL || client->handlers->open == NULL)
        return -1;

    return client->handlers->open(client->handlers->transport_data, client->type);
}

int ipc_client_close(struct ipc_client *client)
{
    if (client == NULL || client->handlers == NULL || client->handlers->close == NULL)
        return -1;

    return client->handlers->close(client->handlers->transport_data);
}

int ipc_client_poll(struct ipc_client *client, struct ipc_poll_fds *fds, struct timeval *timeout)
{
    if (client == NULL || client->handlers == NULL || client->handlers->poll == NULL)
        return -1;

    return client->handlers->poll(client->handlers->transport_data, fds, timeout);
}

int ipc_client_power_on(struct ipc_client *client)
{
    if (client == NULL || client->handlers == NULL || client->handlers->power_on == NULL)
        return -1;

    return client->handlers->power_on(client->handlers->power_data);
}

int ipc_client_power_off(struct ipc_client *client)
{
    if (client == NULL || client->handlers == NULL || client->handlers->power_off == NULL)
        return -1;

    return client->handlers->power_off(client->handlers->power_data);
}

int ipc_client_gprs_activate(struct ipc_client *client, unsigned int cid)
{
    if (client == NULL || client->handlers == NULL || client->handlers->gprs_activate == NULL)
        return -1;

    return client->handlers->gprs_activate(client->handlers->gprs_data, cid);
}

int ipc_client_gprs_deactivate(struct ipc_client *client, unsigned int cid)
{
    if (client == NULL || client->handlers == NULL || client->handlers->gprs_deactivate == NULL)
        return -1;

    return client->handlers->gprs_deactivate(client->handlers->gprs_data, cid);
}

int ipc_client_data_create(struct ipc_client *client)
{
    if (client == NULL || client->handlers == NULL || client->handlers->data_create == NULL)
        return -1;

    return client->handlers->data_create(&client->handlers->transport_data, &client->handlers->power_data, &client->handlers->power_data);
}

int ipc_client_data_destroy(struct ipc_client *client)
{
    if (client == NULL || client->handlers == NULL || client->handlers->data_destroy == NULL)
        return -1;

    return client->handlers->data_destroy(client->handlers->transport_data, client->handlers->power_data, client->handlers->power_data);
}

char *ipc_client_gprs_get_iface(struct ipc_client *client, unsigned int cid)
{
    if (client == NULL || client->gprs_specs == NULL || client->gprs_specs->gprs_get_iface == NULL)
        return NULL;

    return client->gprs_specs->gprs_get_iface(cid);
}

int ipc_client_gprs_get_capabilities(struct ipc_client *client,
    struct ipc_client_gprs_capabilities *capabilities)
{
    if (client == NULL || client->gprs_specs == NULL || client->gprs_specs->gprs_get_capabilities == NULL)
        return -1;

    return client->gprs_specs->gprs_get_capabilities(capabilities);
}

char *ipc_client_nv_data_path(struct ipc_client *client)
{
    if (client == NULL || client->nv_data_specs == NULL || client->nv_data_specs->nv_data_path == NULL)
        return NULL;

    return client->nv_data_specs->nv_data_path;
}

char *ipc_client_nv_data_md5_path(struct ipc_client *client)
{
    if (client == NULL || client->nv_data_specs == NULL || client->nv_data_specs->nv_data_md5_path == NULL)
        return NULL;

    return client->nv_data_specs->nv_data_md5_path;
}

char *ipc_client_nv_data_backup_path(struct ipc_client *client)
{
    if (client == NULL || client->nv_data_specs == NULL || client->nv_data_specs->nv_data_backup_path == NULL)
        return NULL;

    return client->nv_data_specs->nv_data_backup_path;
}

char *ipc_client_nv_data_backup_md5_path(struct ipc_client *client)
{
    if (client == NULL || client->nv_data_specs == NULL || client->nv_data_specs->nv_data_backup_md5_path == NULL)
        return NULL;

    return client->nv_data_specs->nv_data_backup_md5_path;
}

char *ipc_client_nv_data_secret(struct ipc_client *client)
{
    if (client == NULL || client->nv_data_specs == NULL || client->nv_data_specs->nv_data_secret == NULL)
        return NULL;

    return client->nv_data_specs->nv_data_secret;
}

size_t ipc_client_nv_data_size(struct ipc_client *client)
{
    if (client == NULL || client->nv_data_specs == NULL || client->nv_data_specs->nv_data_size == 0)
        return 0;

    return client->nv_data_specs->nv_data_size;
}

size_t ipc_client_nv_data_chunk_size(struct ipc_client *client)
{
    if (client == NULL || client->nv_data_specs == NULL || client->nv_data_specs->nv_data_chunk_size == 0)
        return 0;

    return client->nv_data_specs->nv_data_chunk_size;
}

// vim:ts=4:sw=4:expandtab
