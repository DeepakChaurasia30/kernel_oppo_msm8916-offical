/*
 * kernel/power/wakeup_reason.c
 *
 * Logs the reasons which caused the kernel to resume from
 * the suspend mode.
 *
 * Copyright (C) 2014 Google, Inc.
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/wakeup_reason.h>
#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/notifier.h>
#include <linux/suspend.h>

#ifdef VENDOR_EDIT
/*Chaoying.Chen@Prd6.BaseDrv.Power.Basic,2016/11/15 Add for wake up source */
#include <linux/notifier.h>
#include <linux/fb.h>
#endif /* VENDOR_EDIT */

#define MAX_WAKEUP_REASON_IRQS 32
static int irq_list[MAX_WAKEUP_REASON_IRQS];
static int irqcount;
static struct kobject *wakeup_reason;
static spinlock_t resume_reason_lock;

static ssize_t last_resume_reason_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
	int irq_no, buf_offset = 0;
	struct irq_desc *desc;
	spin_lock(&resume_reason_lock);
	for (irq_no = 0; irq_no < irqcount; irq_no++) {
		desc = irq_to_desc(irq_list[irq_no]);
		if (desc && desc->action && desc->action->name)
			buf_offset += sprintf(buf + buf_offset, "%d %s\n",
					irq_list[irq_no], desc->action->name);
		else
			buf_offset += sprintf(buf + buf_offset, "%d\n",
					irq_list[irq_no]);
	}
	spin_unlock(&resume_reason_lock);
	return buf_offset;
}

#ifdef VENDOR_EDIT
/*Chaoying.Chen@Prd6.BaseDrv.Power.Basic,2016/11/15 Add for wake up source */
extern u64 wakeup_source_count_rtc;
extern u64 wakeup_source_count_wifi;
extern u64 wakeup_source_count_modem;
extern u64 wakeup_source_count_kpdpwr;

static ssize_t ap_resume_reason_stastics_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{			
	int buf_offset = 0;		
	
	buf_offset += sprintf(buf + buf_offset, "wcnss_wlan");		
	buf_offset += sprintf(buf + buf_offset,  "%s",":");
	buf_offset += sprintf(buf + buf_offset,  "%lld \n",wakeup_source_count_wifi);
	printk(KERN_WARNING "%s wakeup %lld times\n","wcnss_wlan",wakeup_source_count_wifi);

	buf_offset += sprintf(buf + buf_offset, "modem");		
	buf_offset += sprintf(buf + buf_offset,  "%s",":");
	buf_offset += sprintf(buf + buf_offset,  "%lld \n",wakeup_source_count_modem);
	printk(KERN_WARNING "%s wakeup %lld times\n","qcom,smd-modem",wakeup_source_count_modem);
	
	buf_offset += sprintf(buf + buf_offset, "qpnp_rtc_alarm");		
	buf_offset += sprintf(buf + buf_offset,  "%s",":");
	buf_offset += sprintf(buf + buf_offset,  "%lld \n",wakeup_source_count_rtc);
	printk(KERN_WARNING "%s wakeup %lld times\n","qpnp_rtc_alarm",wakeup_source_count_rtc);
	
	buf_offset += sprintf(buf + buf_offset, "power_key");		
	buf_offset += sprintf(buf + buf_offset,  "%s",":");
	buf_offset += sprintf(buf + buf_offset,  "%lld \n",wakeup_source_count_kpdpwr);
	printk(KERN_WARNING "%s wakeup %lld times\n","power_key",wakeup_source_count_kpdpwr);
	return buf_offset;
}

#define MODEM_WAKEUP_SRC_NUM 3
extern u16 modem_wakeup_source;
extern int modem_wakeup_src_count[MODEM_WAKEUP_SRC_NUM];
extern char modem_wakeup_src_string[MODEM_WAKEUP_SRC_NUM][20];
static ssize_t modem_resume_reason_stastics_show(struct kobject *kobj, struct kobj_attribute *attr,
		char *buf)
{
    int max_wakeup_src_count = 0;
    int max_wakeup_src_index = 0;
    int i, total = 0;

    for(i = 0; i < MODEM_WAKEUP_SRC_NUM; i++) {
        total += modem_wakeup_src_count[i];
        printk(KERN_WARNING "%s wakeup %d times, total %d times\n",
        modem_wakeup_src_string[i],modem_wakeup_src_count[i],total);

        if (modem_wakeup_src_count[i] > max_wakeup_src_count) {
            max_wakeup_src_index = i;
            max_wakeup_src_count = modem_wakeup_src_count[i];
        }
    }

    return sprintf(buf, "%s:%d:%d\n", modem_wakeup_src_string[max_wakeup_src_index], max_wakeup_src_count, total);
}

extern struct work_struct wakeup_reason_work;
static struct kset *wakeup_reason_kset;
void wakeup_reason_work_func(struct work_struct *work)
{
    int ret = kobject_uevent(&wakeup_reason_kset->kobj, KOBJ_CHANGE);
    printk("[wakeup_reason_work_func]modem_wakeup_source %d, ret %d\n", modem_wakeup_source, ret);
}

static ssize_t uevent_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", modem_wakeup_source);
}

static struct kobj_attribute uevent = __ATTR_RO(uevent);

static struct attribute *attrs_uevent[] = {
    &uevent.attr,
    NULL,
};
static struct attribute_group attrs_uevent_group = {
    .attrs = attrs_uevent,
};

static int wakeup_uevent_ops_uevent(struct kset *kset, struct kobject *kobj,
              struct kobj_uevent_env *env)
{
     add_uevent_var(env, "CHANNEL=%d", modem_wakeup_source);

    return 0;
}
static struct kset_uevent_ops wakeup_uevent_ops = {
    .uevent = wakeup_uevent_ops_uevent,
};
#endif /* VENDOR_EDIT */

static struct kobj_attribute resume_reason = __ATTR_RO(last_resume_reason);

#ifdef VENDOR_EDIT
/*Chaoying.Chen@Prd6.BaseDrv.Power.Basic,2016/11/15 Add for wake up source */
static struct kobj_attribute ap_resume_reason_stastics = __ATTR_RO(ap_resume_reason_stastics);
static struct kobj_attribute modem_resume_reason_stastics = __ATTR_RO(modem_resume_reason_stastics);
#endif /* VENDOR_EDIT */
static struct attribute *attrs[] = {
	&resume_reason.attr,
#ifdef VENDOR_EDIT
/*Chaoying.Chen@Prd6.BaseDrv.Power.Basic,2016/11/15 Add for wake up source */
	&ap_resume_reason_stastics.attr,
	&modem_resume_reason_stastics.attr,
#endif /* VENDOR_EDIT */
	NULL,
};
static struct attribute_group attr_group = {
	.attrs = attrs,
};

/*
 * logs all the wake up reasons to the kernel
 * stores the irqs to expose them to the userspace via sysfs
 */
void log_wakeup_reason(int irq)
{
	struct irq_desc *desc;
	desc = irq_to_desc(irq);
	if (desc && desc->action && desc->action->name)
		printk(KERN_INFO "Resume caused by IRQ %d, %s\n", irq,
				desc->action->name);
	else
		printk(KERN_INFO "Resume caused by IRQ %d\n", irq);

	spin_lock(&resume_reason_lock);
	if (irqcount == MAX_WAKEUP_REASON_IRQS) {
		spin_unlock(&resume_reason_lock);
		printk(KERN_WARNING "Resume caused by more than %d IRQs\n",
				MAX_WAKEUP_REASON_IRQS);
		return;
	}

	irq_list[irqcount++] = irq;
	spin_unlock(&resume_reason_lock);
}

/* Detects a suspend and clears all the previous wake up reasons*/
static int wakeup_reason_pm_event(struct notifier_block *notifier,
		unsigned long pm_event, void *unused)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		spin_lock(&resume_reason_lock);
		irqcount = 0;
		spin_unlock(&resume_reason_lock);
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block wakeup_reason_pm_notifier_block = {
	.notifier_call = wakeup_reason_pm_event,
};
#ifdef VENDOR_EDIT
/*Chaoying.Chen@Prd6.BaseDrv.Power.Basic,2016/11/15 Add for wake up source */
static void wakeup_reason_count_clear(void)
{
   printk(KERN_INFO  "ENTER %s\n", __func__);	
	wakeup_source_count_rtc = 0;
	wakeup_source_count_wifi = 0;
	wakeup_source_count_modem = 0;
	wakeup_source_count_kpdpwr = 0;
}

static void wakeup_reason_count_out(void)
{
	printk(KERN_INFO   "%s wakeup %lld times\n","wcnss_wlan",wakeup_source_count_wifi);
	printk(KERN_INFO   "%s wakeup %lld times\n","qcom,smd-modem",wakeup_source_count_modem);
	printk(KERN_INFO   "%s wakeup %lld times\n","qpnp_rtc_alarm",wakeup_source_count_rtc);	
	printk(KERN_INFO   "%s wakeup %lld times\n","power_key",wakeup_source_count_kpdpwr);
	printk(KERN_INFO  "ENTER %s\n", __func__);	
}

static void modem_wakeup_reason_count_clear(void)
{
    int i;
    printk(KERN_INFO  "ENTER %s\n", __func__);
    for(i = 0; i < MODEM_WAKEUP_SRC_NUM; i++) {
        modem_wakeup_src_count[i] = 0;
    }
}

static void modem_wakeup_reason_count_out(void)
{
    int i;
    for(i = 0; i < MODEM_WAKEUP_SRC_NUM; i++) {
         printk(KERN_WARNING "%s wakeup %d times\n",
         modem_wakeup_src_string[i],modem_wakeup_src_count[i]);
    }
    printk(KERN_INFO  "ENTER %s\n", __func__);	
}

static int wakeup_src_fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;

	if (evdata && evdata->data && event == FB_EVENT_BLANK) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK) {
			wakeup_reason_count_out();
           modem_wakeup_reason_count_out();
		} else if (*blank == FB_BLANK_POWERDOWN) {
			wakeup_reason_count_clear();
           modem_wakeup_reason_count_clear();
        }
	}
	return 0;
}
static struct notifier_block wakeup_src_fb_notif = {
	.notifier_call = wakeup_src_fb_notifier_callback,
};
#endif /* VENDOR_EDIT */

/* Initializes the sysfs parameter
 * registers the pm_event notifier
 */
int __init wakeup_reason_init(void)
{
	int retval;
	spin_lock_init(&resume_reason_lock);
	retval = register_pm_notifier(&wakeup_reason_pm_notifier_block);
	if (retval)
		printk(KERN_WARNING "[%s] failed to register PM notifier %d\n",
				__func__, retval);

	wakeup_reason = kobject_create_and_add("wakeup_reasons", kernel_kobj);
	if (!wakeup_reason) {
		printk(KERN_WARNING "[%s] failed to create a sysfs kobject\n",
				__func__);
		return 1;
	}
	retval = sysfs_create_group(wakeup_reason, &attr_group);
	if (retval) {
		kobject_put(wakeup_reason);
		printk(KERN_WARNING "[%s] failed to create a sysfs group %d\n",
				__func__, retval);
	}
#ifdef VENDOR_EDIT
/*Chaoying.Chen@Prd6.BaseDrv.Power.Basic,2016/11/15 Add for wake up source */
   wakeup_reason_kset = kset_create_and_add("wakeup_reason_uevent", &wakeup_uevent_ops, kernel_kobj);
   if (!wakeup_reason_kset) {
       printk(KERN_WARNING "[%s] failed to create a kset\n", __func__);
   }
   wakeup_reason_kset->kobj.kset = wakeup_reason_kset;
   retval = sysfs_create_group(&wakeup_reason_kset->kobj, &attrs_uevent_group);
   if (retval) {
       printk(KERN_WARNING "[%s] failed to create a sysfs group %d\n", __func__, retval);
   }
   INIT_WORK(&wakeup_reason_work, wakeup_reason_work_func);
	fb_register_client(&wakeup_src_fb_notif);
#endif /* VENDOR_EDIT */
	return 0;
}

late_initcall(wakeup_reason_init);
