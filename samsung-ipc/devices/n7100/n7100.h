/*
 * This file is part of libsamsung-ipc.
 *
 * Copyright (C) 2013-2014 Paul Kocialkowski <contact@paulk.fr>
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

#ifndef __N7100_H__
#define __N7100_H__

#define N7100_MODEM_IMAGE_SIZE                                  0x1000000
#define N7100_PSI_OFFSET                                        0x1000
#define N7100_PSI_SIZE                                          0xE000
#define N7100_EBL_OFFSET                                        0xF000
#define N7100_EBL_SIZE                                          0x19000
#define N7100_SEC_START_OFFSET                                  0x9FF800
#define N7100_SEC_START_SIZE                                    0x800
#define N7100_FIRMWARE_OFFSET                                   0x28000
#define N7100_FIRMWARE_SIZE                                     0x9D7800
#define N7100_NV_DATA_OFFSET                                    0xA00000
#define N7100_NV_DATA_SIZE                                      0x200000

#define N7100_MODEM_IMAGE_DEVICE                "/dev/disk/by-partlabel/RADIO"

struct n7100_transport_data {
    int fd;
};

extern struct ipc_client_ops n7100_fmt_ops;
extern struct ipc_client_ops n7100_rfs_ops;
extern struct ipc_client_handlers n7100_handlers;
extern struct ipc_client_gprs_specs n7100_gprs_specs;
extern struct ipc_client_nv_data_specs n7100_nv_data_specs;

#endif

// vim:ts=4:sw=4:expandtab
