/*
 * Copyright 2009, 2010 Sony Corporation
 *
 * Test driver for HSS
 *
 * You can create/delete an HSS node by writing sysdev attribute file
 * of the /sys/devcies/system/hss_test/ directory.
 *
 * sysdev files:
 *   create_node
 *     Write '1' to try to create an HSS node.  The name and parent
 *     name of the node are specified other files on hss_test directory
 *     (see below)
 *     Write '0' to try to delete an HSS node.  Other string causes error.
 *   node_name
 *     Specify the name of the target HSS node.
 *   parent_node_name
 *     Specify the name of the parent node of the target HSS node.
 *     To clear, write just '\n' to the file.
 *   preset_return_val
 *     Writing any arbitrary value to specify that the new test node
 *     will return the value when the specified callback is called.
 *   retrieve
 *    Writing the nodename to be retrieved, reading the return values
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/device.h>
#include <linux/pm.h>
#include <linux/slab.h>

#include <linux/snsc_hss.h>

/*
 * NOTE:
 * As this module uses functions which are exported via EXPOR_SYMBOL_GPL(),
 * mark this module as GPL.
 * HSS framework exports all functions under non-GPL, therefore HSS client
 * drivers can declare theirselves as proprietary like following:
 * MODULE_LICENSE("Proprietary");
 */
MODULE_LICENSE("GPL");

#define HSS_TEST_NAME_MAX_LEN 31

struct hss_test_node {
	struct list_head next;
	struct hss_node *node;
	unsigned int suspend_fail;
	int returns[HSS_NUM_CALLBACK];
	char name[HSS_TEST_NAME_MAX_LEN + 1];
	char parent_name[HSS_TEST_NAME_MAX_LEN + 1];
};

/*
 * the list of created nodes
 */
LIST_HEAD(hss_test_my_nodes);

static char hss_test_node_name[HSS_TEST_NAME_MAX_LEN + 1];
static char hss_test_node_parent_name[HSS_TEST_NAME_MAX_LEN + 1];
static int returns[HSS_NUM_CALLBACK];
static char hss_test_retrieval_node_name[HSS_TEST_NAME_MAX_LEN + 1];

#if KERNEL_VERSION(2,6,26) <= LINUX_VERSION_CODE
struct bus_type hss_test_subsys = {
	.name = "hss_test",
};
#else
struct bus_type hss_test_subsys = {
	set_kset_name("hss_test"),
};
#endif
/*
 * the HSS node name to be created
 */

static ssize_t hss_test_show_node_name(struct device *dev,
#if KERNEL_VERSION(2,6,34) <= LINUX_VERSION_CODE
				       struct device_attribute * unused,
#endif
				       char *buf)
{
	return sprintf(buf, "%s\n", hss_test_node_name);
};
static ssize_t hss_test_store_node_name(struct device *dev,
#if KERNEL_VERSION(2,6,34) <= LINUX_VERSION_CODE
					struct device_attribute * unused,
#endif
					const char *buf,
					size_t len)
{
	char *p;

	p = strchr(buf, '\n');
	if (p)
		*p = '\0'; /* hackish though... */

	strncpy(hss_test_node_name, buf, HSS_TEST_NAME_MAX_LEN);
	return len; /* FIXME */
};
DEVICE_ATTR(node_name, 0666,
		  hss_test_show_node_name, hss_test_store_node_name);

static ssize_t hss_test_show_node_parent_name(struct device *dev,
#if KERNEL_VERSION(2,6,34) <= LINUX_VERSION_CODE
					      struct device_attribute * unused,
#endif
					      char *buf)
{
	return sprintf(buf, "%s\n", hss_test_node_parent_name);
};
static ssize_t hss_test_store_node_parent_name(struct device *dev,
#if KERNEL_VERSION(2,6,34) <= LINUX_VERSION_CODE
					       struct device_attribute * unused,
#endif
					       const char *buf,
					       size_t len)
{
	char *p;

	p = strchr(buf, '\n');
	if (p)
		*p = '\0'; /* hackish though... */

	strncpy(hss_test_node_parent_name, buf, HSS_TEST_NAME_MAX_LEN);
	return len; /* FIXME */
};
DEVICE_ATTR(node_parent_name, 0666,
		  hss_test_show_node_parent_name, hss_test_store_node_parent_name);

static ssize_t hss_test_show_preset_return_val(struct device *dev,
#if KERNEL_VERSION(2,6,34) <= LINUX_VERSION_CODE
					       struct device_attribute * unused,
#endif
					       char *buf)
{
	ssize_t ret;
	int i;

	ret = 0;
	for (i = 0; i < HSS_NUM_CALLBACK; i++)
		ret += sprintf(buf + ret, "%d:%d\n", i, returns[i]);

	return ret;
};

static ssize_t hss_test_store_preset_return_val(struct device *dev,
#if KERNEL_VERSION(2,6,34) <= LINUX_VERSION_CODE
						struct device_attribute * unused,
#endif
						const char *buf,
						size_t len)
{
	int i, return_val, ret;
	const char *p = buf;

	do {
		ret = sscanf(p, "%d:%d", &i, &return_val);
		if (ret == 2)
			returns[i] = return_val;
		p = strchr(p, '\n');
		if (p)
			p++;
	} while (p && p < (buf + len));
	return len;
}
DEVICE_ATTR(preset_return_val, 0666,
		  hss_test_show_preset_return_val, hss_test_store_preset_return_val);
/*
 * HSS node callbacks
 */
static int hss_test_node_suspend_prepare(struct hss_node *node)
{
	struct hss_test_node *test_node;

	test_node = hss_get_private(node);

	pr_info("%s:name=%s suspend_prepare %d\n", __func__, test_node->name,
		test_node->returns[HSS_CALLBACK_SUSPEND_PREPARE]);
	return test_node->returns[HSS_CALLBACK_SUSPEND_PREPARE];
};

static int hss_test_node_prepare(struct hss_node *node)
{
	struct hss_test_node *test_node;

	test_node = hss_get_private(node);

	pr_info("%s:name=%s prepare %d\n", __func__, test_node->name,
		test_node->returns[HSS_CALLBACK_PREPARE]);
	return test_node->returns[HSS_CALLBACK_PREPARE];
};

static int hss_test_node_suspend(struct hss_node *node)
{
	struct hss_test_node *test_node;
	test_node = hss_get_private(node);

	pr_info("%s:name=%s suspend %d\n", __func__, test_node->name,
		test_node->returns[HSS_CALLBACK_SUSPEND]);
	return test_node->returns[HSS_CALLBACK_SUSPEND];
};

static int hss_test_node_suspend_noirq(struct hss_node *node)
{
	struct hss_test_node *test_node;

	test_node = hss_get_private(node);

	pr_info("%s:name=%s suspend_noirq %d\n", __func__, test_node->name,
		test_node->returns[HSS_CALLBACK_SUSPEND_NOIRQ]);
	return test_node->returns[HSS_CALLBACK_SUSPEND_NOIRQ];
};

static int hss_test_node_resume_noirq(struct hss_node *node)
{
	struct hss_test_node *test_node;

	test_node = hss_get_private(node);

	pr_info("%s:name=%s resume_noirq %d\n", __func__, test_node->name,
		test_node->returns[HSS_CALLBACK_RESUME_NOIRQ]);
	return test_node->returns[HSS_CALLBACK_RESUME_NOIRQ];
};

static int hss_test_node_resume(struct hss_node *node)
{
	struct hss_test_node *test_node;

	test_node = hss_get_private(node);

	pr_info("%s:name=%s resume %d\n", __func__, test_node->name,
		test_node->returns[HSS_CALLBACK_RESUME]);
	return test_node->returns[HSS_CALLBACK_RESUME];
};

static int hss_test_node_complete(struct hss_node *node)
{
	struct hss_test_node *test_node;

	test_node = hss_get_private(node);

	pr_info("%s:name=%s complete %d\n", __func__, test_node->name,
		test_node->returns[HSS_CALLBACK_COMPLETE]);
	return test_node->returns[HSS_CALLBACK_COMPLETE];
};

static int hss_test_node_post_suspend(struct hss_node *node)
{
	struct hss_test_node *test_node;

	test_node = hss_get_private(node);

	pr_info("%s:name=%s post_suspend %d\n", __func__, test_node->name,
		test_node->returns[HSS_CALLBACK_POST_SUSPEND]);
	return test_node->returns[HSS_CALLBACK_POST_SUSPEND];
};

static struct hss_node_ops hss_test_node_ops = {
	.suspend_prepare = hss_test_node_suspend_prepare,
	.prepare = hss_test_node_prepare,
	.suspend = hss_test_node_suspend,
	.suspend_noirq = hss_test_node_suspend_noirq,
	.resume_noirq = hss_test_node_resume_noirq,
	.resume = hss_test_node_resume,
	.complete = hss_test_node_complete,
	.post_suspend = hss_test_node_post_suspend,
};

static int hss_test_create_hss_node(void)
{
	struct hss_node *node;
	struct hss_test_node *test_node;
	const char *parent;
	int error = 0;

	pr_info("%s:<-\n", __func__);

	if (!strlen(hss_test_node_name)) {
		pr_info("%s: node name not specified\n", __func__);
		return -EINVAL;
	}

	pr_info("%s: creating %s node...\n", __func__,
		hss_test_node_name);

	test_node = kzalloc(sizeof(*test_node), GFP_KERNEL);
	if (!test_node) {
		pr_info("%s: test_node alloc failed\n", __func__);
		return -ENOMEM;
	}

	node = hss_alloc_node(hss_test_node_name);
	if (!node) {
		pr_info("%s: alloc_node(%s) failed\n", __func__,
			hss_test_node_name);
		error = -ENOMEM;
		goto failed_alloc_node;
	}

	if (!strlen(hss_test_node_parent_name))
		parent = NULL;
	else
		parent = hss_test_node_parent_name;

	pr_info("%s: parent is %s\n", __func__, parent? parent:"NULL");

	hss_set_private(node, test_node);
	test_node->node = node;

	error = hss_register_node(node, &hss_test_node_ops, parent);
	if (error) {
		pr_info("%s: register_node failed (%d)\n", __func__,
			error);
		goto failed_register_node;
	}

	/*
	 * OK, node registerd.  Then register it for myself...
	 */
	strncpy(test_node->name, hss_test_node_name,
		HSS_TEST_NAME_MAX_LEN);
	strncpy(test_node->parent_name, hss_test_node_parent_name,
		HSS_TEST_NAME_MAX_LEN);
	memcpy(&test_node->returns, &returns, sizeof(returns));

	list_add(&test_node->next, &hss_test_my_nodes);

	pr_info("%s:->\n", __func__);
	return error;

	hss_unregister_node(node);
failed_register_node:
	hss_free_node(node);
failed_alloc_node:
	kfree(test_node);

	return error;
} /* hss_test_create_hss_node */


static int hss_test_delete_hss_node(void)
{
	struct hss_test_node *test_node;
	struct list_head *tmp;

	int error;

	pr_info("%s:<-\n", __func__);

	if (!strlen(hss_test_node_name)) {
		pr_info("%s: node name not specified\n", __func__);
		return -EINVAL;
	}

	/*
	 * find the specified node...
	 */
	pr_info("%s: deleting node '%s'...\n", __func__,
		hss_test_node_name);

	list_for_each(tmp, &hss_test_my_nodes) {
		test_node = list_entry(tmp, struct hss_test_node, next);

		if (!strcmp(test_node->name, hss_test_node_name))
			break;
	};

	if (tmp == &hss_test_my_nodes) {
		pr_info("%s: could not find node '%s'...\n", __func__,
			hss_test_node_name);
		return -ENOENT;
	}
	if (strlen(test_node->parent_name))
		pr_info("%s: node %s has parent '%s'\n", __func__,
			test_node->name, test_node->parent_name);

	error = hss_unregister_node(test_node->node);
	if (error) {
		pr_info("%s: unregister_nodee(%s) failed (%d)\n", __func__,
			hss_test_node_name, error);
		return error;
	}

	hss_free_node(test_node->node);

	list_del(&test_node->next);
	kfree(test_node);

	pr_info("%s:->\n", __func__);

	return 0;
} /* hss_test_delete_hss_node */

static ssize_t hss_test_store_create_node(struct device *dev,
#if KERNEL_VERSION(2,6,34) <= LINUX_VERSION_CODE
					  struct device_attribute * unused,
#endif
					  const char *buf,
					  size_t len)
{
	int error = 0;

	pr_info("%s:<-\n", __func__);

	if (!buf)
		return -EINVAL;

	switch (buf[0]) {
	case '1':
		error = hss_test_create_hss_node();
		break;
	case '0':
		error = hss_test_delete_hss_node();
		break;
	default:
		return -EINVAL;
	};

	pr_info("%s:->\n", __func__);
	return len;
};

DEVICE_ATTR(create_node, 0222, NULL, hss_test_store_create_node);

/*
 * Read specified node return values
 */

static ssize_t hss_test_show_retrieval_node_name(struct device *dev,
#if KERNEL_VERSION(2,6,34) <= LINUX_VERSION_CODE
						 struct device_attribute * unused,
#endif
						 char *buf)
{
	return sprintf(buf, "%s\n", hss_test_retrieval_node_name);
};

static ssize_t hss_test_store_retrieval_node_name(struct device *dev,
#if KERNEL_VERSION(2,6,34) <= LINUX_VERSION_CODE
						  struct device_attribute * unused,
#endif
					       const char *buf,
					       size_t len)
{
	char *p;

	p = strchr(buf, '\n');
	if (p)
		*p = '\0'; /* hackish though... */

	strncpy(hss_test_retrieval_node_name, buf, HSS_TEST_NAME_MAX_LEN);
	return len; /* FIXME */
};
DEVICE_ATTR(retrieval_node, 0666,
		  hss_test_show_retrieval_node_name,
		  hss_test_store_retrieval_node_name);

static ssize_t hss_test_show_retrieve(struct device *dev,
#if KERNEL_VERSION(2,6,34) <= LINUX_VERSION_CODE
				      struct device_attribute * unused,
#endif
				      char *buf)
{
	ssize_t len;
	int i, ret, returnval;
	struct hss_node *node;

	ret = hss_find_node(hss_test_retrieval_node_name, &node);
	if (ret) {
		len = sprintf(buf, "#nodename '%s' not found %d\n",
			      hss_test_retrieval_node_name,
			      ret);
		return len;
	} else
		sprintf(buf, "#nodename '%s' return values\n",
			hss_test_retrieval_node_name);

	len = 0;
	for (i = 0; i < HSS_NUM_CALLBACK; i++) {
		ret = hss_get_callback_result(node, i, &returnval);
		if (!ret)
			len += sprintf(buf + len, "%d:%d\n", i, returnval);
		else
			len += sprintf(buf + len, "%d:#%d\n", i, ret);
	}

	return len;
};

DEVICE_ATTR(retrieve, 0444,
		  hss_test_show_retrieve, NULL);

static int __init hss_test_init(void)
{
	int error;

	pr_info("HSS test driver\n");

	error = subsys_system_register(&hss_test_subsys, NULL);
	if (error)
		return error;

	error = device_create_file(hss_test_subsys.dev_root,
					 &dev_attr_retrieve);
	error = device_create_file(hss_test_subsys.dev_root,
					 &dev_attr_retrieval_node);
	error = device_create_file(hss_test_subsys.dev_root,
					 &dev_attr_create_node);
	error = device_create_file(hss_test_subsys.dev_root,
					 &dev_attr_node_name);
	error = device_create_file(hss_test_subsys.dev_root,
					 &dev_attr_preset_return_val);
	error = device_create_file(hss_test_subsys.dev_root,
					 &dev_attr_node_parent_name);

	pr_info("%s:->\n", __func__);
	return 0;

	/*
	 * error path
	 */
	return error;
}

static void __exit hss_test_exit(void)
{
	struct hss_test_node *test_node;
	struct list_head *tmp, *n;
	int error;

	pr_info("%s:<-\n", __func__);

	list_for_each_safe(tmp, n, &hss_test_my_nodes) {
		test_node = list_entry(tmp, struct hss_test_node, next);

		error = hss_unregister_node(test_node->node);
		if (error)
			pr_info("%s: unregister_node node '%s' failed(%d)\n",
				__func__,
				test_node->name, error);
		hss_free_node(test_node->node);

		list_del(&test_node->next);
		kfree(test_node);
	};

	device_remove_file(hss_test_subsys.dev_root,
				 &dev_attr_node_parent_name);
	device_remove_file(hss_test_subsys.dev_root,
				 &dev_attr_preset_return_val);
	device_remove_file(hss_test_subsys.dev_root,
				 &dev_attr_node_name);
	device_remove_file(hss_test_subsys.dev_root,
				 &dev_attr_create_node);
	device_remove_file(hss_test_subsys.dev_root,
				 &dev_attr_retrieval_node);
	device_remove_file(hss_test_subsys.dev_root,
				 &dev_attr_retrieve);

	bus_unregister(&hss_test_subsys);

	pr_info("%s:->\n", __func__);
}

module_init(hss_test_init);
module_exit(hss_test_exit);
