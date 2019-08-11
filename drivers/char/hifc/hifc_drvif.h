/*
 * COPYRIGHT (C) 2014 Sony Corporation.
 *
 * hifc_drvif.h
 */

#ifndef SPZ_HIFC_DRVIF_H_
#define SPZ_HIFC_DRVIF_H_

#define BYTES_PER_LINE    8

typedef enum CMD_FMT_Tag {
    CMD_FMT_WT_1 = 0,
    CMD_FMT_WT_2,
    CMD_FMT_WT_3,
    CMD_FMT_RD_0,
    CMD_FMT_RD_1,
    CMD_FMT_RD_2,
    CMD_FMT_RD_3,
    CMD_FMT_RW_1,
    CMD_FMT_RW_2,
    CMD_FMT_RW_3,
    CMD_FMT_MAX
} CMD_FMT;

static inline CMD_FMT get_cmd_fmt(const uint8_t icmd)
{
    return ((icmd  == ICMD_FIX_RD_NOP)                                                ? CMD_FMT_RD_0 :
            ((icmd >  ICMD_FIX_RD_NOP)         && (icmd <= ICMD_FIX_RD_FIFO_21))      ? CMD_FMT_RD_1 :
            ((icmd >= ICMD_FIX_WT_SLP)         && (icmd <= ICMD_FIX_WT_FIFO_21))      ? CMD_FMT_WT_1 :
            ((icmd >= ICMD_FIX_RW_GEPT_0)      && (icmd <= ICMD_FIX_RW_GEPT_31))      ? CMD_FMT_RW_1 :
            ((icmd >= ICMD_VAR_RW_GEPT_0)      && (icmd <= ICMD_VAR_RW_GEPT_31))      ? CMD_FMT_RW_2 :
            ((icmd >= ICMD_VAR_RW_OFST_GEPT_0) && (icmd <= ICMD_VAR_RW_OFST_GEPT_31)) ? CMD_FMT_RW_3 :
            ((icmd >= ICMD_VAR_RD_FIFO_0)      && (icmd <= ICMD_VAR_RD_FIFO_21))      ? CMD_FMT_RD_2 :
            CMD_FMT_MAX);
}

static inline uint8_t get_len_low_byte(const uint16_t len)
{
    return len & 0x00FF;
}

static inline uint8_t get_flg_len_high_byte(const uint16_t len, const uint8_t flg)
{
    return ((flg & 0b00000011) << 6) | ((len >> 8) & 0b00111111);
}

typedef enum DUMP_TYP_Tag{
    DUMP_TYP_TX = 0,
    DUMP_TYP_RX,
    DUMP_TYP_MAX
} DUMP_TYP;

static void dump_icmd_buffer(const uint8_t *buf, const uint16_t len, const DUMP_TYP type)
{
#ifdef CONFIG_HIFC_ICMD_DEBUG
    int32_t i = 0;

    (DUMP_TYP_TX == type) ? printk("\nICMD send:\n")
                          : printk("\nICMD recv:\n");

    for (i = 0; i < len; ++i) {
        if (i && (!(i % BYTES_PER_LINE))) {
            printk("\n");
        }

        printk(" %02x", *buf++);
    }

    printk("\n");
#else
    (void)buf;
    (void)len;
    (void)type;
#endif
}

#endif /* SPZ_HIFC_DRVIF_H_ */
