/*
 * lfs port functions
 *
 * Copyright (c) 2017, Arm Limited. All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef LFS_PORT_H
#define LFS_PORT_H
#include "lfs.h"

extern  lfs_t g_lfs_t;

int lfs_init_function(void);
int lfs_test(void);
#endif


