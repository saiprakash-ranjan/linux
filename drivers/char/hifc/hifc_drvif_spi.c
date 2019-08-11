/*
 * COPYRIGHT (C) 2014 Sony Corporation.
 *
 * hifc_drvif_spi.c
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#ifdef CONFIG_CPU_PXA1908
#include <linux/edge_wakeup_mmp.h>
#endif

#ifdef CONFIG_OF
#include <linux/of.h>
#endif

#include "hifc.h"
#include "hifc_drvif.h"

extern int32_t __devinit spz_prot_probe(struct device *dev,
                                        struct hifc_bus_data *bdata);
extern int32_t __devexit spz_prot_remove(struct device *dev);

#define SPI_RD_TAIL_DUM_NUM                2 // spi rd icmd data tail dummy num
#define SPI_WT_TAIL_DUM_NUM                2 // spi wt icmd data tail dummy num
#define SPI_RD_MSG_HEAD_DUM_NUM            HEAD_DUM_NUM_RD
#define SPI_WT_MSG_HEAD_DUM_NUM            HEAD_DUM_NUM_WT

// RD 1: ICMD/dummy..
#define SPI_RD_1_POS_MSG_HEAD_ICMD         0
// RD 1 pos in HEAD_PLACE_HOLDER
#define SPI_RD_1_POS_MSG                   (HEAD_PLACE_HOLDER \
                                            - SPI_RD_1_POS_MSG_HEAD_ICMD \
                                            - SPI_RD_MSG_HEAD_DUM_NUM - 1)
#define SPI_RD_1_POS_ICMD                  (SPI_RD_1_POS_MSG \
                                            + SPI_RD_1_POS_MSG_HEAD_ICMD)
// RD 2: ICMD/LEN_LOW/dummy../FLG_LEN_HIGH
#define SPI_RD_2_POS_MSG_HEAD_ICMD         0
#define SPI_RD_2_POS_MSG_HEAD_LEN_LOW      (SPI_RD_2_POS_MSG_HEAD_ICMD + 1)
#define SPI_RD_2_POS_MSG_HEAD_FLG_LEN_HIGH (SPI_RD_2_POS_MSG_HEAD_LEN_LOW \
                                            + SPI_RD_MSG_HEAD_DUM_NUM + 1)
// RD 2 pos in HEAD_PLACE_HOLDER
#define SPI_RD_2_POS_MSG                   (HEAD_PLACE_HOLDER \
                                            - SPI_RD_2_POS_MSG_HEAD_FLG_LEN_HIGH - 1)
#define SPI_RD_2_POS_ICMD                  (SPI_RD_2_POS_MSG \
                                            + SPI_RD_2_POS_MSG_HEAD_ICMD)
#define SPI_RD_2_POS_LEN_LOW               (SPI_RD_2_POS_MSG \
                                            + SPI_RD_2_POS_MSG_HEAD_LEN_LOW)
#define SPI_RD_2_POS_FLG_LEN_HIGH          (SPI_RD_2_POS_MSG \
                                            + SPI_RD_2_POS_MSG_HEAD_FLG_LEN_HIGH)

// RD 3: ICMD/addroffset/dummy../LEN_LOW/FLG_LEN_HIGH
#define SPI_RD_3_POS_MSG_HEAD_ICMD         0
#define SPI_RD_3_POS_MSG_HEAD_OFST         (SPI_RD_3_POS_MSG_HEAD_ICMD + 1)
#define SPI_RD_3_POS_MSG_HEAD_LEN_LOW      (SPI_RD_3_POS_MSG_HEAD_OFST \
                                            + SPI_RD_MSG_HEAD_DUM_NUM + 1)
#define SPI_RD_3_POS_MSG_HEAD_FLG_LEN_HIGH (SPI_RD_3_POS_MSG_HEAD_LEN_LOW + 1)

// RD 3 in HEAD_PLACE_HOLDER
#define SPI_RD_3_POS_MSG                   (HEAD_PLACE_HOLDER \
                                            - SPI_RD_3_POS_MSG_HEAD_FLG_LEN_HIGH - 1)
#define SPI_RD_3_POS_ICMD                  (SPI_RD_3_POS_MSG \
                                            + SPI_RD_3_POS_MSG_HEAD_ICMD)
#define SPI_RD_3_POS_OFST                  (SPI_RD_3_POS_MSG \
                                            + SPI_RD_3_POS_MSG_HEAD_OFST)
#define SPI_RD_3_POS_LEN_LOW               (SPI_RD_3_POS_MSG \
                                            + SPI_RD_3_POS_MSG_HEAD_LEN_LOW)
#define SPI_RD_3_POS_FLG_LEN_HIGH          (SPI_RD_3_POS_MSG \
                                            + SPI_RD_3_POS_MSG_HEAD_FLG_LEN_HIGH)

// WT 1: ICMD
#define SPI_WT_1_POS_MSG_HEAD_ICMD         0
// WT 1 pos in HEAD_PLACE_HOLDER
#define SPI_WT_1_POS_MSG                   (HEAD_PLACE_HOLDER \
                                            - SPI_WT_MSG_HEAD_DUM_NUM - 1)
#define SPI_WT_1_POS_ICMD                  (SPI_WT_1_POS_MSG \
                                            + SPI_WT_1_POS_MSG_HEAD_ICMD)

// WT 2: ICMD/LEN_LOW/FLG_LEN_HIGH
#define SPI_WT_2_POS_MSG_HEAD_ICMD         0
#define SPI_WT_2_POS_MSG_HEAD_LEN_LOW      (SPI_WT_2_POS_MSG_HEAD_ICMD + 1)
#define SPI_WT_2_POS_MSG_HEAD_FLG_LEN_HIGH (SPI_WT_2_POS_MSG_HEAD_LEN_LOW + 1)

// WT 2 pos in HEAD_PLACE_HOLDER
#define SPI_WT_2_POS_MSG                   (HEAD_PLACE_HOLDER \
                                            - SPI_WT_2_POS_MSG_HEAD_FLG_LEN_HIGH - 1)
#define SPI_WT_2_POS_ICMD                  (SPI_WT_2_POS_MSG \
                                            + SPI_WT_2_POS_MSG_HEAD_ICMD)
#define SPI_WT_2_POS_LEN_LOW               (SPI_WT_2_POS_MSG \
                                            + SPI_WT_2_POS_MSG_HEAD_LEN_LOW)
#define SPI_WT_2_POS_FLG_LEN_HIGH          (SPI_WT_2_POS_MSG \
                                            + SPI_WT_2_POS_MSG_HEAD_FLG_LEN_HIGH)

// WT 3: ICMD/addroffset/LEN_LOW/FLG_LEN_HIGH
#define SPI_WT_3_POS_MSG_HEAD_ICMD         0
#define SPI_WT_3_POS_MSG_HEAD_OFST         (SPI_WT_3_POS_MSG_HEAD_ICMD + 1)
#define SPI_WT_3_POS_MSG_HEAD_LEN_LOW      (SPI_WT_3_POS_MSG_HEAD_OFST + 1)
#define SPI_WT_3_POS_MSG_HEAD_FLG_LEN_HIGH (SPI_WT_3_POS_MSG_HEAD_LEN_LOW + 1)

// WT 3 pos in HEAD_PLACE_HOLDER
#define SPI_WT_3_POS_MSG                   (HEAD_PLACE_HOLDER \
                                            - SPI_WT_3_POS_MSG_HEAD_FLG_LEN_HIGH - 1)
#define SPI_WT_3_POS_ICMD                  (SPI_WT_3_POS_MSG \
                                            + SPI_WT_3_POS_MSG_HEAD_ICMD)
#define SPI_WT_3_POS_OFST                  (SPI_WT_3_POS_MSG \
                                            + SPI_WT_3_POS_MSG_HEAD_OFST)
#define SPI_WT_3_POS_LEN_LOW               (SPI_WT_3_POS_MSG \
                                            + SPI_WT_3_POS_MSG_HEAD_LEN_LOW)
#define SPI_WT_3_POS_FLG_LEN_HIGH          (SPI_WT_3_POS_MSG \
                                            + SPI_WT_3_POS_MSG_HEAD_FLG_LEN_HIGH)

#define SPI_RD_HEAD_WRAP_LEN               (HEAD_PLACE_HOLDER \
                                            + SPI_RD_TAIL_DUM_NUM)
#define SPI_WT_HEAD_WRAP_LEN               (HEAD_PLACE_HOLDER \
                                            + SPI_WT_TAIL_DUM_NUM)

#define SPI_RD_DAT_DUM_TRIM                3
#define SPI_RD_1_DAT_LEN_TRIM              (SPI_WT_1_POS_MSG + SPI_RD_DAT_DUM_TRIM)
#define SPI_RD_2_DAT_LEN_TRIM              (SPI_WT_2_POS_MSG + SPI_RD_DAT_DUM_TRIM)
#define SPI_RD_3_DAT_LEN_TRIM              (SPI_WT_3_POS_MSG + SPI_RD_DAT_DUM_TRIM)

static int32_t spi_sync_icmd(struct spi_device *client,
                             uint8_t *t_buf, uint16_t t_buf_len)
{
    struct spi_transfer t = {
        .tx_buf = t_buf,
        .rx_buf = t_buf,
        .len    = t_buf_len,
    };

    struct spi_message m;

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    return spi_sync(client, &m);
}

static int32_t spi_writef1_icmd(struct spi_device *client,
                                const uint8_t icmd, uint8_t *buf,
                                const uint16_t len)
{
    int32_t ret        = 0;
    uint8_t *t_buf     = buf + SPI_WT_1_POS_MSG;
    uint16_t t_buf_len = len + SPI_WT_HEAD_WRAP_LEN
                         - SPI_WT_1_POS_MSG;

    buf[SPI_WT_1_POS_ICMD] = icmd;

    dump_icmd_buffer(t_buf, t_buf_len, DUMP_TYP_TX);

    ret = spi_sync_icmd(client, t_buf, t_buf_len);
    // TODO, handle StatusFlag in t_buf[4]

    dump_icmd_buffer(t_buf, t_buf_len, DUMP_TYP_RX);

    return ret;
}

static int32_t spi_writef2_icmd(struct spi_device *client,
                                const uint8_t icmd, uint8_t *buf,
                                const uint16_t len, const uint8_t flg)
{
    int32_t ret        = 0;
    uint8_t *t_buf     = buf + SPI_WT_2_POS_MSG;
    uint16_t t_buf_len = len + SPI_WT_HEAD_WRAP_LEN
                         - SPI_WT_2_POS_MSG;

    buf[SPI_WT_2_POS_ICMD]         = icmd;
    buf[SPI_WT_2_POS_LEN_LOW]      = get_len_low_byte(len);
    buf[SPI_WT_2_POS_FLG_LEN_HIGH] = get_flg_len_high_byte(len, flg);

    dump_icmd_buffer(t_buf, t_buf_len, DUMP_TYP_TX);

    ret = spi_sync_icmd(client, t_buf, t_buf_len);
    // TODO, handle StatusFlag in t_buf[4]

    dump_icmd_buffer(t_buf, t_buf_len, DUMP_TYP_RX);

    return ret;
}

static int32_t spi_writef3_icmd(struct spi_device *client,
                                const uint8_t icmd, uint8_t *buf,
                                const uint16_t len, const uint8_t ofst,
                                const uint8_t flg)
{
    int32_t ret        = 0;
    uint8_t *t_buf     = buf + SPI_WT_3_POS_MSG;
    uint16_t t_buf_len = len + SPI_WT_HEAD_WRAP_LEN
                         - SPI_WT_3_POS_MSG;

    buf[SPI_WT_3_POS_ICMD]         = icmd;
    buf[SPI_WT_3_POS_OFST]         = ofst;
    buf[SPI_WT_3_POS_LEN_LOW]      = get_len_low_byte(len);
    buf[SPI_WT_3_POS_FLG_LEN_HIGH] = get_flg_len_high_byte(len, flg);

    dump_icmd_buffer(t_buf, t_buf_len, DUMP_TYP_TX);

    ret = spi_sync_icmd(client, t_buf, t_buf_len);
    // TODO, handle StatusFlag in t_buf[4]

    dump_icmd_buffer(t_buf, t_buf_len, DUMP_TYP_RX);

    return ret;
}

static int32_t spi_send_icmd(void *client, const uint8_t icmd,
                             uint8_t *buf, const size_t len,
                             const uint8_t ofst, const uint8_t flg)
{
    int32_t ret = -EIO;

    switch (get_cmd_fmt(icmd)) {
    case CMD_FMT_WT_1:
    case CMD_FMT_RW_1:
        ret = spi_writef1_icmd(client, icmd, buf, len);
        break;
    case CMD_FMT_WT_2:
    case CMD_FMT_RW_2:
        ret = spi_writef2_icmd(client, icmd, buf, len, flg);
        break;
    case CMD_FMT_WT_3:
    case CMD_FMT_RW_3:
        ret = spi_writef3_icmd(client, icmd, buf, len, ofst, flg);
        break;
    case CMD_FMT_MAX:
    default:
        printk(KERN_ERR "%s unsupported ICMD 0x%02x\n", __FUNCTION__, icmd);
        break;
    }

    return ret;
}

static int32_t spi_readf0_icmd(struct spi_device *client, const uint8_t icmd)
{
    uint8_t t_buf      = icmd;
    uint16_t t_buf_len = sizeof(t_buf);
    int32_t ret        = 0;

    dump_icmd_buffer(&t_buf, t_buf_len, DUMP_TYP_TX);

    ret = spi_sync_icmd(client, &t_buf, t_buf_len);
    // TODO, handle StatusFlag in buf[2]

    dump_icmd_buffer(&t_buf, t_buf_len, DUMP_TYP_RX);

    return ret;
}

static int32_t spi_readf1_icmd(struct spi_device *client,
                               const uint8_t icmd, uint8_t *buf,
                               const uint16_t len, const uint16_t trans_len)
{
    int32_t ret        = 0;
    uint8_t *t_buf      = buf + SPI_RD_1_POS_MSG;
    uint16_t t_buf_len = SPI_RD_HEAD_WRAP_LEN + trans_len - SPI_RD_1_POS_MSG;

    buf[SPI_RD_1_POS_ICMD] = icmd;

    dump_icmd_buffer(t_buf, t_buf_len, DUMP_TYP_TX);

    ret = spi_sync_icmd(client, t_buf, t_buf_len);

    dump_icmd_buffer(t_buf, t_buf_len, DUMP_TYP_RX);

    return ret;
    // TODO, handle StatusFlag in buf[6]
}

static int32_t spi_readf2_icmd(struct spi_device *client,
                               const uint8_t icmd, uint8_t *buf,
                               const uint16_t len, const uint8_t flg,
                               const uint16_t trans_len)
{
    int32_t ret        = 0;
    uint8_t *t_buf     = buf + SPI_RD_2_POS_MSG;
    uint16_t t_buf_len = SPI_RD_HEAD_WRAP_LEN + trans_len - SPI_RD_2_POS_MSG;

    buf[SPI_RD_2_POS_ICMD]         = icmd;
    buf[SPI_RD_2_POS_LEN_LOW]      = get_len_low_byte(len);
    buf[SPI_RD_2_POS_FLG_LEN_HIGH] = get_flg_len_high_byte(len, flg);

    dump_icmd_buffer(t_buf, t_buf_len, DUMP_TYP_TX);

    ret = spi_sync_icmd(client, t_buf, t_buf_len);

    dump_icmd_buffer(t_buf, t_buf_len, DUMP_TYP_RX);

    return ret;
    // TODO, handle StatusFlag in buf[6]
}

static int32_t spi_readf3_icmd(struct spi_device *client,
                               const uint8_t icmd, uint8_t *buf,
                               const uint16_t len, const uint8_t ofst,
                               const uint8_t flg, const uint16_t trans_len)
{
    int32_t ret        = 0;
    uint8_t *t_buf     = buf + SPI_RD_3_POS_MSG;
    uint16_t t_buf_len = SPI_RD_HEAD_WRAP_LEN + trans_len - SPI_RD_3_POS_MSG;

    buf[SPI_RD_3_POS_ICMD]         = icmd;
    buf[SPI_RD_3_POS_OFST]         = ofst;
    buf[SPI_RD_3_POS_LEN_LOW]      = get_len_low_byte(len);
    buf[SPI_RD_3_POS_FLG_LEN_HIGH] = get_flg_len_high_byte(len, flg);

    dump_icmd_buffer(t_buf, t_buf_len, DUMP_TYP_TX);

    ret = spi_sync_icmd(client, t_buf, t_buf_len);

    dump_icmd_buffer(t_buf, t_buf_len, DUMP_TYP_RX);

    // TODO magic num
    buf[7] = buf[6];

    return ret;
    // TODO, handle StatusFlag in buf[6]
}

static int32_t spi_recv_icmd(void *client, const uint8_t icmd,
                             uint8_t *buf , const size_t len,
                             const uint8_t ofst, const uint8_t flg,
                             uint8_t *buf_ofst, const uint16_t trans_len)
{
    int32_t ret = -EIO;

    switch (get_cmd_fmt(icmd)) {
    case CMD_FMT_RD_0:
        ret = spi_readf0_icmd(client, icmd);
        break;
    case CMD_FMT_RD_1:
    case CMD_FMT_RW_1:
        ret = spi_readf1_icmd(client, icmd, buf, len, trans_len);
        *buf_ofst = SPI_RD_1_DAT_LEN_TRIM;
        break;
    case CMD_FMT_RD_2:
    case CMD_FMT_RW_2:
        ret = spi_readf2_icmd(client, icmd, buf, len, flg, trans_len);
        *buf_ofst = SPI_RD_2_DAT_LEN_TRIM;
        break;
    case CMD_FMT_RD_3:
    case CMD_FMT_RW_3:
        ret = spi_readf3_icmd(client, icmd, buf, len, ofst, flg, trans_len);
        *buf_ofst = SPI_RD_3_DAT_LEN_TRIM;
        break;
    case CMD_FMT_MAX:
    default:
        printk(KERN_ERR "%s unsupported ICMD 0x%02x\n", __FUNCTION__, icmd);
        break;
    }

    return ret;
}

#ifdef CONFIG_CPU_PXA1908
static void check_irq_gpio(const struct spi_device *client)
{
    struct hifc_data *hdata = dev_get_drvdata(&client->dev);
    int gpio = gpio_get_value(hdata->bdata->trigger_gpio);
    printk(KERN_DEBUG "check_irq_gpio=%d\n", gpio);
    if (gpio) {
        schedule_work(&hdata->work);
    }
}
#endif

static void spi_enable_irq(const void *client)
{
    WARN_ON(!client);

#ifdef CONFIG_CPU_PXA1908
    check_irq_gpio(client);
#else
    enable_irq(((struct spi_device *)client)->irq);
#endif
}

static const struct hifc_bus_ops spz_buf_ops = {
    .spz_drv_send_icmd  = spi_send_icmd,
    .spz_drv_recv_icmd  = spi_recv_icmd,
    .spz_drv_enable_irq = spi_enable_irq
};

static struct hifc_bus_data spz_bus_data = {
    .bops = &spz_buf_ops
};

static irqreturn_t hifc_isr(int32_t irq, void *dev_id)
{
    struct hifc_data *hdata = dev_id;

#ifndef CONFIG_CPU_PXA1908
    struct spi_device *client = hdata->bdata->client;
    disable_irq_nosync(client->irq);
#endif
    schedule_work(&hdata->work);

    return IRQ_HANDLED;
}

static int32_t __devinit init_client(struct spi_device *client)
{
#ifdef CONFIG_CPU_PXA1908
    unsigned long flag = IRQF_TRIGGER_RISING;
#else
    unsigned long flag = IRQF_TRIGGER_HIGH;
#endif
    int32_t ret = request_irq(client->irq,
                              hifc_isr,
                              flag,
                              "hifc_spi",
                              dev_get_drvdata(&client->dev));

#ifdef CONFIG_CPU_PXA1908
    check_irq_gpio(client);
#endif

    return ret;
}

static int32_t __devinit spz_drv_spi_probe(struct spi_device *client)
{
    int32_t ret = 0;

    printk(KERN_ALERT "%s\n", __FUNCTION__);

#ifdef CONFIG_OF
    if (client->dev.of_node != NULL) {
        if (of_property_read_u32(client->dev.of_node,
                                 "hifc-trigger-io",
                                 &spz_bus_data.trigger_gpio) != 0) {

            printk("no such property \"hifc-trigger-io\" in device node\n");

            return -ENODEV;
        }

        gpio_request(spz_bus_data.trigger_gpio, "hifc");
        client->irq = gpio_to_irq(spz_bus_data.trigger_gpio);

#ifdef CONFIG_CPU_PXA1908
        //request_mfp_edge_wakeup(spz_bus_data.trigger_gpio, NULL, NULL, &client->dev);
#endif
    }
#endif

    spz_bus_data.client = (void *)client;

    return ((ret = spz_prot_probe(&client->dev, &spz_bus_data)) ? ret :
           init_client(client));
}

static int32_t __devexit deinit_client(struct spi_device *client)
{
    free_irq(client->irq, dev_get_drvdata(&client->dev));

    return 0;
}

static int32_t __devexit spz_drv_spi_remove(struct spi_device *client)
{
    struct hifc_data *hdata = (struct hifc_data *)dev_get_drvdata(&client->dev);

    deinit_client(client);
    hdata->bdata = NULL;

    printk(KERN_ALERT "%s\n", __FUNCTION__);
    return spz_prot_remove(&client->dev);
}

static struct spi_driver spz_hifc_spi_driver = {
    .probe     = spz_drv_spi_probe,
    .remove    = __devexit_p(spz_drv_spi_remove),
    .driver    = {
        .name  = "hifc_spi",
        .owner = THIS_MODULE,
        .bus   = &spi_bus_type,
    },
};

static int32_t __init spz_drv_spi_init(void)
{
    printk(KERN_ALERT "%s\n", __FUNCTION__);
    return spi_register_driver(&spz_hifc_spi_driver);
}

static void __exit spz_drv_spi_exit(void)
{
    printk(KERN_ALERT "%s\n", __FUNCTION__);
    return spi_unregister_driver(&spz_hifc_spi_driver);
}

module_init(spz_drv_spi_init);
module_exit(spz_drv_spi_exit);

// MODULE_LICENSE("Proprietary");
MODULE_LICENSE("GPL");
