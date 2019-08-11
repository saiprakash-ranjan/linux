/*
 * COPYRIGHT (C) 2014 Sony Corporation.
 *
 * hifc_msg_disp.h
 */

#ifndef SPZ_HIFC_MSG_DISP_H_
#define SPZ_HIFC_MSG_DISP_H_

#include <linux/device.h>
#include <linux/list.h>

// FIXME: word 'msg' is confusing. event structure in host side including FIFO event OR content in general buffer
// the arg type of send and recv api are not contrast

struct spz_msg_hdlr {
    struct list_head list;
    uint8_t id;
    int (*callback)(void *data, struct mesg_head *msg, size_t len);
    void *data;
};

int32_t spz_disp_reg_hdlr(const struct device *dev, struct spz_msg_hdlr *hdlr);

int32_t spz_disp_send_msg(const struct device *dev, void *msg, size_t len);

#endif /* SPZ_HIFC_MSG_DISP_H_ */
