/*
 * hi_system_handler.c
 *
 * HostIF Handler for Spritzer System Features
 *
 * This program is subject to copyright protection in accordance with the
 * applicable law. It must not, except where allowed by law, by any meansor
 * in any form be reproduced, distributed or lent. Moreover, no part of the
 * program may be used, viewed, printed, disassembled or otherwise interfered
 * with in any form, except where allowed by law, without the express written
 * consent of the copyright holder.
 *
 * Copyright 2016 Sony Corporation
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/stat.h>
#include "hifc.h"
#include "hifc_msg_disp.h"
#include "hi_system_message.h"

#define VER_PROC_NAME "cxd5602_version"
#define ERR_PROC_NAME "cxd5602_status"

#define STATUS_ERR "error"
#define STATUS_OK  "ok"

static struct completion ready_done[0x100];

typedef enum {
	REQ_NORMAL = 0,
	REQ_REBOOT
} monit_status_t;

static int reboot_status = REQ_NORMAL;

static struct device *hifc_dev = NULL;
char ver_buf[HI_SYS_MAX_VER_STR_LEN + 1];
static DECLARE_COMPLETION(ver_done);

#define WAIT_TIMEOUT 100 // 1sec in jiffies
static int fatal_error = 0;

int spz_is_handler_ready(uint8_t handlerId)
{
    return completion_done(&ready_done[handlerId]);
}
EXPORT_SYMBOL(spz_is_handler_ready);

int spz_wait_for_handler_ready(uint8_t handlerId, unsigned long timeout)
{
    if (spz_is_handler_ready(handlerId))
        return 1;
    return wait_for_completion_timeout(&ready_done[handlerId], timeout);
}

static void dump_msg(const char *head, HI_PayloadHeader *msg, size_t size)
{
    uint8_t *buf = (uint8_t*)msg;
    int i;
    printk(KERN_INFO "%s:\n", head);
    for (i = 0; i < size; i ++) {
        char buf2[3 * 16 + 1];
        int j;
        buf2[0] = 0;
        for (j = 0; i < size && j < 16; i ++, j ++) {
            sprintf(buf2 + strlen(buf2), " %02X", buf[i]);
        }
        printk(KERN_INFO "%s\n", buf2);
    }
}

static int handle_msg_ver(HI_PayloadHeader *msg0, size_t size)
{
    HI_SysMsgVerRet *msg = (HI_SysMsgVerRet*)msg0;

    if (size < sizeof(*msg) + 1) {
        printk(KERN_ERR "too small size %d < %d\n", size, sizeof(*msg) + 1);
        dump_msg("sys: ver", msg0, size);
        return 0;
    }

    size -= sizeof(*msg);
    if (size > HI_SYS_MAX_VER_STR_LEN + 1) {
        size = HI_SYS_MAX_VER_STR_LEN + 1;
    }
    memcpy(ver_buf, msg->version, size);
    ver_buf[HI_SYS_MAX_VER_STR_LEN] = 0;

    printk(KERN_INFO "%s size=%d ver=\"%s\"\n", __FUNCTION__, size, ver_buf);

    complete(&ver_done);
    return 1;
}

static int handle_msg_ready(HI_PayloadHeader *msg0, size_t size)
{
    HI_SysMsgReady *msg = (HI_SysMsgReady*)msg0;

    if (size < sizeof(*msg)) {
        printk(KERN_ERR "sys: ready: too small size %d < %d\n", size, sizeof(*msg));
        dump_msg("sys: ready", msg0, size);
        return 0;
    }

    printk(KERN_INFO "sys: %s hid=0x%02X ready=%d\n", __FUNCTION__, msg->handlerId, msg->ready);

    if (msg->ready) {
        complete_all(&ready_done[msg->handlerId]);
    }
    return 0;
}

static int handle_msg_err(HI_PayloadHeader *msg0, size_t size)
{
    HI_SysMsgReportError *msg = (HI_SysMsgReportError*)msg0;

    if (size < sizeof(*msg)) {
        printk(KERN_ERR "sys: error: too small size %d < %d\n", size, sizeof(*msg));
        dump_msg("sys: error", msg0, size);
        return 0;
    }

    printk(KERN_INFO "sys: %s level=%u, module=%u, code=%u\n", __FUNCTION__, msg->level, msg->module, msg->code);

    reboot_status = REQ_REBOOT;
    return 0;
}

static int handle_msg_(void *data, void *buf, size_t size)
{
    HI_PayloadHeader *msg = buf;

    printk(KERN_INFO "sys: handle_msg size=%d\n", size);

    if (size < sizeof(*msg)) {
        printk(KERN_ERR "sys: too small size %d < %d\n", size, sizeof(*msg));
        dump_msg("sys", msg, size);
        return -EINVAL;
    }
    if (size < sizeof(*msg) + msg->payloadSize) {
        printk(KERN_ERR "sys: too small size %d < %d+%d\n", size, sizeof(*msg), msg->payloadSize);
        dump_msg("sys", msg, size);
        return -EINVAL;
    }
    if (size != sizeof(*msg) + msg->payloadSize) {
        printk(KERN_WARNING "sys: size %d != %d+%d\n", size, sizeof(*msg), msg->payloadSize);
        dump_msg("sys", msg, size);
    }

    if (msg->apiId == HI_SYS_MSG_FUNC_READY) {
        return handle_msg_ready(msg, size);
    }
    if (msg->apiId == HI_SYS_MSG_FUNC_VER) {
        return handle_msg_ver(msg, size);
    }
    if (msg->apiId == HI_SYS_MSG_FUNC_REPORT_ERROR) {
        return handle_msg_err(msg, size);
    }

    printk(KERN_ERR "invalid func id 0x%02X\n", msg->apiId);
    return 0;
}
static int handle_msg(void *data, struct mesg_head *mesg, size_t size)
{
    if (mesg->mesg_type != MESG_TYPE_GEPT_MESG) {
        return -EINVAL;
    }
    return handle_msg_(data, mesg + 1, mesg->data_len);
}

static void print_version(struct seq_file *m, const char *name, int type)
{
    HI_SysMsgVerGet msg;
    int ret;

    seq_printf(m, "%s: ", name);

    if (fatal_error) {
        printk(KERN_ERR "fatal error mode. %s ignored\n", __FUNCTION__);
        seq_printf(m, "FATAL\n");
        return;
    }

    reinit_completion(&ver_done);
    memset(ver_buf, 0, sizeof(ver_buf));

    memset(&msg, 0, sizeof(msg));
    msg.head.handlerId = HI_HANDLER_ID_SYSTEM;
    msg.head.apiId = HI_SYS_MSG_FUNC_VER;
    msg.head.payloadSize = sizeof(msg) - sizeof(msg.head);
    msg.type = type;

    // TODO: split support
    spz_disp_send_msg(hifc_dev, &msg, sizeof(msg));

    ret = wait_for_completion_timeout(&ver_done, WAIT_TIMEOUT);
    if (ret <= 0) {
        fatal_error = 1;
        printk(KERN_ERR "%s: wait_for_completion failed %d\n", __FUNCTION__, ret);
        seq_printf(m, "TMOUT\n");
        return;
    }

    seq_printf(m, "%s\n", ver_buf);
}

static int ver_proc_show(struct seq_file *m, void *v)
{
    printk(KERN_INFO "%s\n", __FUNCTION__);

    print_version(m, "SBL", HI_SYS_GET_VER_SBL);
    print_version(m, "FW", HI_SYS_GET_VER_FW);
    print_version(m, "CUID0", HI_SYS_GET_VER_CUID0);
    print_version(m, "CUID1", HI_SYS_GET_VER_CUID1);
    print_version(m, "UDID0", HI_SYS_GET_VER_UDID0);
    print_version(m, "UDID1", HI_SYS_GET_VER_UDID1);

    return 0;

}

static int err_proc_show(struct seq_file *m, void *v)
{
    if (reboot_status == REQ_REBOOT) {
        seq_printf(m, "%s\n", STATUS_ERR);
    } else {
        seq_printf(m, "%s\n", STATUS_OK);
    }

    return 0;
}

static int ver_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, ver_proc_show, NULL);
}

static int err_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, err_proc_show, NULL);
}

static struct file_operations ver_proc_fops = {
    .open    = ver_proc_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};

static const struct file_operations err_proc_fops = {
    .open       = err_proc_open,
    .read       = seq_read,
    .llseek     = seq_lseek,
    .release    = single_release,
};

static struct spz_msg_hdlr handler = {
    .id = HI_HANDLER_ID_SYSTEM,
    .callback = handle_msg,
};

int spz_hi_system_init(void)
{
    struct proc_dir_entry* entry;
    int i;

    printk(KERN_INFO "%s\n", __FUNCTION__);

    for (i = 0; i < ARRAY_SIZE(ready_done); i ++) {
        init_completion(&ready_done[i]);
    }

    spz_disp_reg_hdlr(hifc_dev, &handler);

    entry = proc_create(VER_PROC_NAME, S_IRUSR, NULL, &ver_proc_fops);
    if (!entry) {
        printk(KERN_INFO "create_proc_entry %s failed\n", VER_PROC_NAME);
        return -EBUSY;
    }

    entry = proc_create(ERR_PROC_NAME, 0, NULL, &err_proc_fops);
    if (!entry) {
        printk(KERN_INFO "create_proc_entry %s failed\n", ERR_PROC_NAME);
        return -EBUSY;
    }

    return 0;
}
EXPORT_SYMBOL(spz_hi_system_init);

void spz_hi_system_exit(void)
{
    printk(KERN_INFO "%s\n", __FUNCTION__);

    remove_proc_entry(VER_PROC_NAME, NULL);

    remove_proc_entry(ERR_PROC_NAME, NULL);

}
EXPORT_SYMBOL(spz_hi_system_exit);

//module_init(spz_hi_system_init);
//module_exit(spz_hi_system_exit);

// MODULE_LICENSE("Proprietary");
MODULE_LICENSE("GPL");

