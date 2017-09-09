/*
   Copyright (c) 2018 Gluesys. Co., Ltd. <http://www.gluesys.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

/*
 * File   : stub.c
 * Author : Ji-Hyeon Gim <potatogim@gluesys.com>
 */

#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

#include "stub.h"

int32_t
stub_engine_init (xlator_t *this)
{
	gf_log(this->name, GF_LOG_INFO, "vscan engine loaded: %s",
		VSCAN_ENGINE_NAME);

	return 0;
}
