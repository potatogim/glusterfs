/*
   Copyright (c) 2018 Gluesys. Co., Ltd. <http://www.gluesys.com>
   This file is part of GlusterFS.

   This file is licensed to you under your choice of the GNU Lesser
   General Public License, version 3 or any later version (LGPLv3 or
   later), or the GNU General Public License, version 2 (GPLv2), in all
   cases as published by the Free Software Foundation.
*/

#include "globals.h"
#include "run.h"
#include "syscall.h"
#include "glusterd.h"
#include "glusterd-utils.h"
#include "glusterd-volgen.h"
#include "glusterd-svc-mgmt.h"
#include "glusterd-vscand-svc.h"
#include "glusterd-svc-helper.h"
#include "glusterd-messages.h"

char *vscand_svc_name = "vscand";

void
glusterd_vscandsvc_build (glusterd_svc_t *svc)
{
        svc->manager = glusterd_vscandsvc_manager;
        svc->start   = glusterd_vscandsvc_start;
        svc->stop    = glusterd_svc_stop;
}

int glusterd_vscandsvc_init (glusterd_svc_t *svc)
{
        xlator_t                *this   = NULL;
        glusterd_conf_t         *priv   = NULL;
        int                      ret    = -1;

        this = THIS;
        GF_ASSERT (this);

        priv = this->private;
        GF_ASSERT (priv);

        ret = glusterd_svc_init (svc, vscand_svc_name);
        if (ret)
                goto out;

out:
        return ret;
}

static int
glusterd_vscandsvc_create_volfile (glusterd_volinfo_t *volinfo)
{
        char             filepath[PATH_MAX] = {0,};
        int              ret                = -1;
        xlator_t        *this               = NULL;
        glusterd_conf_t *priv               = NULL;

        this = THIS;
        GF_ASSERT (this);

        priv = this->private;
        GF_ASSERT (priv);

        glusterd_svc_build_volfile_path (vscand_svc_name, priv->workdir,
                                         filepath, sizeof (filepath));

        ret = glusterd_create_global_volfile (build_vscand_graph, filepath,
                                              NULL);

        if (ret) {
                gf_msg (this->name, GF_LOG_ERROR, 0,
                        GD_MSG_VOLFILE_CREATE_FAIL,
                        "Failed to create volfile");
                goto out;
        }

out:
        gf_msg_debug (this ? this->name : "glusterd", 0, "Returning %d", ret);

        return ret;
}

int
glusterd_vscandsvc_manager (glusterd_svc_t *svc, void *data, int flags)
{
        int                 ret     = 0;
        xlator_t           *this    = NULL;
        glusterd_volinfo_t *volinfo = NULL;

        this = THIS;
        GF_ASSERT (this);

        if (!svc->inited) {
                ret = glusterd_vscandsvc_init (svc);
                if (ret) {
                        gf_msg (THIS->name, GF_LOG_ERROR, 0,
                                GD_MSG_FAILED_INIT_VSCANSVC, "Failed to init "
                                "vscand service");
                        goto out;
                } else {
                        svc->inited = _gf_true;
                        gf_msg_debug (THIS->name, 0, "vscand service "
                                      "initialized");
                }
        }

        volinfo = data;

        /* If all the volumes are stopped or all shd compatible volumes
         * are stopped then stop the service if:
         * - volinfo is NULL or
         * - volinfo is present and volume is shd compatible
         * Otherwise create volfile and restart service if:
         * - volinfo is NULL or
         * - volinfo is present and volume is shd compatible
         */
        if (glusterd_are_all_volumes_stopped () ||
            glusterd_all_volumes_with_vscan_stopped ()) {
                if (!(volinfo && !glusterd_is_volume_vscan_enabled (volinfo))) {
                        ret = svc->stop (svc, SIGTERM);
                }
        } else {
                if (!(volinfo && !glusterd_is_volume_vscan_enabled (volinfo))) {
                        ret = glusterd_vscandsvc_create_volfile (volinfo);
                        if (ret)
                                goto out;

                        ret = svc->stop (svc, SIGTERM);
                        if (ret)
                                goto out;

                        ret = svc->start (svc, flags);
                        if (ret)
                                goto out;

                        gf_msg (THIS->name, GF_LOG_INFO, 0,
                                GD_MSG_PG_DEBUG,
                                "svc->conn.sockpath: %s",
                                svc->conn.sockpath);

                        ret = glusterd_conn_connect (&(svc->conn));
                        if (ret)
                                goto out;
                }
        }

out:
        if (ret)
                gf_event (EVENT_SVC_MANAGER_FAILED, "svc_name=%s", svc->name);

        gf_msg_debug (THIS->name, 0, "Returning %d", ret);

        return ret;
}

void
glusterd_svc_build_vscand_socket_filepath (glusterd_volinfo_t *volinfo,
                                          char *path, int path_len)
{
        char                    sockfilepath[PATH_MAX] = {0,};
        char                    rundir[PATH_MAX]       = {0,};

        snprintf (sockfilepath, sizeof (sockfilepath), "%s/run-%s",
                  rundir, uuid_utoa (MY_UUID));

        glusterd_set_socket_filepath (sockfilepath, path, path_len);
}

int
glusterd_vscandsvc_start (glusterd_svc_t *svc, int flags)
{
        xlator_t *this    = NULL;
        int       ret     = -1;
        dict_t   *cmdline = NULL;

        this = THIS;
        GF_ASSERT (this);

        cmdline = dict_new ();
        if (!cmdline)
                goto out;

        ret = glusterd_svc_start (svc, flags, cmdline);

out:
        if (cmdline)
                dict_unref (cmdline);

        gf_msg_debug (THIS->name, 0, "Returning %d", ret);

        return ret;
}

int
glusterd_vscandsvc_reconfigure ()
{
        int                  ret        = -1;
        xlator_t            *this       = NULL;
        glusterd_conf_t     *priv       = NULL;
        gf_boolean_t         identical  = _gf_false;
        glusterd_volinfo_t  *volinfo    = NULL;

        this = THIS;
        GF_VALIDATE_OR_GOTO (this->name, this, out);

        priv = this->private;
        GF_VALIDATE_OR_GOTO (this->name, priv, out);

        if (glusterd_all_volumes_with_vscan_stopped ())
                goto manager;

        /*
         * Check both OLD and NEW volfiles, if they are SAME by size
         * and cksum i.e. "character-by-character". If YES, then
         * NOTHING has been changed, just return.
         */
        ret = glusterd_svc_check_volfile_identical (priv->vscand_svc.name,
                                                    build_vscand_graph,
                                                    &identical);
        if (ret)
                goto out;

        if (identical) {
                ret = 0;
                goto out;
        }

        /*
         * They are not identical. Find out if the topology is changed
         * OR just the volume options. If just the options which got
         * changed, then inform the xlator to reconfigure the options.
         */
        identical = _gf_false; /* RESET the FLAG */
        ret = glusterd_svc_check_topology_identical (priv->vscand_svc.name,
                                                     build_vscand_graph,
                                                     &identical);
        if (ret)
                goto out;

        /* Topology is not changed, but just the options. But write the
         * options to vscand volfile, so that vscand will be reconfigured.
         */
        if (identical) {
                ret = glusterd_vscandsvc_create_volfile (volinfo);
                if (ret == 0) {/* Only if above PASSES */
                        ret = glusterd_fetchspec_notify (THIS);
                }
                goto out;
        }
manager:
        /*
         * vscand volfile's topology has been changed. vscand server needs
         * to be RESTARTED to ACT on the changed volfile.
         */
        ret = priv->vscand_svc.manager (&(priv->vscand_svc), NULL,
                                        PROC_START_NO_WAIT);

out:
        gf_msg_debug (this->name, 0, "Returning %d", ret);
        return ret;
}

void
glusterd_svc_build_vscand_volfile_path (glusterd_volinfo_t *volinfo,
                                  char *path, int path_len)
{
        char                workdir[PATH_MAX]   = {0,};
        glusterd_conf_t     *priv               = THIS->private;

        GLUSTERD_GET_VOLUME_DIR (workdir, volinfo, priv);

        snprintf (path, path_len, "%s/%s-vscand.vol", workdir,
                  volinfo->volname);
}

int
glusterd_svc_check_vscan_volfile_identical (char *svc_name,
                                           glusterd_volinfo_t *volinfo,
                                           gf_boolean_t *identical)
{
        char         orgvol[PATH_MAX]   = {0,};
        char         tmpvol[PATH_MAX]   = {0,};
        xlator_t    *this               = NULL;
        int          ret                = -1;
        int          need_unlink        = 0;
        int          tmp_fd             = -1;

        this = THIS;

        GF_VALIDATE_OR_GOTO (THIS->name, this, out);
        GF_VALIDATE_OR_GOTO (this->name, identical, out);

        glusterd_svc_build_vscand_volfile_path (volinfo, orgvol,
                        sizeof (orgvol));

        snprintf (tmpvol, sizeof (tmpvol), "/tmp/g%s-XXXXXX", svc_name);

        tmp_fd = mkstemp (tmpvol);
        if (tmp_fd < 0) {
                gf_msg (this->name, GF_LOG_WARNING, errno,
                        GD_MSG_FILE_OP_FAILED, "Unable to create temp file"
                        " %s:(%s)", tmpvol, strerror (errno));
                goto out;
        }

        need_unlink = 1;
        ret = build_rebalance_volfile (volinfo, tmpvol, NULL);
        if (ret)
                goto out;

        ret = glusterd_check_files_identical (orgvol, tmpvol,
                                              identical);
        if (ret)
                goto out;

out:
        if (need_unlink)
                sys_unlink (tmpvol);

        if (tmp_fd >= 0)
                sys_close (tmp_fd);

        return ret;
}
