#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/slab.h>

#define DEVICE_NAME "scullchar"
#define CLASS_NAME "char"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vishal Srivastava");
MODULE_DESCRIPTION(" A character driver ");
MODULE_VERSION("0.1");

struct storage{
	char **data;
	size_t blockId;
	size_t size;
	size_t cellSize;
	struct storage *next;
	struct storage *prev;
};

struct scull_dev{
	size_t devId;
	size_t size;
	struct  storage *entry;
	struct storage *exit;
} dev1;

static int majorNumber;
static size_t  numOpens=0;
static size_t blockIds=0;
static struct class* scullClass = NULL;
static struct device* scullDevice = NULL;
static size_t cellS=10;

static int scull_open(struct inode *, struct file *);
static int scull_release(struct inode *, struct file *);
static ssize_t scull_read(struct file *, char *, size_t, loff_t *);
static ssize_t scull_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops =
{
	.owner = THIS_MODULE,
	.open = scull_open,
	.read = scull_read,
	.write = scull_write,
	.release = scull_release,
};

/////////Will be used for cleanup of complete device////////////////////////////
static int scull_trim(struct scull_dev *dev){
	struct storage *block;
	size_t blkSize=0,cellSize=0;
	//goto last block
	block=dev->exit;
	//until all blocks empty
	while(block != NULL){
		size_t blkIndex=0,cellIndex=0;
		blkSize=block->size;
		for(blkIndex=0;blkIndex<blkSize;blkIndex++){
			kfree(block->data[blkIndex]);
		}
		kfree(block->data);
		block->data=NULL;
		block=block->prev;
		if(block != NULL){
			kfree(block->next);
			block->next=NULL;
		}
	}
	kfree(dev->entry);
	dev->entry=NULL;
	dev->exit=NULL;
	return 0;
}


///////////Used for Reading a cell in a block//////////////////////////////////////
static ssize_t scull_read(struct file *fp, char *buf, size_t count, loff_t *f_pos){
	int err = 0;
	size_t i=0,j=0,k=0;
	size_t messageSize;
	size_t cellCount=0;
	struct scull_dev *dev=fp->private_data;
	char *retString=NULL;
	struct storage *block;
	if((f_pos==NULL)||(*f_pos<0)){
		printk(KERN_INFO "SCULL: fpos is either NULL or contains negative value\n");
	}
	//printk(KERN_INFO "SCULL: Phase1\n");
	if(dev->entry == NULL){
		printk(KERN_ALERT "SCULL: Device is empty");
		return -EFAULT;
	}
	//considering fops  to be block no and count to be  no characters in the block.

        //printk(KERN_INFO "SCULL: Phase2\n");
 	block = dev->entry;
	while((block->blockId != *f_pos)&&(block != NULL)){
		block=block->next;
	}
	if(block == NULL){
		printk(KERN_INFO "SCULL: Block not found");
		return -EFAULT;
	}
        //printk(KERN_INFO "SCULL: Phase3\n");
	retString=kmalloc((count+1)*sizeof(char),GFP_KERNEL);
	cellCount=count/cellS;
	for(i=0;i<cellCount;i++){
		for(j=0;j<cellS;j++){
			retString[i*cellS+j]=(char)block->data[i][j];
		}
	}

	for(k=0;k<(count%cellS);k++){
		retString[i*cellS+k]=(char)block->data[i][k];
		//printk(KERN_INFO "SCULL: Char:%c",(char)block->data[i][k]);
	}
	retString[i*cellS+k]='\0';
        //printk(KERN_INFO "SCULL: Phase4 %s %d\n",retString, strlen(retString));
 
	err = copy_to_user(buf,(char*)(retString), strlen(retString));
	kfree(retString);
        //printk(KERN_INFO "SCULL: Phase5\n");
 

	if(err==0){
		printk(KERN_INFO "SCULL: Sent %ul characters to the user", count);
	}
	else{
		printk(KERN_INFO "SCULL: Failed to send %d charcters to the user\n", err);
		return -EFAULT;
	}
	return 0;
}

///////////Used to Allocate Blocks //////////////////////////////////
static int block_alloc(size_t NBlocks, size_t Ncells, struct scull_dev *dev){

	size_t cellCount=Ncells;
	size_t BlockCount=NBlocks;
	dev->size=dev->size + NBlocks;
	printk(KERN_INFO "SCULL: Within block alloc\n");
	while(BlockCount --){
	        if((dev->exit != NULL)&&(dev->entry != NULL)){
        	        dev->exit->next=kmalloc(sizeof(struct storage),GFP_KERNEL);
                	dev->exit->next->data=kmalloc(Ncells*sizeof(char *),GFP_KERNEL);
			for(cellCount=0;cellCount < Ncells; cellCount++){
                		dev->exit->next->data[cellCount]=kmalloc(cellS*sizeof(char), GFP_KERNEL);
			}
			dev->exit->next->cellSize=cellS;
			dev->exit->next->prev=dev->exit;
			dev->exit=dev->exit->next;
			dev->exit->next=NULL;
			dev->exit->blockId=blockIds;
			blockIds++;
			dev->exit->size=Ncells;

        	}
		else if((dev->exit == NULL)&&(dev->entry == NULL)){
                	dev->exit=kmalloc(sizeof(struct storage),GFP_KERNEL);
                	dev->exit->data=kmalloc(Ncells*sizeof(char *),GFP_KERNEL);
			for(cellCount=0;cellCount < Ncells;cellCount++){
                		dev->exit->data[cellCount]=kmalloc(cellS*sizeof(char), GFP_KERNEL);
			}
			dev->exit->cellSize=cellS;
                	dev->exit->next=NULL;
			dev->exit->prev=NULL;
                	dev->entry=dev->exit;
			dev->exit->size=Ncells;
			dev->exit->blockId=blockIds;
			blockIds++;
		}
	}
	printk(KERN_INFO "SCULL: Block alloc done\n");
	return 0;

}

static int cell_increase(struct storage *block, size_t increaseBy){
	//allocate new data cells
	char **newData,**oldData;
	size_t oldBlockSize=(block->size);
	size_t newBlockSize=oldBlockSize+ increaseBy;
	size_t cellSize=(block->cellSize);
	size_t cellIndex=0;
	//printk(KERN_INFO "SCULL: Cell increase started\n");
        if(block==NULL){
                printk(KERN_INFO "SCULL: NULL ptr passed");
                return -EFAULT;
        }
	newData=kmalloc(newBlockSize*sizeof(char*), GFP_KERNEL);
	//fill new data cells, allocate some more
	for(cellIndex=0;cellIndex<oldBlockSize;cellIndex++){
		newData[cellIndex]=block->data[cellIndex];
	}
	for(cellIndex=oldBlockSize;cellIndex<newBlockSize;cellIndex++){
		newData[cellIndex]=kmalloc(cellS*sizeof(char), GFP_KERNEL);
	}

	//reassign pointer and delete pointer table
	kfree(block->data);
	block->data=(char *)newData;
	block->size=newBlockSize;
	//printk(KERN_INFO "SCULL: Cell increase ended\n");

	return 0;
}

////////////////Used to Write Blocks///////////////////////////////////////////////
static ssize_t scull_write(struct file *fp, const char *buf, size_t  count, loff_t *f_pos){
	int err=0;
	struct scull_dev *dev;
	size_t cellCount=0;
	size_t oldBlockSize;
	size_t i=0,j=0,k=0;
        struct storage *block;
	char *buffer;
	//printk(KERN_INFO "SCULL: Writing ");
	//printk(KERN_INFO "SCULL: p1");
	dev= fp->private_data;
	//check for empty device and assign block
        if((f_pos==NULL)||(*f_pos<0)){
                printk(KERN_INFO "SCULL: fpos is either NULL or contains negative value\n");
        }
	if(dev == NULL){
		printk(KERN_ALERT "SCULL: Problem with device");
		return -EFAULT;
	}
	block=dev->entry;
	if(block==NULL){
		block_alloc(1,0,dev);
		dev->entry->blockId = *f_pos;
	}

	//check for block with blkid
	block=dev->entry;
        while((block->blockId != *f_pos)&&(block != NULL)){
                block=block->next;
        }
	//block not found
        if(block == NULL){
                block_alloc(1,0,dev);//allocate empty block
		dev->exit->blockId = *f_pos;
        }

	//search again for the block
        block=dev->entry;
        while((block->blockId != *f_pos)&&(block != NULL)){
                block=block->next;
        }
	if(block == NULL){
		printk(KERN_ALERT "SCULL: Block not found");
		return -EFAULT;
	}
	//block should now be found
	oldBlockSize=block->size;

	cellCount = count/cellS;
	if(count%cellS != 0){
		cellCount++;
	}
	cell_increase(block,cellCount);
	buffer=kmalloc((count+1)*sizeof(char), GFP_KERNEL);

	err=copy_from_user(buffer, buf, count);

        if(err==0){
                printk(KERN_INFO "SCULL: Written %ul characters to block_id:%ul (Used %ul cells) the user", count, *f_pos, cellCount);
        }
        else{
                printk(KERN_INFO "SCULL: Failed to read %d charcters to the user\n", err);
                return -EFAULT;
        }

        for(i=0;i<(cellCount-1);i++){
                for(j=0;j<cellS;j++){
			if(block->data[oldBlockSize+i] == NULL){
				printk(KERN_ALERT "SCULL: Trying to allocate invalid cell (multiple cell instance %ul) at Cell:%ul index:%ul",cellCount,i,j);
				return -EFAULT;
			}
                        block->data[oldBlockSize+i][j]=buffer[i*cellS+j];
                }
        }
        for(k=0;k<(count%cellS);k++){
		if(block->data[oldBlockSize+i] == NULL){
			printk(KERN_ALERT "SCULL: Trying to allocate invalid cell at single Cell:%ul index:%ul offset:%ul",i,j,oldBlockSize);
                        return -EFAULT;
                }
                block->data[oldBlockSize+i][k]=buffer[i*cellS+k];
	        //block->data[oldBlockSize+i][k]='\0';

 	}
 	kfree(buffer);
	//printk(KERN_INFO "SCULL: Writing completed");
	return 0;
}


//////////////Used to open a device///////////////////////////
static int scull_open(struct inode *inode, struct file *fp){
	struct scull_dev *dev=&dev1;//For now reading a global variable. Later will implement using inode.
	fp->private_data = dev;
	numOpens++;
	if((fp->f_flags & O_ACCMODE) == O_WRONLY){
		scull_trim(dev);
		printk(KERN_INFO "SCULL: Device opened in write only. Previous data cleared");
	}
	printk(KERN_INFO "SCULL: Device has been opened %ul time(s)\n", numOpens);
	return 0;
}

/////////////Used to close a device///////////////////////////
static int scull_release(struct inode *inodep, struct file *fp){
/*	struct scull_dev *dev;
	if(fp == NULL){
		printk(KERN_INFO "SCULL: fp is NULL");
	}
	dev=fp->private_data;
	scull_trim(dev);
*/	printk(KERN_INFO "SCULL: Device successfully closed\n");
	return 0;
}


///////////////Used to initialize the device///////////////////
static int __init scull_init(void){
	printk(KERN_INFO "SCULL: Intializing Scull \n");

	majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
	if(majorNumber<0){
		printk(KERN_ALERT "SCULL: Failed to register a major number\n");
		return majorNumber;
	}
	printk(KERN_INFO "SCULL: Major Number %ul registered\n",majorNumber);

	scullClass = class_create(THIS_MODULE, CLASS_NAME);
	if(IS_ERR(scullClass)){
		unregister_chrdev(majorNumber, DEVICE_NAME);
		printk(KERN_ALERT "SCULL: Failed to register device class\n");
		return PTR_ERR(scullClass);
	}
	printk(KERN_INFO "SCULL: Device class registered correctly\n");

	scullDevice = device_create(scullClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
	if(IS_ERR(scullDevice)){
		class_destroy(scullClass);
		unregister_chrdev(majorNumber, DEVICE_NAME);
		printk(KERN_ALERT "SCULL: Failed to create the device\n");
		return PTR_ERR(scullDevice);
	}
	printk(KERN_INFO "SCULL: Device class created correctly\n");
	return 0;
}
 
///////////////Used for cleanup for the device/////////////////////////
static void __exit scull_exit(void){
	scull_trim(&dev1);
	device_destroy(scullClass, MKDEV(majorNumber, 0));
	class_unregister(scullClass);
	class_destroy(scullClass);
	unregister_chrdev(majorNumber, DEVICE_NAME);
	printk(KERN_INFO "SCULL: Goodbye from Scull \n");
}


module_init(scull_init);
module_exit(scull_exit);

