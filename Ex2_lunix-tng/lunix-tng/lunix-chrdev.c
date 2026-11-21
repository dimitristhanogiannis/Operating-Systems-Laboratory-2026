/*
 * lunix-chrdev.c
 *
 * Implementation of character devices
 * for Lunix:TNG
 *
 * Dimitrios Thanogiannis <el21902@mail.ntua.gr>, Dimitrios Karatzanos <el21083@mail.ntua.gr>
 *
 */

#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mmzone.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>

#include "lunix.h"
#include "lunix-chrdev.h"
#include "lunix-lookup.h"

/*
 * Global data
 */
struct cdev lunix_chrdev_cdev;

/*
 * Just a quick [unlocked] check to see if the cached
 * chrdev state needs to be updated from sensor measurements.
 */
/*
 * Declare a prototype so we can define the "unused" attribute and keep
 * the compiler happy. This function is not yet used, because this helpcode
 * is a stub.
 */
static int __attribute__((unused)) lunix_chrdev_state_needs_refresh(struct lunix_chrdev_state_struct *);
static int lunix_chrdev_state_needs_refresh(struct lunix_chrdev_state_struct *state)
{
	struct lunix_sensor_struct *sensor;
	
	WARN_ON (!(sensor = state->sensor));
	/* ? */
	
	/* The following return is bogus, just for the stub to compile */
	return (sensor->msr_data[state->type]->last_update != state->buf_timestamp); //Timestamp comparison to check if new data exist
	/* ? */
}

/* Used by raw ioctl while sleeping for a sample newer than raw_timestamp. */
static int lunix_chrdev_raw_needs_refresh(struct lunix_chrdev_state_struct *state)
{
	struct lunix_msr_data_struct *m;

	m = state->sensor->msr_data[state->type];
	return m->last_update != state->raw_timestamp;
}

/*
 * Updates the cached state of a character device
 * based on sensor data. Must be called with the
 * character device state lock held.
 */
static int lunix_chrdev_state_update(struct lunix_chrdev_state_struct *state)
{
	struct lunix_sensor_struct *sensor;
	struct lunix_msr_data_struct *m;
	uint32_t raw;
	uint32_t ts;
	long formatted_value;
	long integer_part;
	long decimal_part;
	unsigned long flags; //CPU register state for spinlock

	debug("entering\n");

	sensor = state->sensor;
	m = state->sensor->msr_data[state->type];

	/*
	 * Grab the raw data quickly, hold the
	 * spinlock for as little as possible.
	 */
	/* ? */
	/* Why use spinlocks? See LDD3, p. 119 */

	spin_lock_irqsave(&sensor->lock, flags);

	raw = m->values[0];
	ts = m->last_update;

	spin_unlock_irqrestore(&sensor->lock, flags);

	/*
	 * Any new data available?
	 */
	/* ? */

	if(ts == state->buf_timestamp) return -EAGAIN;

	/*
	 * Now we can take our time to format them,
	 * holding only the private state semaphore
	 */

	/* ? */

	state->buf_timestamp = ts;
	

	switch(state->type){
	case BATT:
		formatted_value = lookup_voltage[raw];
		break;
	case TEMP:
		formatted_value = lookup_temperature[raw];
		break;
	case LIGHT:
		formatted_value = lookup_light[raw];
		break;
	default:
		return -EINVAL;
	}

	integer_part = formatted_value / 1000;
	decimal_part = formatted_value % 1000;

	if(decimal_part < 0) decimal_part = -decimal_part;

	if (formatted_value < 0 && integer_part == 0) 
		state->buf_lim = snprintf((char *)state->buf_data, LUNIX_CHRDEV_BUFSZ, "-%ld.%03ld\n", integer_part, decimal_part);
	else 
    	state->buf_lim = snprintf((char *)state->buf_data, LUNIX_CHRDEV_BUFSZ, "%ld.%03ld\n", integer_part, decimal_part);

	debug("leaving\n");
	return 0;
}

/*************************************
 * Implementation of file operations
 * for the Lunix character device
 *************************************/

static int lunix_chrdev_open(struct inode *inode, struct file *filp)
{
	/* Declarations */
	/* ? */

	struct lunix_chrdev_state_struct *state;
	int ret;
	unsigned int minor, sensor, type;

	debug("entering\n");
	ret = -ENODEV;
	if ((ret = nonseekable_open(inode, filp)) < 0)
		goto out;

	/*
	 * Associate this open file with the relevant sensor based on
	 * the minor number of the device node [/dev/sensor<NO>-<TYPE>]
	 */
	
	/* Allocate a new Lunix character device private state structure */
	/* ? */

	minor = iminor(inode);
	sensor = minor >> 3;
	type = minor & 7;

	if (sensor >= lunix_sensor_cnt || type >= N_LUNIX_MSR) {
		ret = -ENODEV;
		goto out;
	}

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (!state) {
		ret = -ENOMEM;
		goto out;
	}
 
	state->type = type;
	state->sensor = &lunix_sensors[sensor];
	state->buf_lim = 0;
	state->buf_timestamp = 0;
	state->raw_timestamp = 0;
	sema_init(&state->lock, 1);

	filp->private_data = state;
	ret = 0;

out:
	debug("leaving, with ret = %d\n", ret);
	return ret;
}

static int lunix_chrdev_release(struct inode *inode, struct file *filp)
{
	/* ? */
	kfree(filp->private_data);
	return 0;
}

static long lunix_chrdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct lunix_chrdev_state_struct *state = filp->private_data;
	struct lunix_sensor_struct *sensor;
	unsigned long flags;
	uint16_t raw;
	uint32_t timestamp;
	int ret = 0;

	// The open file state identifies the sensor and measurement type.
	if (!state)
		return -EINVAL;
	// Check that this command belongs to Lunix and has a supported number.
	if (_IOC_TYPE(cmd) != LUNIX_IOC_MAGIC || _IOC_NR(cmd) > LUNIX_IOC_MAXNR)
		return -ENOTTY;
	// These commands copy data from the kernel to user space.
	if (_IOC_DIR(cmd) != _IOC_READ)
		return -EINVAL;

	sensor = state->sensor;
	// Protect this open file's cached state.
	if (down_interruptible(&state->lock))
		return -ERESTARTSYS;

	switch (cmd) {
	case LUNIX_IOC_GET_COOKED:
		// Refresh the formatted cache, waiting for a new sample as read() does.
		while ((ret = lunix_chrdev_state_update(state)) == -EAGAIN) {
			// Release the semaphore before sleeping.
			up(&state->lock);
			// A non-blocking file returns immediately if no sample is ready.
			if (filp->f_flags & O_NONBLOCK)
				return -EAGAIN;
			// Sleep until the measurement timestamp changes.
			if (wait_event_interruptible(sensor->wq, lunix_chrdev_state_needs_refresh(state)))
				return -ERESTARTSYS;
			// Reacquire the semaphore and check again after waking.
			if (down_interruptible(&state->lock))
				return -ERESTARTSYS;
		}
		// Copy the formatted text to the user-provided buffer.
		if (!ret && copy_to_user((void __user *)arg, state->buf_data, state->buf_lim))
			ret = -EFAULT;
		break;
	case LUNIX_IOC_GET_RAW:
		// Wait for a raw sample newer than the last one returned by this ioctl.
		for (;;) {
			// Read the raw value and timestamp together as one consistent snapshot.
			spin_lock_irqsave(&sensor->lock, flags);
			raw = (uint16_t)sensor->msr_data[state->type]->values[0];
			timestamp = sensor->msr_data[state->type]->last_update;
			spin_unlock_irqrestore(&sensor->lock, flags);

			// A changed timestamp means a new sample is ready.
			if (timestamp != state->raw_timestamp)
				break;

			// No new sample: release the semaphore before waiting.
			up(&state->lock);
			if (filp->f_flags & O_NONBLOCK)
				return -EAGAIN;
			// Sleep until the raw timestamp changes.
			if (wait_event_interruptible(sensor->wq,
					lunix_chrdev_raw_needs_refresh(state)))
				return -ERESTARTSYS;
			// Reacquire the semaphore and take another snapshot after waking.
			if (down_interruptible(&state->lock))
				return -ERESTARTSYS;
		}
		// Copy the raw value to user space.
		if (copy_to_user((void __user *)arg, &raw, sizeof(raw)))
			ret = -EFAULT;
		else
			// Mark the sample as returned only after the copy succeeds.
			state->raw_timestamp = timestamp;
		break;
	default:
		// This ioctl command is not supported.
		ret = -ENOTTY;
		break;
	}

	// Release the semaphore before returning.
	up(&state->lock);
	return ret;
}

static ssize_t lunix_chrdev_read(struct file *filp, char __user *usrbuf, size_t cnt, loff_t *f_pos)
{
	ssize_t ret;

	struct lunix_sensor_struct *sensor;
	struct lunix_chrdev_state_struct *state;

	state = filp->private_data;
	WARN_ON(!state);

	sensor = state->sensor;
	WARN_ON(!sensor);

	/* Lock? (sem)*/
	if (down_interruptible(&state->lock)) // get the semaphore and if it fails return error status
		return -ERESTARTSYS;
	/*
	 * If the cached character device state needs to be
	 * updated by actual sensor data (i.e. we need to report
	 * on a "fresh" measurement, do so
	 */

	if (*f_pos == 0) {
        while (lunix_chrdev_state_update(state) == -EAGAIN) {
			/* ? */
			/* The process needs to sleep (wq)*/
			/* See LDD3, page 153 for a hint */
			up(&state->lock); // leave semaphore to be able to sleep without deadlock
			if (filp->f_flags & O_NONBLOCK) // if user wants non blocking read we do not let the process sleep
				return -EAGAIN;
			if (wait_event_interruptible(sensor->wq, lunix_chrdev_state_needs_refresh(state))) // let process sleep and wake up when new data come from the sensor
				return -ERESTARTSYS;
			if (down_interruptible(&state->lock)) // get the semaphore and if it fails return error status
				return -ERESTARTSYS;
	    }
	}

	/* End of file */
	/* ? */
	if(*f_pos >= state->buf_lim){ // if something went wrong, the read should return 0
		ret = 0;
		goto out;
	}
	
	/* Determine the number of cached bytes to copy to userspace */
	/* ? */
	if(*f_pos + cnt > state->buf_lim){ // if requested cnt is bigger than buffer limit we send what is left
		cnt = state->buf_lim - *f_pos;
	}	

	if(copy_to_user(usrbuf, state->buf_data + *f_pos, cnt)){ 
        ret = -EFAULT;
        goto out;
	}
    
	*f_pos += cnt; // move buffer position pointer
	ret = cnt; // retrun number of bytes given to the user

	/* Auto-rewind on EOF mode? */
	/* ? */
	if(*f_pos >= state->buf_lim) *f_pos = 0; // return buffer position th the start

out:
	/* Unlock? (sem)*/
	up(&state->lock);
	return ret;
}

static int lunix_chrdev_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct lunix_chrdev_state_struct *state;
	unsigned long pfn; // page location in the kernel memory
	unsigned long size;

	state = filp->private_data;

	WARN_ON(!state);

	size = vma->vm_end - vma->vm_start;
	if (size > PAGE_SIZE) // we want only one page for allocation
		return -EINVAL;

	if (vma->vm_pgoff != 0) // offset must be 0 (one page)
		return -EINVAL;

	if (vma->vm_flags & VM_WRITE) // only read operation is allowed
		return -EPERM;

	vm_flags_set(vma, VM_IO | VM_DONTEXPAND | VM_DONTDUMP); // this page is for I/O (not swapping in disk), 
														  // page expanding not allowed (possible collision with other measurments),
														  // dont dump this page in case of failure (segmentation fault)

	pfn = page_to_pfn(virt_to_page(state->sensor->msr_data[state->type])); // find page frame number in the kernel memory
																		   // multiplied by 4096 so that we can map pfn to actual address

	if (remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot)) // map the virtual memory page of the user space 
																		   // to the physical memory page of the sensor data
		return -EAGAIN;

	return 0;
}

static struct file_operations lunix_chrdev_fops = 
{
	.owner          = THIS_MODULE,
	.open           = lunix_chrdev_open,
	.release        = lunix_chrdev_release,
	.read           = lunix_chrdev_read,
	.unlocked_ioctl = lunix_chrdev_ioctl,
	.mmap           = lunix_chrdev_mmap
};

int lunix_chrdev_init(void)
{
	/*
	 * Register the character device with the kernel, asking for
	 * a range of minor numbers (number of sensors * 8 measurements / sensor)
	 * beginning with LINUX_CHRDEV_MAJOR:0
	 */
	int ret;
	dev_t dev_no;
	unsigned int lunix_minor_cnt = lunix_sensor_cnt << 3;

	debug("initializing character device\n");
	cdev_init(&lunix_chrdev_cdev, &lunix_chrdev_fops);
	lunix_chrdev_cdev.owner = THIS_MODULE;
	
	dev_no = MKDEV(LUNIX_CHRDEV_MAJOR, 0);
	/* ? */
	ret = register_chrdev_region(dev_no, 16 * 8, "lunix"); // register 16 * 8 minor numbers for lunix chrdev

	if (ret < 0) {
		debug("failed to register region, ret = %d\n", ret);
		goto out;
	}
	/* ? */
	ret = cdev_add(&lunix_chrdev_cdev, dev_no, 16 * 8); // add character device
	if (ret < 0) {
		debug("failed to add character device\n");
		goto out_with_chrdev_region;
	}
	debug("completed successfully\n");
	return 0;

out_with_chrdev_region:
	unregister_chrdev_region(dev_no, lunix_minor_cnt);
out:
	return ret;
}

void lunix_chrdev_destroy(void)
{
	dev_t dev_no;
	unsigned int lunix_minor_cnt = lunix_sensor_cnt << 3;

	debug("entering\n");
	dev_no = MKDEV(LUNIX_CHRDEV_MAJOR, 0);
	cdev_del(&lunix_chrdev_cdev);
	unregister_chrdev_region(dev_no, lunix_minor_cnt);
	debug("leaving\n");
}
