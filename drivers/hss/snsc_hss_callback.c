/*
 * Hierarchical Suspend Set (HSS)
 *
 * This privides the framework for in-kernel drivers so that
 * the driver can suspend/resume in specified order of drivers.
 *
 * Copyright 2009 Sony Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 * WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 * USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA
 *
 */
#include <linux/version.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/device.h>
#include <linux/snsc_hss.h>

#include "snsc_hss_internal.h"

static void hss_new_do_suspend_prepare(void)
{
	struct list_head *tmp;
	struct hss_node *p;
	int ret = 0;
	pr_debug("%s:<-\n", __func__);

	/*
	 * we are going to start suspend
	 * release it on resume or suspend error recovery
	 */
	mutex_lock(&hss_list_lock);

	hss_clear_invocation_flag_all();
	/*
	 * call the nodes in the reverse order so that
	 * children get called earlier than their parent
	 */
	list_for_each_prev(tmp, &hss_all_nodes_list) {
		p = list_entry(tmp, struct hss_node, next);
		HSS_VERBOSE_PRINT("hss: suspend_prepare '%s'\n", p->name);
		if (p->ops->suspend_prepare != NULL) {
			HSS_VERBOSE_PRINT(" calling...\n");
			ret = p->ops->suspend_prepare(p);
			hss_set_invocation(p, HSS_CALLBACK_SUSPEND_PREPARE, ret);
			HSS_VERBOSE_PRINT(" done ret=%d\n", ret);
		}
#ifdef DEBUG
		if (ret < 0) {
			pr_debug("%s: suspend_prepare of %s = %d\n", __func__,
				 p->name, ret);
			ret = 0;
		}
#endif
	}
	pr_debug("%s:->\n", __func__);
}

int hss_new_prepare(struct device *dev)
{
	struct list_head *tmp;
	struct hss_node *p;
	int ret = 0;
	pr_debug("%s:<-\n", __func__);

	/*
	 * call the nodes in the reverse order so that
	 * children get called earlier than their parent
	 */
	list_for_each_prev(tmp, &hss_all_nodes_list) {
		p = list_entry(tmp, struct hss_node, next);
		HSS_VERBOSE_PRINT("hss: prepare '%s'\n", p->name);
		if (p->ops->prepare != NULL) {
			HSS_VERBOSE_PRINT(" calling...\n");
			ret = p->ops->prepare(p);
			hss_set_invocation(p, HSS_CALLBACK_PREPARE, ret);
			HSS_VERBOSE_PRINT(" done ret=%d\n", ret);
		}
#ifdef DEBUG
		if (ret < 0) {
			pr_debug("%s: prepare of %s = %d\n", __func__,
				 p->name, ret);
			ret = 0;
		}
#endif
	}
	pr_debug("%s:->\n", __func__);
	return 0;
};

int hss_new_suspend(struct device *dev)
{
	struct list_head *tmp;
	struct hss_node *p;
	int ret = 0;

	pr_debug("%s:<-\n", __func__);

#ifdef DEBUG
	BUG_ON(!mutex_is_locked(&hss_list_lock));
#endif
	/*
	 * The list is configured that parents always are placed previous
	 * their children, so call the nodes in reversed order.
	 */
	list_for_each_prev(tmp, &hss_all_nodes_list) {
		p = list_entry(tmp, struct hss_node, next);
		ret = 0;
		HSS_VERBOSE_PRINT("hss: suspend '%s'\n", p->name);
		if (p->ops->suspend != NULL) {
			HSS_VERBOSE_PRINT(" calling...\n");
			ret = p->ops->suspend(p);
			hss_set_invocation(p, HSS_CALLBACK_SUSPEND, ret);
			HSS_VERBOSE_PRINT(" done ret=%d\n", ret);
		}

		if (ret < 0) {
			if (ret != -EBUSY) {
				pr_info("%s: %s returns ! EBUSY (%d)"
					"use EBUSY for suspend cancel\n",
					__func__, p->name, ret);
				ret = -EBUSY;
			}
			break;
		} else
			list_add(&p->processed, &hss_suceeded_nodes_list);
	}

	pr_debug("%s:->\n", __func__);
	return ret;
};

int hss_new_suspend_noirq(struct device *dev)
{
	struct list_head *tmp;
	struct hss_node *p;
	int ret;
	pr_debug("%s:<-\n", __func__);

#ifdef DEBUG
	BUG_ON(!mutex_is_locked(&hss_list_lock));
#endif
	/*
	 * If this function called, suspend is NOT going to be cancelled.
	 * But be paranoid, call suspend_noirq() callbacks that the suceeded
	 * nodes have
	 */

	list_for_each_prev(tmp, &hss_suceeded_nodes_list) {
		p = list_entry(tmp, struct hss_node, processed);
		HSS_VERBOSE_PRINT("hss: suspend_noirq '%s'\n", p->name);
		if (p->ops->suspend_noirq != NULL) {
			HSS_VERBOSE_PRINT(" calling...\n");
			ret = p->ops->suspend_noirq(p);
			hss_set_invocation(p, HSS_CALLBACK_SUSPEND_NOIRQ, ret);
			HSS_VERBOSE_PRINT(" done ret=%d\n", ret);
		}
	}

	pr_debug("%s:->\n", __func__);
	return 0;
};

static void hss_new_resume_noirq_helper(void)
{
	struct list_head *tmp;
	struct hss_node *p;
	int ret;

	list_for_each(tmp, &hss_suceeded_nodes_list) {
		p = list_entry(tmp, struct hss_node, processed);
		HSS_VERBOSE_PRINT("hss: resume_noirq '%s'\n", p->name);
		if (p->ops->resume_noirq != NULL) {
			HSS_VERBOSE_PRINT(" calling...\n");
			ret = p->ops->resume_noirq(p);
			hss_set_invocation(p, HSS_CALLBACK_RESUME_NOIRQ, ret);
			HSS_VERBOSE_PRINT(" done ret=%d\n", ret);
		}
	}
}

int hss_new_resume_noirq(struct device *dev)
{
	pr_debug("%s:<-\n", __func__);

#ifdef DEBUG
	BUG_ON(!mutex_is_locked(&hss_list_lock));
#endif
	hss_new_resume_noirq_helper();

	pr_debug("%s:->\n", __func__);
	return 0;
};

static void hss_new_resume_helper(void)
{
	struct list_head *tmp, *n;
	struct hss_node *p;
	int ret;

	list_for_each_safe(tmp, n, &hss_suceeded_nodes_list) {
		p = list_entry(tmp, struct hss_node, processed);
		HSS_VERBOSE_PRINT("hss: resume '%s'\n", p->name);

		if (p->ops->resume != NULL) {
			HSS_VERBOSE_PRINT(" calling...\n");
			ret = p->ops->resume(p);
			hss_set_invocation(p, HSS_CALLBACK_RESUME, ret);
			HSS_VERBOSE_PRINT(" done ret=%d\n", ret);
		}
		/*
		 * resume completed, remove it from the list
		 */
		list_del(&p->processed);
	}
};

int hss_new_resume(struct device *dev)
{
	pr_debug("%s:<-\n", __func__);

#ifdef DEBUG
	BUG_ON(!mutex_is_locked(&hss_list_lock));
	if (list_empty(&hss_suceeded_nodes_list))
		pr_info("%s: empty?\n", __func__);
#endif
	hss_new_resume_helper();

	pr_debug("%s:->\n", __func__);
	return 0;
};

void hss_new_complete(struct device *dev)
{
	struct list_head *tmp;
	struct hss_node *p;
	int irqs_disabled;
	int ret;

	pr_debug("%s:<-\n", __func__);
#ifdef DEBUG
	BUG_ON(!mutex_is_locked(&hss_list_lock));
#endif
	/*
	 * If HSS reported error on HSS' suspend(),
	 * resume() callback of HSS has not been called.
	 * so we should care the case here.
	 */
	if (!list_empty(&hss_suceeded_nodes_list)) {
		irqs_disabled = irqs_disabled();

		if (!irqs_disabled)
			arch_suspend_disable_irqs();

		hss_new_resume_noirq_helper();

		if (!irqs_disabled)
			arch_suspend_enable_irqs();

		hss_new_resume_helper();
	}
	/*
	 * all registered nodes got called with prepare()
	 * thus we call all complete() nodes here
	 */
	list_for_each(tmp, &hss_all_nodes_list) {
		p = list_entry(tmp, struct hss_node, next);
		HSS_VERBOSE_PRINT("hss: complete '%s'\n", p->name);
		if (p->ops->complete != NULL) {
			HSS_VERBOSE_PRINT(" calling...\n");
			ret = p->ops->complete(p);
			hss_set_invocation(p, HSS_CALLBACK_COMPLETE, ret);
			HSS_VERBOSE_PRINT(" done ret=%d\n", ret);
		}
	}
	pr_debug("%s:->\n", __func__);
};

static void hss_new_do_post_suspend(void)
{
	struct list_head *tmp;
	struct hss_node *p;
	int ret;
	pr_debug("%s:<-\n", __func__);

	/*
	 * When notifiers are invoked for PM_SUSPEND_PREPARE, if a
	 * notifier returns NOTIFY_STOP or NOTIFY_BAD, it's possible
	 * that hss_new_do_suspend_prepare() is not called. In that
	 * case, here is first timing for hss to be called.
	 */
	if (!mutex_is_locked(&hss_list_lock)) {
		mutex_lock(&hss_list_lock);
		hss_clear_invocation_flag_all();
	}
	/*
	 * all registered nodes got called with suspend_prepare()
	 * thus we call all post_suspend() nodes here
	 */
	list_for_each(tmp, &hss_all_nodes_list) {
		p = list_entry(tmp, struct hss_node, next);
		HSS_VERBOSE_PRINT("hss: post_suspend '%s'\n", p->name);

		if (p->ops->post_suspend != NULL) {
			HSS_VERBOSE_PRINT(" calling...\n");
			ret = p->ops->post_suspend(p);
			hss_set_invocation(p, HSS_CALLBACK_POST_SUSPEND, ret);
			HSS_VERBOSE_PRINT(" done ret=%d\n", ret);
		}
	}
	/* we've done resuming */
	mutex_unlock(&hss_list_lock);
	pr_debug("%s:->\n", __func__);
}

static int hss_new_notifier_callback(struct notifier_block *nb,
				 unsigned long action,
				 void *arg)
{
	pr_debug("%s: %ld\n", __func__, action);

	switch (action) {
	case PM_SUSPEND_PREPARE:
		hss_new_do_suspend_prepare();
		break;
	case PM_POST_SUSPEND:
		hss_new_do_post_suspend();
		break;
	default:
		break;
	}

#ifdef DEBUG
	if (in_irq())
		pr_info("%s: IN IRQ\n", __func__);
	if (in_softirq())
		pr_info("%s: IN SWIRQ\n", __func__);
	if (in_interrupt())
		pr_info("%s: IN INTR\n", __func__);
#endif
	return NOTIFY_DONE;
}

struct notifier_block hss_nb = {
	.notifier_call = hss_new_notifier_callback
};
