/*
 * Copyright 2019, Oracle and/or its affiliates.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef QEMU_PROXY_H
#define QEMU_PROXY_H

#include <linux/kvm.h>

#include "io/mpqemu-link.h"
#include "hw/proxy/memory-sync.h"
#include "qemu/event_notifier.h"
#include "hw/pci/pci.h"
#include "block/qdict.h"

#define TYPE_PCI_PROXY_DEV "pci-proxy-dev"

#define PCI_PROXY_DEV(obj) \
            OBJECT_CHECK(PCIProxyDev, (obj), TYPE_PCI_PROXY_DEV)

#define PCI_PROXY_DEV_CLASS(klass) \
            OBJECT_CLASS_CHECK(PCIProxyDevClass, (klass), TYPE_PCI_PROXY_DEV)

#define PCI_PROXY_DEV_GET_CLASS(obj) \
            OBJECT_GET_CLASS(PCIProxyDevClass, (obj), TYPE_PCI_PROXY_DEV)

typedef struct PCIProxyDev PCIProxyDev;

typedef struct ProxyMemoryRegion {
    PCIProxyDev *dev;
    MemoryRegion mr;
    bool memory;
    bool present;
    uint8_t type;
} ProxyMemoryRegion;

extern const MemoryRegionOps proxy_default_ops;

struct PCIProxyDev {
    PCIDevice parent_dev;

    int n_mr_sections;
    MemoryRegionSection *mr_sections;

    MPQemuLinkState *mpqemu_link;

    RemoteMemSync *sync;
    bool mem_init;
    struct kvm_irqfd irqfd;

    EventNotifier intr;
    EventNotifier resample;

    pid_t remote_pid;
    int rsocket;
    int socket;
    int mmio_sock;

    char *rid;

    bool managed;
    char *dev_id;

    QLIST_ENTRY(PCIProxyDev) next;

    void (*set_proxy_sock) (PCIDevice *dev, int socket);
    int (*get_proxy_sock) (PCIDevice *dev);

    void (*set_remote_opts) (PCIDevice *dev, QDict *qdict, unsigned int cmd);
    void (*proxy_ready) (PCIDevice *dev);
    void (*init_proxy) (PCIDevice *dev, char *command, bool need_spawn, Error **errp);

    ProxyMemoryRegion region[PCI_NUM_REGIONS];
};

typedef struct PCIProxyDevClass {
    PCIDeviceClass parent_class;

    void (*realize)(PCIProxyDev *dev, Error **errp);

    char *command;
} PCIProxyDevClass;

int remote_spawn(PCIProxyDev *pdev, const char *command, Error **errp);

typedef struct PCIProxyDevList {
    QLIST_HEAD(, PCIProxyDev) devices;
} proxy_dev_list_t;

extern QemuMutex proxy_list_lock;
extern proxy_dev_list_t proxy_dev_list;

void proxy_default_bar_write(void *opaque, hwaddr addr, uint64_t val,
                             unsigned size);

uint64_t proxy_default_bar_read(void *opaque, hwaddr addr, unsigned size);

void proxy_device_reset(DeviceState *dev);

#endif /* QEMU_PROXY_H */
