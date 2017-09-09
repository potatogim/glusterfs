/*
   Copyright (c) 2018 Gluesys, Co., Ltd. <http://www.gluesys.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

/*
 * File   : vscan.h
 * Author : Ji-Hyeon Gim <potatogim@gluesys.com>
 */

#ifndef __VSCAN_H__
#define __VSCAN_H__

#include "glusterfs.h"
#include "logging.h"
#include "dict.h"
#include "xlator.h"
#include "defaults.h"

struct vscan_struct {
        fd_t            *fd;            /* for the fd of existing file */
        fd_t            *newfd;         /* for the newly created file */
        loc_t           loc;            /* to store the location of the
                                           existing file */
        loc_t           newloc;         /* to store the location for the new
                                           file */
        size_t          fsize;          /* for keeping the size of existing
                                           file */
        off_t           cur_offset;     /* current offset for read and write
                                           ops */
        off_t           fop_offset;     /* original offset received with the
                                           fop */
        pid_t           pid;
        char            origpath[PATH_MAX];
        char            newpath[PATH_MAX];
        int32_t         loop_count;
        gf_boolean_t    is_set_pid;
        struct iatt     preparent;
        struct iatt     postparent;
        gf_boolean_t    ctr_link_count_req;
};

typedef struct vscan_struct vscan_local_t;

struct vscan_priv {
        gf_boolean_t    state;
        char            *brick_path;
};

typedef struct vscan_priv vscan_private_t;

#endif /* __VSCAN_H__ */
