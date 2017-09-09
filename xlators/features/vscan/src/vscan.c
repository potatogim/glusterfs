/*
   Copyright (c) 2018 Gluesys. Co., Ltd. <http://www.gluesys.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

/*
 * File   : vscan.c
 * Author : Ji-Hyeon Gim <potatogim@gluesys.com>
 */

#include <dlfcn.h>

#include "vscan.h"
#include "vscan-mem-types.h"
#include "vscan-messages.h"

int32_t
init (xlator_t *this)
{
        int             ret   = -1;
        vscan_private_t *priv = NULL;
        data_t          *data = NULL;
        char            *tmp  = NULL;

        GF_VALIDATE_OR_GOTO("vscan", this, out);

        if (!this) {
                gf_msg ("vscan", GF_LOG_ERROR, 0, VSCAN_MSG_NULL_THIS,
                        "this is NULL. init() failed");
                goto out;
        }

        if (!this->parents) {
                gf_log(this->name, GF_LOG_WARNING,
                        "Dangling volume. check volfile ");
        }

        if (!this->children || this->children->next) {
                gf_log(this->name, GF_LOG_ERROR,
                        "not configured with exactly one child. exiting");
                return -1;
        }

        priv = GF_CALLOC(1, sizeof(*priv), gf_vscan_mt_vscan_private_t);

        if (!priv) {
                gf_log(this->name, GF_LOG_ERROR, "out of memory");
                ret = ENOMEM;
                goto out;
        }

        GF_OPTION_INIT("vscan", priv->state, bool, out);
        GF_OPTION_INIT("engine", tmp, str, out);

        this->local_pool = mem_pool_new(vscan_local_t, 64);

        if (!this->local_pool) {
                gf_log(this->name, GF_LOG_ERROR,
                        "failed to create local_t's memory pool");
                ret = ENOMEM;
                goto out;
        }

//        data = dict_get(this->options, "brick-path");
//
//        if (!data) {
//                gf_log(this->name, GF_LOG_ERROR,
//                        "no option specified for 'brick-path'");
//                ret = ENOMEM;
//                goto out;
//        }
//
//        priv->brick_path = gf_strdup(data->data);
//
//        if (!priv->brick_path) {
//                ret = ENOMEM;
//                gf_log(this->name, GF_LOG_DEBUG, "out of memory");
//                goto out;
//        }

        this->private = (void *)priv;
        ret = 0;

out:
        if (ret) {
                if (priv) {
                        if (priv->brick_path)
                                GF_FREE(priv->brick_path);
                        GF_FREE(priv);
                }

                mem_pool_destroy(this->local_pool);
        }

        return ret;
}

int
reconfigure (xlator_t *this, dict_t *options)
{
        int             ret   = 0;
        vscan_private_t *priv = NULL;
        char            *tmp  = NULL;

        priv = this->private;

        GF_OPTION_RECONF("vscan", priv->state, options, bool, out);
        GF_OPTION_RECONF("engine", tmp, options, str, out);

out:
        return ret;
}

void
fini (xlator_t *this)
{
        vscan_private_t *priv = NULL;

        GF_VALIDATE_OR_GOTO("vscan", this, out);

        priv = this->private;

        if (priv) {
                if (priv->brick_path)
                        GF_FREE(priv->brick_path);
                GF_FREE(priv);
        }

        mem_pool_destroy(this->local_pool);

        this->private = NULL;

out:
        return;
}

int32_t
mem_acct_init (xlator_t *this)
{
        int32_t ret = -1;

        if (!this)
                return ret;

        ret = xlator_mem_acct_init(this, gf_vscan_mt_end + 1);

        if (ret != 0) {
                gf_msg(this->name, GF_LOG_WARNING, 0, VSCAN_MSG_MEM_ACNT_FAILED,
                        "Memory accounting init failed");
                return ret;
        }

        return ret;
}

int32_t
vscan_open (call_frame_t *frame, xlator_t *this,
            loc_t *loc, int flags, fd_t *fd, dict_t *xdata)
{
        int32_t         op_ret   = 0;
        int32_t         op_errno = 0;
        vscan_private_t *priv    = NULL;

        priv = this->private;
        GF_VALIDATE_OR_GOTO("vscan", priv, out);

        gf_msg (this->name, GF_LOG_INFO, 0,
                VSCAN_MSG_FD_OPEN_FAILED,
                "vscan_open()");

out:
        return 0;
}

int32_t
vscan_create (call_frame_t *frame, xlator_t *this,
              loc_t *loc, int flags, mode_t mode,
              mode_t umask, fd_t *fd, dict_t *xdata)
{
        int32_t         op_ret   = 0;
        int32_t         op_errno = 0;
        vscan_private_t *priv    = NULL;

        priv = this->private;
        GF_VALIDATE_OR_GOTO("vscan", priv, out);

        gf_msg (this->name, GF_LOG_INFO, 0,
                VSCAN_MSG_FD_CREATE_FAILED,
                "vscan_create()");

out:
        return 0;
}

struct xlator_fops fops = {
        .open   = vscan_open,
        .create = vscan_create,
};

struct xlator_cbks cbks = {
};

struct volume_options options[] = {
        {
                .key           = { "vscan" },
                .type          = GF_OPTION_TYPE_BOOL,
                .default_value = "off",
                .description   = "Enable/disable vscan translator"
        },
        {
                .key           = { "engine" },
                .type          = GF_OPTION_TYPE_STR,
                .default_value = "clamav",
                .description   = "Antivirus engine",
                .value         =
                {
                        "clamav",
                        "kaspersky",
                        "fprot",
                        "f-secure"
                }
        },
        {
                .key = { NULL }
        },
};
