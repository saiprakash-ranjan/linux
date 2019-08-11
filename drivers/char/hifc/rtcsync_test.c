#ifdef RTCSYNC_TEST

#define MODULE

#include <asm/uaccess.h>    /* copy_from_user */
#include <linux/errno.h>
#include <linux/module.h>   /* module 作成には必須 */
#include <linux/kernel.h>   /* printk */
#include <linux/proc_fs.h>
#include <linux/stat.h>     /* S_IFREG, ... */

#include <hi_rtcsync_message.h>

MODULE_LICENSE( "GPL" );

static char* msg = "module [proc.o]";

#define MAXBUF 32
static unsigned char procbuf[ MAXBUF ];
static int buf_pos;

int PD_RtcGetSync(PD_RtcCh_t ch, PD_RtcCounter_t *time);

int PD_RtcSetSync(PD_RtcCh_t ch, PD_RtcCounter_t *time);


static int
proc_write(struct file* filp, const char* buffer, unsigned long count, void* data)
{
    return 0;
}

static int
proc_read(char* page, char** start, off_t offset, int count, int* eof, void* data)
{

    PD_RtcCounter_t time;
    int ret;

    // RTCドライバの初期化
    PD_RtcInit(PD_RTC_CH0);

    ret = PD_RtcGetCounter(PD_RTC_CH0, &time);
    printk(KERN_INFO "RtcGetCounter Result=%d, time=%d:%d\n", ret, time.preCounter, time.postCounter);

    if (ret == 0) {
        // 未来時間を設定
        time.postCounter += 5;

        ret = PD_RtcSetSync(PD_RTC_CH0, &time);
        printk(KERN_INFO "RtcSetSync Result=%d\n", ret);
    }

    return ret;

}


/*
 * モジュールの初期処理
 * insmod 時に呼ばれる
 */
int
rtcsync_init_module(void)
{
    struct proc_dir_entry* entry;

    printk(KERN_INFO "ent_module\n");
    entry = create_proc_entry("rtctest", S_IFREG | S_IRUGO | S_IWUGO, NULL);

    if (entry != NULL) {
        entry->read_proc  = proc_read;
        entry->write_proc = proc_write;
    } else {
        printk(KERN_INFO "%s : create_proc_entry failed\n", msg);
        return -EBUSY;
    }

    return 0;
}


/*
 * モジュールの解放処理
 * モジュールの参照数が 0 であれば、rmmod 時に呼ばれる
 */
void
rtcsync_cleanup_module(void)
{
    remove_proc_entry("rtctest", NULL);
    printk(KERN_INFO "%s : removed from kernel\n", msg);
}


module_init(rtcsync_init_module);
module_exit(rtcsync_cleanup_module);

#endif
