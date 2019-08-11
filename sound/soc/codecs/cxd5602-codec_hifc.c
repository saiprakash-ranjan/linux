/*
 * COPYRIGHT (C) 2014 Sony Corporation.
 *
 * cxd5602-codec_hifc_hdlr.c
 */

#define DEBUG
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/slab.h>

#include "../../../drivers/char/hifc/hifc.h"
#include "../../../drivers/char/hifc/hifc_msg_disp.h"
#include "../../../drivers/char/hifc/hi_system_handler.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#include "../../../drivers/char/hifc/audio/high_level_api/as_high_level_api.h"
#pragma GCC diagnostic pop
#include "../../../drivers/char/hifc/hi_audio_message.h"
#include "cxd5602_codec.h"

#define WAIT_TIMEOUT msecs_to_jiffies(15000)
#define WAIT_TIMEOUT_READY msecs_to_jiffies(30000)
static int fatal_error = 0;
static size_t recv_ext_size;

static const struct device *hifc_dev;
static DECLARE_COMPLETION(ready_done);
static AudioResult audio_result;
static DECLARE_COMPLETION(recv_done);
static DEFINE_MUTEX(mutex_ext);
static DECLARE_COMPLETION(recv_ext_done);

static void (*trigger_handler)(int, int) = NULL;

static union {
    HI_AudioExtResult AudioExtResult;
    HI_AudioExtResVolume AudioExtResVolume;
    HI_AudioExtResMute AudioExtResMute;
    HI_AudioExtResMode AudioExtResMode;
    HI_AudioExtResXLoud AudioExtResXLoud;
    HI_AudioExtResEnvAdapt AudioExtResEnvAdapt;
    HI_AudioExtResOutDev AudioExtResOutDev;
    HI_AudioExtResMicSel AudioExtResMicSel;
    HI_AudioExtResMicGain AudioExtResMicGain;
    HI_AudioExtResScuMicGain AudioExtResScuMicGain;
} ext_result;

int cxd5602_codec_hifc_is_ready(void)
{
    return completion_done(&ready_done) || spz_is_handler_ready(HI_HANDLER_ID_AUDIO);
}

static int wait_for_ready(void)
{
    int i, ret;
    if (completion_done(&ready_done)) {
        return 1;
    }
    for (i = 0; i < WAIT_TIMEOUT_READY; i += msecs_to_jiffies(1000)) {
        if (spz_is_handler_ready(HI_HANDLER_ID_AUDIO)) {
            // FIXME: audio ready message will be lost if Spz is earlier than Host
            // So pass even if handler ready only
            return 1;
        }
        ret = wait_for_completion_timeout(&ready_done, msecs_to_jiffies(1000));
        if (ret) {
            // succeeded or error
            return ret;
        }
    }
    // timed out
    return 0;
}

void AS_SendAudioCommand(AudioCommand *command)
{
    HI_AudioMsgSend msg;
    int ret, size;

    if (fatal_error) {
        printk(KERN_ERR "fatal error mode. %s ignored\n", __FUNCTION__);
        return;
    }

    ret = wait_for_ready();
    if (ret <= 0) {
        fatal_error = 1;
        printk(KERN_ERR "%s: wait for ready failed %d\n", __FUNCTION__, ret);
        return;
    }

    // FIXME: lock

    memset(&msg, 0, sizeof(msg));
    msg.head.handlerId = HI_HANDLER_ID_AUDIO;
    msg.head.apiId = HI_AUDIO_MSG_FUNC_SEND;
    msg.head.payloadSize = command->header.packet_length << 2;

    memcpy(&msg.command, command, msg.head.payloadSize);

    // TODO: split support
    size = sizeof(msg.head) + msg.head.payloadSize;
    ret = spz_disp_send_msg(hifc_dev, &msg, size);

    if (ret) {
        //fatal_error = 1;
        printk(KERN_ERR "%s: spz_disp_send_msg failed %d\n", __FUNCTION__, ret);
        return;
    }
}

void AS_ReceiveAudioResult(AudioResult *result)
{
    HI_PayloadHeader msg;
    int ret;

    if (fatal_error) {
        printk(KERN_ERR "fatal error mode. %s ignored\n", __FUNCTION__);
        return;
    }

    reinit_completion(&recv_done);
    memset(&audio_result, 0, sizeof(audio_result));

    memset(&msg, 0, sizeof(msg));
    msg.handlerId = HI_HANDLER_ID_AUDIO;
    msg.apiId = HI_AUDIO_MSG_FUNC_RECV;
    msg.payloadSize = 0;
    spz_disp_send_msg(hifc_dev, &msg, sizeof(msg));

    ret = wait_for_completion_timeout(&recv_done, WAIT_TIMEOUT);
    if (ret <= 0) {
        //fatal_error = 1;
        printk(KERN_ERR "%s: wait_for_completion failed %d\n", __FUNCTION__, ret);
        return;
    }

    memcpy(result, &audio_result, sizeof(*result));
}

static void handle_msg_result(HI_PayloadHeader *msg0, size_t size)
{
    HI_AudioMsgResult *msg = (HI_AudioMsgResult*)msg0;
    int len;

    if (size < sizeof(msg->head) + sizeof(msg->result.header)) {
        printk(KERN_ERR "too small size %d < %d\n", size, sizeof(msg->head) + sizeof(msg->result.header));
        return;
    }

    len = msg->result.header.packet_length << 2;
    if (len <= sizeof(msg->result.header)) {
        printk(KERN_WARNING "len %d <= sizeof header %d\n", len, sizeof(msg->result.header));
        len = sizeof(msg->result.header);
    }
    if (size < sizeof(msg->head) + len) {
        // TBD: split support. currently no big result in spec
        printk(KERN_ERR "size %d < result.size %d\n", size, sizeof(msg->head) + len);
        return;
    }
    if (size != sizeof(msg->head) + len) {
        printk(KERN_WARNING "size %d != result.size %d\n", size, sizeof(msg->head) + len);
    }

    printk(KERN_INFO "result len=%d code=%02X:%02X\n",
                         msg->result.header.packet_length, msg->result.header.result_code,
                         msg->result.header.sub_code);

    memcpy(&audio_result, &msg->result, len);
    complete(&recv_done);
}

static void handle_msg_ext_result(HI_PayloadHeader *msg0, size_t size)
{
    HI_AudioExtCmdHeader *msg = (HI_AudioExtCmdHeader*)msg0;

    printk(KERN_INFO "handle_msg_ext_result size=%d cmd=%#x\n", size, msg->CmdCode);

    if (sizeof(ext_result) < size) {
        memcpy(&ext_result, msg, sizeof(ext_result));
    } else {
        memcpy(&ext_result, msg, size);
    }
    recv_ext_size = size;

    complete(&recv_ext_done);
}

static void handle_msg_trigger(HI_PayloadHeader *msg0, size_t size)
{
    HI_AudioMsgTrigger *msg = (HI_AudioMsgTrigger*)msg0;

    if (size < sizeof(*msg)) {
        printk(KERN_ERR "too small size %d < %d\n", size, sizeof(*msg));
        return;
    }
    if (size != sizeof(*msg)) {
        printk(KERN_WARNING "size %d != %d\n", size, sizeof(*msg));
    }

    printk(KERN_INFO "%s %#x %#x\n", __FUNCTION__, msg->CommandCode, msg->SubCode);

    if (trigger_handler)
        trigger_handler(msg->CommandCode, msg->SubCode);
}

int HI_AudioExtCommand(HI_AudioExtCmdHeader *cmd, HI_AudioExtCmdHeader *result, int req_size, int *set_size)
{
    int ret;

    printk(KERN_INFO "%s cmd=%#x rsize=%d\n", __FUNCTION__, cmd->CmdCode, req_size);

    if (fatal_error) {
        printk(KERN_ERR "fatal error mode. %s ignored\n", __FUNCTION__);
        return -EBUSY;
    }

    ret = wait_for_ready();
    if (ret <= 0) {
        fatal_error = 1;
        printk(KERN_ERR "%s: wait for ready failed %d\n", __FUNCTION__, ret);
        return -EBUSY;
    }

    if (mutex_lock_interruptible(&mutex_ext)) {
        printk(KERN_ERR "%s: lock failed\n", __FUNCTION__);
        return -EBUSY;
    }

    reinit_completion(&recv_ext_done);
    memset(&ext_result, 0, sizeof(ext_result));

    cmd->Header.handlerId = HI_HANDLER_ID_AUDIO;
    cmd->Header.flag = 0;
    cmd->Header.apiId = HI_AUDIO_MSG_FUNC_EXT;

    // TODO: split support
    spz_disp_send_msg(hifc_dev, cmd, sizeof(cmd->Header) + cmd->Header.payloadSize);

    ret = wait_for_completion_timeout(&recv_ext_done, WAIT_TIMEOUT);
    if (ret <= 0) {
        mutex_unlock(&mutex_ext);
        //fatal_error = 1;
        printk(KERN_ERR "%s: wait_for_completion failed %d\n", __FUNCTION__, ret);
        return -EBUSY;
    }

    if (req_size > recv_ext_size) {
        memcpy(result, &ext_result, recv_ext_size);
    } else {
        memcpy(result, &ext_result, req_size);
    }
    *set_size = recv_ext_size;

    mutex_unlock(&mutex_ext);
    return 0;
}

static void handle_msg_ready(HI_PayloadHeader *msg0, size_t size)
{
    HI_AudioMsgReady *msg = (HI_AudioMsgReady*)msg0;

    if (size < sizeof(*msg)) {
        printk(KERN_ERR "too small size %d < %d\n", size, sizeof(*msg));
        return;
    }
    if (size != sizeof(*msg)) {
        printk(KERN_WARNING "size %d != %d\n", size, sizeof(*msg));
    }

    printk(KERN_INFO "%s %d\n", __FUNCTION__, msg->ready);

    if (msg->ready)
        complete_all(&ready_done);
}

static int handle_msg_(void *data, void *buf, size_t size)
{
    HI_PayloadHeader *msg = buf;

    printk(KERN_INFO "handle_msg buf=%p size=%d\n", buf, size);

    if (size < sizeof(*msg)) {
        printk(KERN_ERR "too small size %d < %d\n", size, sizeof(*msg));
        return -1;
    }
    if (size < sizeof(*msg) + msg->payloadSize) {
        printk(KERN_ERR "too small size %d < %d+%d\n", size, sizeof(*msg), msg->payloadSize);
        return -1;
    }
    if (size != sizeof(*msg) + msg->payloadSize) {
        printk(KERN_WARNING "size %d != %d\n", size, sizeof(*msg) + msg->payloadSize);
    }

    if (msg->apiId == HI_AUDIO_MSG_FUNC_RESULT) {
        handle_msg_result(msg, size);
        return 1;
    }
    if (msg->apiId == HI_AUDIO_MSG_FUNC_EXT) {
        handle_msg_ext_result(msg, size);
        return 1;
    }
    if (msg->apiId == HI_AUDIO_MSG_FUNC_TRIGGER) {
        handle_msg_trigger(msg, size);
        return 1;
    }
    if (msg->apiId == HI_AUDIO_MSG_FUNC_READY) {
        handle_msg_ready(msg, size);
        return 1;
    }

    printk(KERN_ERR "invalid func id 0x%02X\n", msg->apiId);
    return 0;
}
static int handle_msg(void *data, struct mesg_head *mesg, size_t size)
{
    if (mesg->mesg_type != MESG_TYPE_GEPT_MESG) {
        return -1;
    }
    return handle_msg_(data, mesg + 1, mesg->data_len);
}

static struct spz_msg_hdlr audio_handler = {
    .id = HI_HANDLER_ID_AUDIO,
    .callback = handle_msg,
};

int cxd5602_codec_hifc_init(const struct device *dev, void (*handler)(int, int))
{
    printk(KERN_INFO "%s\n", __FUNCTION__);

    hifc_dev = dev;
    trigger_handler = handler;

    return spz_disp_reg_hdlr(dev, &audio_handler);
}

MODULE_LICENSE("GPL");
