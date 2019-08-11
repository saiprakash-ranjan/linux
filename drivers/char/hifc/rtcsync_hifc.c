/* COPYRIGHT (C) 2014 Sony Corporation.
*
* rtcsync_hifc.c
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/completion.h>
#include <linux/kfifo.h>

#include "hifc.h"
#include "hifc_msg_disp.h"
#include "hi_rtcsync_message.h"

static struct device *hifc_dev = NULL;
struct RtcGet_task {
    RtcGetResult *result;
    struct completion done;
};
struct RtcSet_task {
    RtcSetResult *result;
    struct completion done;
};

static DEFINE_KFIFO(RtcGet_list, struct RtcGet_task *, 16);
static DEFINE_KFIFO(RtcSet_list, struct RtcSet_task *, 8);
static DEFINE_SPINLOCK(recv_list_lock);

void ReceiveRtcGetResult(RtcGetCommand *command, RtcGetResult *result)
{
    struct RtcGet_task task;
    char sendbuf[sizeof(RtcGetCommand)+1];

    task.result = result;
    init_completion(&task.done);
    spin_lock(&recv_list_lock);
//#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,0)
//    kfifo_put(&RtcGet_list, &task);
//#else
    struct RtcGet_task *task_ptr = &task;
    kfifo_put(&RtcGet_list, &task_ptr);
//#endif

    spin_unlock(&recv_list_lock);

    sendbuf[0] = HI_HANDLER_ID_RTCSYNC;
    memcpy(&sendbuf[1], command, sizeof(RtcGetCommand));
    spz_disp_send_msg(hifc_dev, sendbuf, sizeof(RtcGetCommand)+1);

    wait_for_completion_interruptible(&task.done);

    // FIXME: remove task if failed. kfifo doesn't support cancel...
}

void ReceiveRtcSetResult(RtcSetCommand *command, RtcSetResult *result)
{

    struct RtcSet_task task;
    char sendbuf[sizeof(RtcSetCommand)+1];

    task.result = result;
    init_completion(&task.done);
    spin_lock(&recv_list_lock);
//#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,13,0)
//    kfifo_put(&RtcSet_list, &task);
//#else
    struct RtcSet_task *task_ptr = &task;
    kfifo_put(&RtcSet_list, &task_ptr);
//#endif

    spin_unlock(&recv_list_lock);

    sendbuf[0] = HI_HANDLER_ID_RTCSYNC;
    memcpy(&sendbuf[1], command, sizeof(RtcSetCommand));
    spz_disp_send_msg(hifc_dev, sendbuf, sizeof(RtcSetCommand)+1);

    wait_for_completion_interruptible(&task.done);

    // FIXME: remove task if failed. kfifo doesn't support cancel...

}

static void handle_msg_RtcGet_result(RtcHeader *msg0, size_t size)
{
    RtcGetResult *msg = (RtcGetResult*)msg0;
    struct RtcGet_task *task = NULL;
    int len, ret;

    if (size != sizeof(RtcGetResult)) {
        printk(KERN_ERR "Recv size error=%d\n", size);
        return;
    }

    len = msg->header.paramLength;
    if (len != sizeof(RtcGetResult) - sizeof(RtcHeader)) {
        printk(KERN_ERR "Payload length error=%d\n", len);
        return;
    }

    spin_lock(&recv_list_lock);
    ret = kfifo_get(&RtcGet_list, &task);
    spin_unlock(&recv_list_lock);

    if (!ret || !task) {
        printk(KERN_ERR "no recv request but got result\n");
        return;
    }

    if (task->result == NULL) {
        printk(KERN_ERR "handle_msg_RtcGet_result:task->result=NULL error!\n");
        return;
    }

    memcpy(task->result, &msg->header, size);
    if (&task->done == NULL) {
        printk(KERN_ERR "handle_msg_RtcGet_result:task->done NULL error!\n");
        return;
    }
    complete(&task->done);

}

static void handle_msg_RtcSet_result(RtcHeader *msg0, size_t size)
{

    RtcSetResult *msg = (RtcSetResult*)msg0;
    struct RtcSet_task *task = NULL;
    int len, ret;

    if (size != sizeof(RtcSetResult)) {
        printk(KERN_ERR "Recv size error=%d\n", size);
        return;
    }

    len = msg->header.paramLength;
    if (len != sizeof(RtcSetResult) - sizeof(RtcHeader)) {
        printk(KERN_ERR "Payload length error=%d\n", len);
        return;
    }

    spin_lock(&recv_list_lock);
    ret = kfifo_get(&RtcSet_list, &task);
    spin_unlock(&recv_list_lock);

    if (!ret || !task) {
        printk(KERN_ERR "no recv request but got result\n");
        return;
    }

    if (task->result == NULL) {
        printk(KERN_ERR "handle_msg_RtcSet_result:task->result=NULL error!\n");
        return;
    }

    memcpy(task->result, &msg->header, size);
    if (&task->done == NULL) {
        printk(KERN_ERR "handle_msg_RtcSet_result:task->done NULL error!\n");
        return;
    }
    complete(&task->done);

}

static void handle_msg_(void *data, void *buf, size_t size)
{
    RtcHeader *msg = buf;

    if (msg->mesgType == HI_RTCSYNC_MSG_RTCGET) {
        handle_msg_RtcGet_result(msg, size);
    }else if ( msg->mesgType == HI_RTCSYNC_MSG_RTCSET ) {
        handle_msg_RtcSet_result(msg, size);
    } else {
        printk(KERN_ERR "invalid Type 0x%02X\n", msg->mesgType);
        return;
    }

    return;

}
static void handle_msg(void *data, struct mesg_head *mesg, size_t size)
{
    if (mesg->mesg_type != MESG_TYPE_GEPT_MESG) {
        printk(KERN_INFO "handle_msg <Type Err>=%d\n", mesg->mesg_type);
        return;
    }
    handle_msg_(data, mesg + 1, mesg->data_len);
}

static struct spz_msg_hdlr handler = {
    .id = HI_HANDLER_ID_RTCSYNC,
    .callback = handle_msg,
};

int PD_RtcInit(PD_RtcCh_t ch)
{

    return spz_disp_reg_hdlr(hifc_dev, &handler);
}

EXPORT_SYMBOL(PD_RtcInit);

int PD_RtcGetCounter(PD_RtcCh_t ch, PD_RtcCounter_t *time)
{

    RtcGetCommand rtcgetcmd;
    RtcGetResult  rtcgetres;

    memset(&rtcgetcmd, 0 , sizeof(rtcgetcmd));
    rtcgetcmd.header.mesgType = HI_RTCSYNC_MSG_RTCGET;
    rtcgetcmd.header.paramLength = sizeof(RtcGetCommand) - sizeof(RtcHeader);
    rtcgetcmd.ch = ch;

    ReceiveRtcGetResult(&rtcgetcmd, &rtcgetres);
    *time = rtcgetres.cnt;
#ifdef DEBUG
    printk(KERN_INFO "RtcGetCounter ret=%d, time=%d:%d\n", rtcgetres.retcode, time->preCounter, time->postCounter);
#endif
    return(rtcgetres.retcode);

}

EXPORT_SYMBOL(PD_RtcGetCounter);

int PD_RtcSetSync(PD_RtcCh_t ch, PD_RtcCounter_t *time)
{

    RtcSetCommand rtcsetcmd;
    RtcSetResult  rtcsetres;

    memset(&rtcsetcmd, 0 , sizeof(rtcsetcmd));
    rtcsetcmd.header.mesgType = HI_RTCSYNC_MSG_RTCSET;
    rtcsetcmd.header.paramLength = sizeof(RtcSetCommand) - sizeof(RtcHeader);
    rtcsetcmd.ch = ch;
    rtcsetcmd.cnt = *time;

#ifdef DEBUG
    printk(KERN_INFO "RtcSetCounter:Time1=%d, Time2=%d\n", rtcsetcmd.cnt.preCounter, rtcsetcmd.cnt.postCounter);
#endif
    ReceiveRtcSetResult(&rtcsetcmd, &rtcsetres);
#ifdef DEBUG
    printk(KERN_INFO "RtcSet ret=%d\n", rtcsetres.retcode);
#endif

    return(rtcsetres.retcode);

}

EXPORT_SYMBOL(PD_RtcSetSync);

MODULE_LICENSE("GPL");

