/*
 * Detect Hung Task
 *
 * kernel/hung_task.c - kernel thread for detecting tasks stuck in D state
 *
 */

#include <linux/mm.h>
#include <linux/cpu.h>
#include <linux/nmi.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/lockdep.h>
#include <linux/export.h>
#include <linux/sysctl.h>
#include <linux/sched.h>

#ifdef VENDOR_EDIT //fangpan@oppo.com,2015/11/12
#include <linux/slab.h>
#define HUNG_TIMES 3
struct hung_task {
	struct list_head list;
	struct task_struct *tsk;
	int updated;   /*this flag used to indicate whether this hung task is alive*/
	int hung_count;  /*if this count is larger than HUNG_TIMES, will kill it*/
};
struct list_head hung_task_list;
//struct mutex hung_task_lock;
#endif
/*
 * The number of tasks checked:
 */
unsigned long __read_mostly sysctl_hung_task_check_count = PID_MAX_LIMIT;

/*
 * Limit number of tasks checked in a batch.
 *
 * This value controls the preemptibility of khungtaskd since preemption
 * is disabled during the critical section. It also controls the size of
 * the RCU grace period. So it needs to be upper-bound.
 */
#define HUNG_TASK_BATCHING 1024

/*
 * Zero means infinite timeout - no checking done:
 */
unsigned long __read_mostly sysctl_hung_task_timeout_secs = CONFIG_DEFAULT_HUNG_TASK_TIMEOUT;

unsigned long __read_mostly sysctl_hung_task_warnings = 10;

static int __read_mostly did_panic;

static struct task_struct *watchdog_task;

/*
 * Should we panic (and reboot, if panic_timeout= is set) when a
 * hung task is detected:
 */
unsigned int __read_mostly sysctl_hung_task_panic =
				CONFIG_BOOTPARAM_HUNG_TASK_PANIC_VALUE;

static int __init hung_task_panic_setup(char *str)
{
	sysctl_hung_task_panic = simple_strtoul(str, NULL, 0);

	return 1;
}
__setup("hung_task_panic=", hung_task_panic_setup);

static int
hung_task_panic(struct notifier_block *this, unsigned long event, void *ptr)
{
#ifdef VENDOR_EDIT //fangpan@oppo.com,2015/11/12
	struct hung_task* task_item, * tmp;
	printk(KERN_ERR "dump all the hung tasks");
	if(!list_empty(&hung_task_list)) {
		list_for_each_entry_safe(task_item, tmp, &hung_task_list, list)
			sched_show_task(task_item->tsk);
	}
#endif
	did_panic = 1;
	return NOTIFY_DONE;
}

static struct notifier_block panic_block = {
	.notifier_call = hung_task_panic,
};
#ifdef VENDOR_EDIT //fangpan@oppo.com,2015/11/12
static int update_hung_task_list(struct task_struct *t)
{
	struct hung_task* task_item, *tmp;
	struct hung_task* hung;
	if(!list_empty(&hung_task_list)) {
		list_for_each_entry_safe(task_item, tmp, &hung_task_list, list)
			if(task_item->tsk == t) {
				task_item->updated = 1;
				task_item->hung_count++;
				if(fatal_signal_pending(t) && task_item->hung_count >= HUNG_TIMES) {
					t->flags |= PF_OPPO_KILLING; /*this will mark the Dstate killing flag*/
					printk(KERN_ERR "INFO: Now can clean the Uninterrupted task %s\n", t->comm);
					wake_up_process(t);
					/*clean up the task_item too*/
					list_del(&task_item->list);
					kfree(task_item);
					return -EINTR; 
				}
				return 0;
			}
	}
	hung = (struct hung_task*)kmalloc(sizeof(struct hung_task), GFP_ATOMIC);
	if(hung == NULL) {
		printk(KERN_ERR "can't save the hung task %s\n", t->comm);
		return -ENOMEM;
	}
	hung->tsk = t;
	hung->updated = 1;
	hung->hung_count = 1;
	list_add(&hung->list, &hung_task_list);
	return 0;
}
#endif
static void check_hung_task(struct task_struct *t, unsigned long timeout)
{
	unsigned long switch_count = t->nvcsw + t->nivcsw;

	/*
	 * Ensure the task is not frozen.
	 * Also, skip vfork and any other user process that freezer should skip.
	 */
	if (unlikely(t->flags & (PF_FROZEN | PF_FREEZER_SKIP)))
	    return;

	/*
	 * When a freshly created task is scheduled once, changes its state to
	 * TASK_UNINTERRUPTIBLE without having ever been switched out once, it
	 * musn't be checked.
	 */
	if (unlikely(!switch_count))
		return;

	if (switch_count != t->last_switch_count) {
		t->last_switch_count = switch_count;
		return;
	}
#ifdef VENDOR_EDIT //fangpan@Swdp.shanghai,2015/11/12 add the hung task detect
	if(update_hung_task_list(t))
		return;
#endif
	if (!sysctl_hung_task_warnings)
		return;
	sysctl_hung_task_warnings--;

	/*
	 * Ok, the task did not get scheduled for more than 2 minutes,
	 * complain:
	 */
	printk(KERN_ERR "INFO: task %s:%d blocked for more than "
			"%ld seconds.\n", t->comm, t->pid, timeout);
	printk(KERN_ERR "\"echo 0 > /proc/sys/kernel/hung_task_timeout_secs\""
			" disables this message.\n");
	sched_show_task(t);
	debug_show_held_locks(t);

	touch_nmi_watchdog();

	if (sysctl_hung_task_panic) {
		trigger_all_cpu_backtrace();
		panic("hung_task: blocked tasks");
	}
}

/*
 * To avoid extending the RCU grace period for an unbounded amount of time,
 * periodically exit the critical section and enter a new one.
 *
 * For preemptible RCU it is sufficient to call rcu_read_unlock in order
 * to exit the grace period. For classic RCU, a reschedule is required.
 */
static bool rcu_lock_break(struct task_struct *g, struct task_struct *t)
{
	bool can_cont;

	get_task_struct(g);
	get_task_struct(t);
	rcu_read_unlock();
	cond_resched();
	rcu_read_lock();
	can_cont = pid_alive(g) && pid_alive(t);
	put_task_struct(t);
	put_task_struct(g);

	return can_cont;
}

/*
 * Check whether a TASK_UNINTERRUPTIBLE does not get woken up for
 * a really long time (120 seconds). If that happens, print out
 * a warning.
 */
static void check_hung_uninterruptible_tasks(unsigned long timeout)
{
	int max_count = sysctl_hung_task_check_count;
	int batch_count = HUNG_TASK_BATCHING;
	struct task_struct *g, *t;
#ifdef VENDOR_EDIT //fangpan@oppo.com,2015/11/12
	struct hung_task* task_item, * tmp;
#endif

	/*
	 * If the system crashed already then all bets are off,
	 * do not report extra hung tasks:
	 */
	if (test_taint(TAINT_DIE) || did_panic)
		return;

	rcu_read_lock();
	do_each_thread(g, t) {
		if (!max_count--)
			goto unlock;
		if (!--batch_count) {
			batch_count = HUNG_TASK_BATCHING;
			if (!rcu_lock_break(g, t))
				goto unlock;
		}
		/* use "==" to skip the TASK_KILLABLE tasks waiting on NFS */
		if (t->state == TASK_UNINTERRUPTIBLE)
			check_hung_task(t, timeout);
	} while_each_thread(g, t);
 unlock:
	rcu_read_unlock();
#ifdef VENDOR_EDIT //fangpan@oppo.com,2015/11/12
	if(!list_empty(&hung_task_list)) {
		list_for_each_entry_safe(task_item, tmp, &hung_task_list, list)
			if(task_item->updated == 0) {
				list_del(&task_item->list);
				kfree(task_item);
			} else {
				task_item->updated = 0;
			}
	} else
		sysctl_hung_task_warnings = 10;
#endif
}

static unsigned long timeout_jiffies(unsigned long timeout)
{
	/* timeout of 0 will disable the watchdog */
	return timeout ? timeout * HZ : MAX_SCHEDULE_TIMEOUT;
}

/*
 * Process updating of timeout sysctl
 */
int proc_dohung_task_timeout_secs(struct ctl_table *table, int write,
				  void __user *buffer,
				  size_t *lenp, loff_t *ppos)
{
	int ret;

	ret = proc_doulongvec_minmax(table, write, buffer, lenp, ppos);

	if (ret || !write)
		goto out;

	wake_up_process(watchdog_task);

 out:
	return ret;
}

/*
 * kthread which checks for tasks stuck in D state
 */
static int watchdog(void *dummy)
{
	set_user_nice(current, 0);

	for ( ; ; ) {
		unsigned long timeout = sysctl_hung_task_timeout_secs;

		while (schedule_timeout_interruptible(timeout_jiffies(timeout)))
			timeout = sysctl_hung_task_timeout_secs;

		check_hung_uninterruptible_tasks(timeout);
	}

	return 0;
}

static int __init hung_task_init(void)
{
	atomic_notifier_chain_register(&panic_notifier_list, &panic_block);
	watchdog_task = kthread_run(watchdog, NULL, "khungtaskd");
#ifdef VENDOR_EDIT //fangpan@oppo.com,2015/11/12
	INIT_LIST_HEAD(&hung_task_list);
#endif

	return 0;
}

module_init(hung_task_init);
