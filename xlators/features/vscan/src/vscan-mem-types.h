/*
   Copyright (c) 2018 Gluesys, Co., Ltd. <http://www.gluesys.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

/*
 * File   : vscan-mem-types.h
 * Author : Ji-Hyeon Gim <potatogim@gluesys.com>
 */

#ifndef __VSCAN_MEM_TYPES_H__
#define __VSCAN_MEM_TYPES_H__

#include "mem-types.h"

enum gf_vscan_mem_types_ {
        gf_vscan_mt_vscan_private_t = gf_common_mt_end + 1,
        gf_vscan_mt_end
};
#endif

