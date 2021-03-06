#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/slab.h>

#define DEVICE_NAME "crypto_encrypt"
#define CLASS_NAME "char"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Vishal Srivastava");
MODULE_DESCRIPTION(" Crypto Engine: Encypt ");
MODULE_VERSION("0.1");

static int major_number;
static size_t  num_opens;
static size_t block_ids;
static struct class *crypto_engine_encrypt_class;
static struct device *crypto_engine_encrypt_device;

static int crypto_engine_encrypt_open(struct inode *, struct file *);
static int crypto_engine_encrypt_release(struct inode *, struct file *);
static ssize_t crypto_engine_encrypt_read(
	struct file *,
	char *,
	size_t,
	loff_t *);
static ssize_t crypto_engine_encrypt_write(
	struct file *,
	const char *,
	size_t,
	loff_t *);

static const struct file_operations fops = {
	.owner = THIS_MODULE,
	.open = crypto_engine_encrypt_open,
	.read = crypto_engine_encrypt_read,
	.write = crypto_engine_encrypt_write,
	.release = crypto_engine_encrypt_release,
};

/* Used for Reading a cell in a block */
static ssize_t crypto_engine_encrypt_read(
	struct file *fp,
	char *buf,
	size_t count,
	loff_t *f_pos)
{
	return 0;
}

/* Used to Write Blocks */
static ssize_t crypto_engine_encrypt_write(
	struct file *fp,
	const char *buf,
	size_t  count,
	loff_t *f_pos)
{
	return 0;
}

//////////////Used to open a device///////////////////////////
static int crypto_engine_encrypt_open(struct inode *inode, struct file *fp)
{
	num_opens++;
	if ((fp->f_flags & O_ACCMODE) == O_WRONLY)
		pr_debug("CRYPT_ENCRYPT: Device opened in write only. Previous data cleared");

	pr_debug("CRYPT_ENCRYPT: Device opened %ul time(s)\n", num_opens);
	return 0;
}

/////////////Used to close a device///////////////////////////
static int crypto_engine_encrypt_release(struct inode *inodep, struct file *fp)
{
	pr_debug("CRYPT_ENCRYPT: Device successfully closed\n");
	return 0;
}

///////////////Used to initialize the device///////////////////
static int __init crypto_engine_encrypt_init(void)
{
	pr_debug("CRYPT_ENCRYPT: Intializing Crypto Engine Encrypt\n");
	num_opens = 0;
	major_number = register_chrdev(0, DEVICE_NAME, &fops);
	if (major_number < 0) {
		pr_alert("CRYPT_ENCRYPT: Failed to register a major number\n");
		return major_number;
	}
	pr_debug("CRYPT_ENCRYPT: Major Number %ul registered\n", major_number);

	crypto_engine_encrypt_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(crypto_engine_encrypt_class)) {
		unregister_chrdev(major_number, DEVICE_NAME);
		pr_alert("CRYPT_ENCRYPT: Failed to register device class\n");
		return PTR_ERR(crypto_engine_encrypt_class);
	}
	pr_debug("CRYPT_ENCRYPT: Device class registered correctly\n");

	crypto_engine_encrypt_device =
		device_create(
			crypto_engine_encrypt_class,
			NULL,
			MKDEV(major_number, 0),
			NULL,
			DEVICE_NAME
			);
	if (IS_ERR(crypto_engine_encrypt_device)) {
		class_destroy(crypto_engine_encrypt_class);
		unregister_chrdev(major_number, DEVICE_NAME);
		pr_alert("CRYPT_ENCRYPT: Failed to create the device\n");
		return PTR_ERR(crypto_engine_encrypt_device);
	}
	pr_debug("CRYPT_ENCRYPT: Device class created correctly\n");
	return 0;
}

///////////////Used for cleanup for the device/////////////////////////
static void __exit crypto_engine_encrypt_exit(void)
{
	device_destroy(crypto_engine_encrypt_class, MKDEV(major_number, 0));
	class_unregister(crypto_engine_encrypt_class);
	class_destroy(crypto_engine_encrypt_class);
	unregister_chrdev(major_number, DEVICE_NAME);
	pr_debug("CRYPT_ENCRYPT: Goodbye from Crypto Engine Encrypt\n");
}

module_init(crypto_engine_encrypt_init);
module_exit(crypto_engine_encrypt_exit);
