/*
   Copyright (c) 2018 Gluesys. Co., Ltd. <http://www.gluesys.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
 */

#include "common-utils.h"
#include "cli1-xdr.h"
#include "xdr-generic.h"
#include "glusterd.h"
#include "glusterd-op-sm.h"
#include "glusterd-store.h"
#include "glusterd-utils.h"
#include "glusterd-volgen.h"
#include "run.h"
#include "syscall.h"
#include "byte-order.h"
#include "compat-errno.h"
#include "glusterd-scrub-svc.h"
#include "glusterd-messages.h"

#include <sys/wait.h>
#include <dlfcn.h>

const char *gd_vscan_op_list[GF_VSCAN_OPTION_TYPE_MAX] = {
        [GF_VSCAN_OPTION_TYPE_NONE]    = "none",
        [GF_VSCAN_OPTION_TYPE_ENABLE]  = "enable",
        [GF_VSCAN_OPTION_TYPE_DISABLE] = "disable",
};

static int
glusterd_vscan_enable (glusterd_volinfo_t *volinfo, char **op_errstr)
{
        int32_t      ret    = -1;
        xlator_t    *this   = NULL;

        this = THIS;
        GF_ASSERT (this);

        GF_VALIDATE_OR_GOTO (this->name, volinfo, out);
        GF_VALIDATE_OR_GOTO (this->name, op_errstr, out);

        if (glusterd_is_volume_started (volinfo) == 0) {
                *op_errstr = gf_strdup ("Volume is stopped, start volume "
                                        "to enable vscan.");
                ret = -1;
                goto out;
        }

        ret = glusterd_is_volume_vscan_enabled (volinfo);
        if (ret) {
                *op_errstr = gf_strdup ("vscan is already enabled");
                ret = -1;
                goto out;
        }

        ret = dict_set_dynstr_with_alloc (volinfo->dict, VKEY_FEATURES_VSCAN,
                                          "on");
        if (ret) {
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        GD_MSG_DICT_SET_FAILED, "dict set failed");
                goto out;
        }

        ret = 0;

        gf_msg (this->name, GF_LOG_INFO, 0,
                GD_MSG_VSCAN_ENABLED,
                "vscan enabled");

out:
        if (ret && op_errstr && !*op_errstr)
                gf_asprintf (op_errstr, "Enabling vscan on volume %s has been "
                             "unsuccessful", volinfo->volname);

        return 0;
}

static int
glusterd_vscan_disable (glusterd_volinfo_t *volinfo, char **op_errstr)
{
        int32_t          ret    = -1;
        xlator_t        *this   = NULL;

        this = THIS;

        GF_VALIDATE_OR_GOTO ("glusterd", this, out);
        GF_VALIDATE_OR_GOTO (this->name, volinfo, out);
        GF_VALIDATE_OR_GOTO (this->name, op_errstr, out);

        ret = dict_set_dynstr_with_alloc (volinfo->dict, VKEY_FEATURES_VSCAN,
                                          "off");
        if (ret) {
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        GD_MSG_DICT_SET_FAILED,
                        "Failed to set features.vscan value");
                goto out;
        }

        gf_msg (this->name, GF_LOG_INFO, 0,
                GD_MSG_VSCAN_DISABLED,
                "vscan disabled");

        ret = 0;
out:
        if (ret && op_errstr && !*op_errstr)
                gf_asprintf (op_errstr, "Disabling vscan on volume %s has "
                             "been unsuccessful", volinfo->volname);

        return 0;
}

int
glusterd_op_stage_vscan (dict_t *dict, char **op_errstr, dict_t *rsp_dict)
{
        int                      ret                    = 0;
        char                    *volname                = NULL;
        char                    *scrub_cmd              = NULL;
        char                    *scrub_cmd_from_dict    = NULL;
        char                     msg[2048]              = {0,};
        int                      type                   = 0;
        xlator_t                *this                   = NULL;
        glusterd_conf_t         *priv                   = NULL;
        glusterd_volinfo_t      *volinfo                = NULL;

        this = THIS;
        GF_ASSERT (this);
        priv = this->private;
        GF_ASSERT (priv);

        GF_ASSERT (dict);
        GF_ASSERT (op_errstr);

        ret = dict_get_str (dict, "volname", &volname);
        if (ret) {
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        GD_MSG_DICT_GET_FAILED, "Unable to get volume name");
                goto out;
        }

        ret = glusterd_volinfo_find (volname, &volinfo);
        if (ret) {
                gf_asprintf (op_errstr, FMTSTR_CHECK_VOL_EXISTS, volname);
                goto out;
        }

        if (!glusterd_is_volume_started (volinfo)) {
                *op_errstr = gf_strdup ("Volume is stopped, start volume "
                                        "before executing vscan command.");
                ret = -1;
                goto out;
        }

        ret = dict_get_int32 (dict, "type", &type);
        if (ret) {
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        GD_MSG_DICT_GET_FAILED, "Unable to get type for "
                        "operation");

                *op_errstr = gf_strdup ("Staging stage failed for vscan "
                                        "operation.");
                goto out;
        }

        if ((GF_VSCAN_OPTION_TYPE_ENABLE != type) &&
            (glusterd_is_volume_vscan_enabled (volinfo) == 0)) {
                ret = -1;
                gf_asprintf (op_errstr, "vscan is not enabled on volume %s",
                             volname);
                goto out;
        }

 out:
        if (ret && op_errstr && *op_errstr)
                gf_msg (this->name, GF_LOG_ERROR, 0,
                        GD_MSG_OP_STAGE_VSCAN_FAIL, "%s", *op_errstr);
        gf_msg_debug (this->name, 0, "Returning %d", ret);

        return ret;
}

int
__glusterd_handle_vscan (rpcsvc_request_t *req)
{
        int32_t          ret       = -1;
        gf_cli_req       cli_req   = { {0,} };
        dict_t          *dict      = NULL;
        glusterd_op_t    cli_op    = GD_OP_VSCAN;
        char            *volname   = NULL;
        char            *scrub     = NULL;
        int32_t          type      = 0;
        char             msg[2048] = {0,};
        xlator_t        *this      = NULL;
        glusterd_conf_t *conf      = NULL;

        GF_ASSERT (req);

        this = THIS;
        GF_ASSERT (this);

        conf = this->private;
        GF_ASSERT (conf);

        ret = xdr_to_generic (req->msg[0], &cli_req, (xdrproc_t)xdr_gf_cli_req);
        if (ret < 0) {
                req->rpc_err = GARBAGE_ARGS;
                goto out;
        }

        if (cli_req.dict.dict_len) {
                /* Unserialize the dictionary */
                dict  = dict_new ();

                ret = dict_unserialize (cli_req.dict.dict_val,
                                        cli_req.dict.dict_len,
                                        &dict);
                if (ret < 0) {
                        gf_msg (this->name, GF_LOG_ERROR, 0,
                                GD_MSG_DICT_UNSERIALIZE_FAIL, "failed to "
                                "unserialize req-buffer to dictionary");
                        snprintf (msg, sizeof (msg), "Unable to decode the "
                                  "command");
                        goto out;
                } else {
                        dict->extra_stdfree = cli_req.dict.dict_val;
                }
        }

        ret = dict_get_str (dict, "volname", &volname);
        if (ret) {
                snprintf (msg, sizeof (msg), "Unable to get volume name");
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        GD_MSG_DICT_GET_FAILED, "Unable to get volume name, "
                        "while handling vscan command");
                goto out;
        }

        ret = dict_get_int32 (dict, "type", &type);
        if (ret) {
                snprintf (msg, sizeof (msg), "Unable to get type of command");
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        GD_MSG_DICT_GET_FAILED, "Unable to get type of cmd, "
                        "while handling vscan command");
                goto out;
        }

        if (conf->op_version < GD_OP_VERSION_3_7_0) {
                snprintf (msg, sizeof (msg), "Cannot execute command. The "
                          "cluster is operating at version %d. vscan command "
                          "%s is unavailable in this version", conf->op_version,
                          gd_vscan_op_list[type]);
                ret = -1;
                goto out;
        }

        ret = glusterd_op_begin_synctask (req, GD_OP_VSCAN, dict);

out:
        if (ret) {
                if (msg[0] == '\0')
                        snprintf (msg, sizeof (msg), "vscan operation failed");
                ret = glusterd_op_send_cli_response (cli_op, ret, 0, req,
                                                     dict, msg);
        }

        return ret;
}

int
glusterd_handle_vscan (rpcsvc_request_t *req)
{
        return glusterd_big_locked_handler (req, __glusterd_handle_vscan);
}

int
glusterd_op_vscan (dict_t *dict, char **op_errstr, dict_t *rsp_dict)
{
        glusterd_volinfo_t     *volinfo      = NULL;
        int32_t                 ret          = -1;
        char                   *volname      = NULL;
        int                     type         = -1;
        glusterd_conf_t        *priv         = NULL;
        xlator_t               *this         = NULL;

        GF_ASSERT (dict);
        GF_ASSERT (op_errstr);

        this = THIS;
        GF_ASSERT (this);
        priv = this->private;
        GF_ASSERT (priv);

        gf_msg (this->name, GF_LOG_INFO, 0,
                GD_MSG_OP_SUCCESS,
                "vscan operation");

        ret = dict_get_str (dict, "volname", &volname);
        if (ret) {
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        GD_MSG_DICT_GET_FAILED, "Unable to get volume name");
                goto out;
        }

        ret = glusterd_volinfo_find (volname, &volinfo);
        if (ret) {
                gf_asprintf (op_errstr, FMTSTR_CHECK_VOL_EXISTS, volname);
                goto out;
        }

        ret = dict_get_int32 (dict, "type", &type);
        if (ret) {
                gf_msg (this->name, GF_LOG_ERROR, errno,
                        GD_MSG_DICT_GET_FAILED, "Unable to get type from "
                        "dict");
                goto out;
        }

        switch (type) {
        case GF_VSCAN_OPTION_TYPE_ENABLE:
                ret = glusterd_vscan_enable (volinfo, op_errstr);
                if (ret < 0)
                        goto out;
                break;

        case GF_VSCAN_OPTION_TYPE_DISABLE:
                ret = glusterd_vscan_disable (volinfo, op_errstr);
                if (ret < 0)
                        goto out;

                break;

        default:
                gf_asprintf (op_errstr, "vscan command failed. Invalid "
                             "opcode");
                ret = -1;
                goto out;
        }

        ret = glusterd_create_volfiles_and_notify_services (volinfo);
        if (ret) {
                gf_msg (this->name, GF_LOG_ERROR, 0,
                        GD_MSG_VOLFILE_CREATE_FAIL, "Unable to re-create "
                        "volfiles");
                ret = -1;
                goto out;
        }

        ret = glusterd_store_volinfo (volinfo,
                                      GLUSTERD_VOLINFO_VER_AC_INCREMENT);
        if (ret) {
                gf_msg_debug (this->name, 0, "Failed to store volinfo for "
                        "vscan");
                goto out;
        }

out:
        return ret;
}
