/*
 * Copyright © 2018, 2020 Oracle and/or its affiliates.
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qapi/error.h"
#include "qemu/cutils.h"
#include "qemu/error-report.h"
#include "monitor/qdev.h"
#include "monitor/qdev.h"
#include "sysemu/sysemu.h"
#include "sysemu/runstate.h"
#include "qemu/option.h"
#include "qemu-options.h"
#include "sysemu/blockdev.h"

#include "chardev/char.h"
#include "monitor/monitor.h"
#include "qemu/config-file.h"

#include "sysemu/arch_init.h"

#include "vl.h"
#include "softmmu/remote-dev.h"

int only_migratable; /* turn it off unless user states otherwise */
bool enable_mlock;

/***********************************************************/
/* QEMU Block devices */

static const QEMUOption qemu_options[] = {
    { "h", 0, QEMU_OPTION_h, QEMU_ARCH_ALL },
#define QEMU_OPTIONS_GENERATE_OPTIONS
#include "qemu-options-wrapper.h"
    { NULL },
};

const QEMUOption *lookup_opt(int argc, char **argv,
                                    const char **poptarg, int *poptind)
{
    const QEMUOption *popt;
    int optind = *poptind;
    char *r = argv[optind];
    const char *optarg;

    loc_set_cmdline(argv, optind, 1);
    optind++;
    /* Treat --foo the same as -foo.  */
    if (r[1] == '-') {
        r++;
    }
    popt = qemu_options;
    if (!popt) {
        error_report("Not valid qemu_options");
    }
    for (;;) {
        if (!popt->name) {
            error_report("invalid option*");
            exit(1);
            popt++;
            continue;
        }
        if (!strcmp(popt->name, r + 1)) {
            break;
        }
        popt++;
    }
    if (popt->flags & HAS_ARG) {
        if (optind >= argc) {
            error_report("optind %d, argc %d", optind, argc);
            error_report("requires an argument");
            exit(1);
        }
        optarg = argv[optind++];
        loc_set_cmdline(argv, optind - 2, 2);
    } else {
        optarg = NULL;
    }

    *poptarg = optarg;
    *poptind = optind;

    return popt;
}

int drive_init_func(void *opaque, QemuOpts *opts, Error **errp)
{
    BlockInterfaceType *block_default_type = opaque;

    if (!drive_new(opts, *block_default_type, errp)) {
        error_report_err(*errp);
    }

    return 0;
}

#if defined(CONFIG_MPQEMU)
int rdevice_init_func(void *opaque, QemuOpts *opts, Error **errp)
{
    Object *obj;
    RemoteDev *rdev;

    const char *rid = qemu_opt_get(opts, "remote-device");
    if (!rid) {

        fprintf(stderr, "Cant find rid\n");
        return -EINVAL;
    }

    obj = object_resolve_path_component(
        object_get_objects_root(), rid);

    rdev = REMOTE_DEV(obj);
    if (!rdev) {
        error_setg(errp, "Could not get proxy object");
        return -EINVAL;
    }

    fprintf(stderr, "qemu: Created new remote dev object!\n");
    if (!rdev->create_proxy) {
        error_setg(errp, "Object method create_proxy is not set");
        return -EINVAL;
    }

    rdev->proxy = rdev->create_proxy(obj, errp);
    if (!rdev->proxy) {
        error_setg(errp, "qdev_remote_add failed for device.");
        return -EINVAL;
    }
    qdev_remote_add(opts, rdev->proxy, errp);
    
    //qemu_opt_del(opts, "remote-device");
    return 0;
}
#endif

int device_init_func(void *opaque, QemuOpts *opts, Error **errp)
{
    DeviceState *dev;

#if defined(CONFIG_MPQEMU)
    const char *drive_name;

    drive_name = qemu_opt_get(opts, "driver");
    fprintf(stderr, "drive name is %s\n", drive_name); 
    if (strcmp(drive_name, "remote-pci-dev") == 0) {
        if (!qemu_opt_get(opts, "remote-device")) {
            fprintf(stderr, "no remote-device id specified\n");
            return -EINVAL;
        }

        rdevice_init_func(opaque, opts, errp);
        return 0;
    }
#endif

    dev = qdev_device_add(opts, errp);
    if (!dev) {
        return -1;
    }
    object_unref(OBJECT(dev));
    return 0;
}
