/*
 * COPYRIGHT (C) 2014 Sony Corporation.
 *
 * hifc.h
 */

#ifndef SPZ_HIFC_H_
#define SPZ_HIFC_H_

#include <linux/types.h>

#ifndef __devinit
#define __devinit
#endif
#ifndef __devexit
#define __devexit
#endif
#ifndef __devexit_p
#define __devexit_p
#endif

#define ICMD_FIX_RD_NOP           0x00 // fix read start
#define ICMD_FIX_RD_INTFA_8       0x09
#define ICMD_FIX_RD_LEN_GEPT_0    0x10
#define ICMD_FIX_RD_LEN_GEPT_1    0x11
#define ICMD_FIX_RD_FIFO_0        0x30
#define ICMD_FIX_RD_FIFO_21       0x45 // fix read end
#define ICMD_FIX_WT_SLP           0x50 // fix write start
#define ICMD_FIX_WT_INT_MASK_4    0x53
#define ICMD_FIX_WT_INT_MASK_8    0x55
#define ICMD_FIX_WT_INT_UNMASK_4  0x57
#define ICMD_FIX_WT_INT_UNMASK_8  0x59
#define ICMD_FIX_WT_INT_CLR_2     0x5A
#define ICMD_FIX_WT_FIFO_21       0x75 // fix write end
#define ICMD_FIX_RW_GEPT_0        0x80 // fix r/w start
#define ICMD_FIX_RW_GEPT_31       0x9F // fix r/w end
#define ICMD_VAR_RW_GEPT_0        0xA0 // variable r/w start
#define ICMD_VAR_RW_GEPT_1        0xA1
#define ICMD_VAR_RW_GEPT_2        0xA2
#define ICMD_VAR_RW_GEPT_3        0xA3
#define ICMD_VAR_RW_GEPT_31       0xBF // variable r/w end
#define ICMD_VAR_RW_OFST_GEPT_0   0xC0 // variable ofst r/w port start
#define ICMD_VAR_RW_OFST_GEPT_1   0xC1
#define ICMD_VAR_RW_OFST_GEPT_2   0xC2
#define ICMD_VAR_RW_OFST_GEPT_3   0xC3
#define ICMD_VAR_RW_OFST_GEPT_31  0xDF // variable ofst r/w port end
#define ICMD_VAR_RD_FIFO_0        0xE0 // variable read fifo start
#define ICMD_VAR_RD_FIFO_21       0xF5 // variable read fifo end

#define MIN(a, b)                 ((a) > (b) ? (b) : (a))
#define MAX(a, b)                 ((a) > (b) ? (a) : (b))

#define HEAD_DUM_NUM_RD           0
#define HEAD_DUM_NUM_WT           0
#define HEAD_DUM_NUM_MAX          MAX((HEAD_DUM_NUM_RD), (HEAD_DUM_NUM_WT))

#define MSG_HEAD_LEN              4
#define HEAD_PLACE_HOLDER         (MSG_HEAD_LEN + HEAD_DUM_NUM_MAX)
#define TAIL_DUM_NUM              2 // max of read and write icmd tail dummy num
#define HEAD_WRAP_LEN             (HEAD_PLACE_HOLDER + TAIL_DUM_NUM)

#define BYTES_PER_SAMPLE_UNKNOWN         0
#define BYTES_PER_SAMPLE_ACCEL           6
#define BYTES_PER_SAMPLE_GYRO            6
#define BYTES_PER_SAMPLE_MAG             6
#define BYTES_PER_SAMPLE_BARO            6
#define BYTES_PER_SAMPLE_LIGHT_APDS9930  4
#define BYTES_PER_SAMPLE_LIGHT_BH1721FVC 2

#define FIFO_TIME_STAMP_LEN              6
#define FIFO_STATUS_LEN                  1

struct hifc_bus_ops {
    int32_t (*spz_drv_send_icmd)(void *client, const uint8_t icmd,
                                 uint8_t *buf, const size_t len,
                                 const uint8_t ofst, const uint8_t flg);
    int32_t (*spz_drv_recv_icmd)(void *client, const uint8_t icmd,
                                 uint8_t *buf , const size_t len,
                                 const uint8_t ofst, const uint8_t flg,
                                 uint8_t *buf_ofst, const uint16_t trans_len);
    void (*spz_drv_enable_irq)(const void *client);
};

struct hifc_bus_data {
    void *client;
    const struct hifc_bus_ops *bops;
    unsigned trigger_gpio;
};

struct mesg_head;
struct disp_hdlr_data_internal;
struct disp_hdlr_data {
    int32_t (*spz_disp_recv_mesg)(struct disp_hdlr_data *dhdlr, struct mesg_head *mesg, const size_t len);
    struct disp_hdlr_data_internal *data;
};

struct hifc_ops {
    int32_t (*spz_prot_send_mesg)(void *data, uint8_t *mesg, const size_t len);
    void    (*spz_prot_recv_mesg)(struct work_struct *work);
};

struct prot_kcache_obj {
    struct kmem_cache *gept_mesg_cache;
    struct kmem_cache *intfa_cache;
};

struct hifc_data {
    const struct hifc_ops *pops;
    struct hifc_bus_data *bdata;
    struct disp_hdlr_data *dhdlr;
    struct hifc_prot_tmgr *tmgr;
    struct prot_kcache_obj *kcobj;
    struct work_struct work;
    uint32_t gept_mask;
    uint32_t comm_ready;
};

typedef enum MESG_TYPE_Tag {
    MESG_TYPE_FIFO = 0,
    MESG_TYPE_GEPT_MESG,
    MESG_TYPE_ERR,
} MESG_TYPE;

struct fifo_head {
    uint16_t fifo_num;
};

struct gept_mesg_head {
    uint16_t reserved;
};

struct mesg_head {
    uint16_t mesg_type;
    union {
        struct fifo_head fifo;
        struct gept_mesg_head gept_mesg;
    };
    uint16_t data_len;
    uint8_t handler_type;
    uint8_t reserved;
};

#endif // SPZ_HIFC_H_
