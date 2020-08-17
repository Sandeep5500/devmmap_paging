#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <mydev.h>

MODULE_DESCRIPTION("My kernel module - mykmod");
MODULE_AUTHOR("Sandeep Kumar and Vedant Singh");
MODULE_LICENSE("GPL");

// Dynamically allocate major no

#define MYKMOD_MAX_DEVS 256
#define MYKMOD_DEV_MAJOR 0
#define PAGE_SIZ 4096

static int mykmod_init_module(void);
static void mykmod_cleanup_module(void);

static int mykmod_open(struct inode *inode, struct file *filp);
static int mykmod_close(struct inode *inode, struct file *filp);
static int mykmod_mmap(struct file *filp, struct vm_area_struct *vma);

module_init(mykmod_init_module);
module_exit(mykmod_cleanup_module);

static struct file_operations mykmod_fops = {
	.owner = THIS_MODULE,	 /* owner (struct module *) */
	.open = mykmod_open,	 /* open */
	.release = mykmod_close, /* release */
	.mmap = mykmod_mmap,	 /* mmap */
};

static void mykmod_vm_open(struct vm_area_struct *vma);
static void mykmod_vm_close(struct vm_area_struct *vma);
static int mykmod_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf);

// TODO Data-structure to keep per device info
struct mykmod_dev_info
{
	char *data;	 //String which holds all data of a device special file
	size_t size; //Size of the file
};

// TODO Device table data-structure to keep all devices
struct mykmod_dev_info **dev_table; // An array of mykmod_dev_info pointers is suitable for keeping track of all the loaded devices.

// TODO Data-structure to keep per VMA info
struct mykmod_vma_info
{
	struct mykmod_dev_info *dev_info; // Will store information about the file which the VMA is associated with
	unsigned long int npagefaults;	  // Number of page faults that have occured in this VMA
};

static const struct vm_operations_struct mykmod_vm_ops = {
	.open = mykmod_vm_open,
	.close = mykmod_vm_close,
	.fault = mykmod_vm_fault};

int mykmod_major;
int n_dev = 0;

static int mykmod_init_module(void) // Carries out the loading off the device module.
{
	printk("mykmod loaded\n");
	printk("mykmod initialized at=%p\n", init_module);

	if ((mykmod_major = register_chrdev(MYKMOD_DEV_MAJOR, "mykmod", &mykmod_fops)) < 0) //register_chrdev() returns the major number of the module when succesful
	{
		printk(KERN_WARNING "Failed to register character device\n");
		return 1;
	}
	else
	{
		printk("register character device %d\n", mykmod_major);
	}

	// TODO initialize device table
	dev_table = kmalloc(MYKMOD_MAX_DEVS * sizeof(struct mykmod_dev_info*), GFP_KERNEL); // Allocates memory for the device table. The array is 256 elements long, one for each possible device file. This is consistent with the fact that there are 256 minor numbers.
	return 0;
}

static void mykmod_cleanup_module(void) // Unloads the device module and deletes all related data structures.
{
	int i = 0;
	printk("mykmod unloaded\n");
	unregister_chrdev(mykmod_major, "mykmod");
	// TODO free device info structures from device table
	while (i < n_dev) // The while loop frees all the initialized elements of the dev_table
	{
		kfree(dev_table[i]);
		i++;
	}
	kfree(dev_table); //Free the dev_table itself, now that all its elements have been freed.
	return;
}

static int
mykmod_open(struct inode *inodep, struct file *filep)
{
	printk("mykmod_open: filep=%p f->private_data=%p "
		   "inodep=%p i_private=%p i_rdev=%x maj:%d min:%d\n",
		   filep, filep->private_data,
		   inodep, inodep->i_private, inodep->i_rdev, MAJOR(inodep->i_rdev), MINOR(inodep->i_rdev));

	// TODO: Allocate memory for dev_info and store in device table and i_private.
	if (inodep->i_private == NULL) //If the file has not yet been loaded into memory in any process, we must allocate space for the file and store its details in the inode pointer.
	{
		struct mykmod_dev_info *info;
		info = kmalloc(sizeof(struct mykmod_dev_info), GFP_KERNEL);
		info->data = (char *)kzalloc(MYDEV_LEN, GFP_KERNEL); // A 1MB slice of memory is alloted to the string field inside the mykmod_dev_info structure
		info->size = MYDEV_LEN;
		inodep->i_private = info; // inodep can now access the device file and its related info through it's private data

		if (n_dev <= MYKMOD_MAX_DEVS)
		{
			dev_table[n_dev] = inodep->i_private;
			n_dev++;
		}
	}

	// Store device info in file's private_data aswell
	filep->private_data = inodep->i_private;
	return 0;
}

static int
mykmod_close(struct inode *inodep, struct file *filep) //Called when closing the device file.
{
	printk("mykmod_close: inodep=%p filep=%p\n", inodep, filep);
	return 0;
}

static int mykmod_mmap(struct file *filp, struct vm_area_struct *vma) // Called when mmap() is done
{
	struct mykmod_vma_info *v_info; // The aim is to update the vma pointer with information regarding the file it points to. This is achieved via the use of its vm_private_data field.
	
	if (MYDEV_LEN < (vma->vm_pgoff * PAGE_SIZ) + vma->vm_end - vma->vm_start) //When the proposed VMA size and offset mean that it doesn't fit in the file, it results in an EINVAL error.
	{
		printk("EINVAL ERROR");
		return -EINVAL;
	}
	printk("mykmod_mmap: filp=%p vma=%p flags=%lx\n", filp, vma, vma->vm_flags);
	
	//TODO setup vma's flags, save private data (dev_info, npagefaults) in vm_private_data
	vma->vm_ops = &mykmod_vm_ops; //Sets the associated vma operations
	vma->vm_flags |= VM_DONTEXPAND | VM_DONTDUMP;

	v_info = kmalloc(sizeof(struct mykmod_vma_info), GFP_KERNEL); // Allocates memory to the mykmod_vma_info structure without initializing
	v_info->dev_info = filp->private_data;						  // The filp->private_data was made to point to the corresponding devinfo structure in mykmod_open and this allows us to access it here.
	vma->vm_private_data = v_info;
	mykmod_vm_open(vma); //Opens the VMA after all the related information has been set

	return 0;
}

static void
mykmod_vm_open(struct vm_area_struct *vma) // Called when VMA is opened
{
	struct mykmod_vma_info *info_open = vma->vm_private_data;
	info_open->npagefaults = 0; //Since the VMA has just been opened, there have been no pagefaults so far.
	printk("mykmod_vm_open: vma=%p npagefaults:%lu\n", vma, info_open->npagefaults);
}

static void
mykmod_vm_close(struct vm_area_struct *vma) // Called when VMA is closed
{
	struct mykmod_vma_info *info_close = vma->vm_private_data;
	printk("mykmod_vm_close: vma=%p npagefaults:%lu\n", vma, info_close->npagefaults);
	info_close->npagefaults = 0; // Now that the VMA has been closed, the value of npagefaults is reinitialized to 0.
}

static int
mykmod_vm_fault(struct vm_area_struct *vma, struct vm_fault *vmf) // When a page that is not present in memory is accessed by the user, it results in a fault and calls vm_fault.
{
	struct mykmod_vma_info *info_fault = vma->vm_private_data;

	// TODO: build virt->phys mappings
	if (info_fault != NULL) // If true, the VMA has been initialized and there is no cause for a segmentation fault.
	{
		struct page *pageptr;
		info_fault->npagefaults += 1;																	// Updates page fault count for VMA in consideration.
		pageptr = virt_to_page(info_fault->dev_info->data + ((vmf->pgoff + vma->vm_pgoff) * PAGE_SIZ)); // info_fault->dev_info->data contains the address
		//of the beginning of the file data and on adding the two offsets to it, we get the virtual address of the location of the fault. vmf->pgoff contains the
		//offset of the fault from the beginning of the vma and vma->vm_pgoff is the offset of the start of the vma from the start of the file.
		get_page(pageptr);
		vmf->page = pageptr; //Stores page pointer in vmf->page

		printk("mykmod_vm_fault: vma=%p vmf=%p pgoff=%lu page=%p\n", vma, vmf, vmf->pgoff, vmf->page);
	}
	else //When the VMA hasn't been intialized and has NULL private data, it leads to a segmentations fault.
	{
		printk("Segmentation fault\n");
		return -VM_FAULT_SIGSEGV;
	}

	return 0;
}
