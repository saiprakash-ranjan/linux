/* 2013-10-07: File added and changed by Sony Corporation */
/*
 *  linux/fs/proc/nlver.c
 *
 *  proc interface for quick look-up kernel release versions
 *
 *  Copyright 2002-2008 Sony Corporation
 *
 *  This program is free software; you can redistribute	 it and/or modify it
 *  under  the terms of	 the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the	License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED	  ``AS	IS'' AND   ANY	EXPRESS OR IMPLIED
 *  WARRANTIES,	  INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO	EVENT  SHALL   THE AUTHOR  BE	 LIABLE FOR ANY	  DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED	  TO, PROCUREMENT OF  SUBSTITUTE GOODS	OR SERVICES; LOSS OF
 *  USE, DATA,	OR PROFITS; OR	BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN	 CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA.
 */

#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/snsc_nlver.h>
#include <linux/slab.h>

#include "internal.h"

#define MODNAME                "nlver"
#define MSG_HEAD               MODNAME ": # "
#define PROC_NAME              MODNAME

/* Kernel release version is added as default */
#define NSCLINUX_NAME          "Kernel release"

#ifdef CONFIG_SNSC_NLVER_CUSTOMVER_ENABLE
#define CUSTOMVER_NAME         "Custom version"
#define CUSTOMVER_STR          CONFIG_SNSC_NLVER_CUSTOMVER
#endif

#ifdef CONFIG_SNSC_NLVER_REPOSITORYVERSION_AUTO
#include <linux/snsc_nlver_repover.h>
#define REPOVER_NAME           "Kernel revision"
#define REPOVER_STR            NSCLINUX_REPOSITORY_VERSION
#endif

struct nlver_entry {
        struct list_head  list;
        const char        *name;
        const char        *verstr;
};

LIST_HEAD(nlver_head);
static struct proc_dir_entry *proc_nlver = NULL;
static DEFINE_RWLOCK(nlver_lock);
/*
 *  find nlver entry
 */
static struct nlver_entry *
nlver_find(const char *name)
{
        struct list_head *p;
        struct nlver_entry *entry;

        list_for_each(p, &nlver_head) {
                entry = list_entry(p, struct nlver_entry, list);
                if (strcmp(entry->name, name) == 0) {
                        return entry;
                }
        }

        return NULL;
}

/*
 *  add nlver entry
 */
int
nlver_add(const char *name, const char *verstr)
{
        struct nlver_entry *entry;

        if (!name || !strlen(name) || !verstr || !strlen(verstr)) {
                return -EINVAL;
        }

        entry = kmalloc(sizeof(struct nlver_entry), GFP_KERNEL);
        if (entry == NULL) {
                printk(MSG_HEAD "cannot malloc entry\n");
                return -ENOMEM;
        }

        entry->name = name;
        entry->verstr = verstr;

        write_lock(&nlver_lock);
        list_add_tail(&entry->list, &nlver_head);
        write_unlock(&nlver_lock);

	try_module_get(THIS_MODULE);

        return 0;
}

/*
 *  delete nlver entry
 */
void
nlver_del(const char *name)
{
        struct nlver_entry *entry;

        write_lock(&nlver_lock);
        entry = nlver_find(name);
        if (entry) {
                list_del(&entry->list);
                kfree(entry);
		module_put(THIS_MODULE);
        }
        write_unlock(&nlver_lock);
}

/*
 *  proc read interface
 */
static ssize_t nlver_proc_read(struct file *file, char __user *buf,
				size_t count, loff_t *off)
{
        struct list_head *p;
        struct nlver_entry *entry;
	char tmp[50];
	size_t tmp_size = 0;

        read_lock(&nlver_lock);
        list_for_each(p, &nlver_head) {
                entry = list_entry(p, struct nlver_entry, list);
		tmp_size = snprintf(tmp, sizeof(tmp), "%s: %s\n", entry->name, entry->verstr);
        }
        read_unlock(&nlver_lock);
	return simple_read_from_buffer(buf, count, off, tmp, tmp_size);
}

static const struct file_operations nlver_fops = {
	.owner	= THIS_MODULE,
	.read	= nlver_proc_read,
};

int __init
nlver_init(void)
{
        if ((proc_nlver = proc_create(PROC_NAME, 0, NULL, &nlver_fops)) == NULL) {
                printk(MSG_HEAD "cannot create proc entry\n");
                return 1;
        }

        nlver_add(NSCLINUX_NAME, NSCLINUX_RELEASE);
#ifdef CONFIG_SNSC_NLVER_CUSTOMVER_ENABLE
        if (strlen(CUSTOMVER_STR))
                nlver_add(CUSTOMVER_NAME, CUSTOMVER_STR);
#endif
#ifdef CONFIG_SNSC_NLVER_REPOSITORYVERSION_AUTO
        if (strlen(REPOVER_STR))
                nlver_add(REPOVER_NAME, REPOVER_STR);
#endif
        return 0;
}

void __exit
nlver_exit(void)
{
        struct list_head *p;
        struct nlver_entry *entry;

        remove_proc_entry(PROC_NAME, NULL);

        list_for_each(p, &nlver_head) {
                entry = list_entry(p, struct nlver_entry, list);
                kfree(entry);
        }
}

#ifdef MODULE
module_init(nlver_init);
module_exit(nlver_exit);
#endif

EXPORT_SYMBOL(nlver_add);
EXPORT_SYMBOL(nlver_del);
