/*
 *Copyright (c) 2017 Vishal SRIVASTAVA <vsriva10@asu.edu>
 *
 *Permission is hereby granted, free of charge, to any person obtaining a copy
 *of this software and associated documentation files (the "Software"), to deal
 *in the Software without restriction, including without limitation the rights 
 *to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *copies of the Software, and to permit persons to whom the Software is
 *furnished to do so, subject to the following conditions:
 *
 *The above copyright notice and this permission notice shall be included in all
 *copies or substantial portions of the Software.
 *
 *THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *SOFTWARE.
 */
#include <linux/clk.h>
#include <linux/crypto.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <crypto/scatterwalk.h>
#include <linux/scatterlist.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <crypto/aes.h>

#define BASE_ADDRESS	0x01C15000

#define CE_CTL		0x00
#define CE_KEY0		0x04
#define CE_KEY1		0x08
#define CE_KEY2		0x0C
#define CE_KEY3		0x10
#define CE_KEY4		0x14
#define CE_KEY5		0x18
#define CE_KEY6		0x1C
#define CE_KEY7		0x20

#define CE_IV0		0x24
#define CE_IV1		0x28
#define CE_IV2		0x2C
#define CE_IV3		0x30
#define CE_IV4		0x34
#define CE_IV5		0x38
#define CE_IV6		0x3C
#define CE_IV7		0x40

#define CE_FCSR		0x44
#define CE_ICSR		0x48
#define CE_MD0		0x4C
#define CE_MD1		0x50
#define CE_MD2		0x54
#define CE_MD3		0x58
#define CE_MD4		0x5C

#define CE_RXFIFO	0x200
#define CE_TXFIFO	0x204

/* For CE_CTL Reg */

#define CE_CTL_CE_KEY		(0 << 24)
#define CE_CTL_PRNG_ONESHOT	(0 << 15)
#define CE_CTL_PRNG_CONT	(1 << 15)
#define CE_CTL_IV_CONST		(0 << 14)
#define CE_CTL_IV_ARB		(1 << 14)
#define CE_CTL_CE_ECB		(0 << 12)
#define CE_CTL_CE_CBC		(1 << 12)
#define CE_CTL_KEYSIZE_AES128	(0 << 8)
#define CE_CTL_KEYSIZE_AES192	(1 << 8)
#define CE_CTL_KEYSIZE_AES256	(2 << 8)
#define CE_CTL_CE_ENCRYPT	(0 << 7)
#define CE_CTL_CE_DECRYPT	(1 << 7)
#define CE_CTL_CE_AES		(0 << 4)
#define CE_CTL_CE_DES		(1 << 4)
#define CE_CTL_CE_3DES		(2 << 4)
#define CE_CTL_CE_SHA1		(3 << 4)
#define CE_CTL_CE_MD5		(4 << 4)
#define CE_CTL_CE_PRNG		(5 << 4)
#define CE_CTL_SHA_DATAEND	(1 << 2)
#define CE_CTL_PRNG_START	(1 << 1)
#define CE_CTL_CE_DIS		(0 << 0)
#define CE_CTL_CE_EN		(1 << 0)

/* For CE_FCSR Reg */

#define CE_FCSR_RXFIFO_NOTFULLFLAG	(1 << 30)
#define CE_FCSR_RXFIFO_WC(val)		((val >> 24) & 0x3f)
#define CE_FCSR_TXFIFO_NOTEMPTYFLAG	(1 << 22)
#define CE_FCSR_TXFIFO_WC(val)		((val >> 16) & 0x07)
#define CE_FCSR_RXFIFO_EMPTYTRIG(val)	((val) & 0x0f)
#define CE_FCSR_TXFIFO_TRIG(val)	((val) & 0x0f)

/* For CE_ICSR */

#define CE_ICSR_RXFIFO_EMPTYNOTPEN	(0 << 10)
#define CE_ICSR_RXFIFO_EMPTYPEN		(1 << 10)
#define CE_ICSR_TXFIFO_DATANOTPEN	(0 << 8)
#define CE_ICSR_TXFIFO_DATAPEN		(1 << 8)
#define CE_ICSR_DRQDIS			(0 << 4)
#define CE_ICSR_DRQEN			(1 << 4)
#define CE_ICSR_RXFIFO_EMPTYINTDIS	(0 << 2)
#define CE_ICSR_RXFIFO_EMPTYINTEN	(1 << 2)
#define CE_ICSR_TXFIFO_DATAINTDIS	(0 << 0)
#define CE_ICSR_TXFIFO_DATAINTEN	(1 << 0)


#define CE_RX_MAX	32
#define CE_RX_DEFAULT	CE_RX_MAX
#define CE_TX_MAX	33

struct crypto_ctx {
	void __iomem *base;
	int irq;
	struct clk *busclk;
	struct clk *devclk;
	struct reset_control *reset;
	struct device *dev;
	struct resource *res;
	spinlock_t slock; /* control the use of the device */
};

struct crypto_tfm_ctx {
	u32 key[AES_MAX_KEY_SIZE / 4];/* divided by sizeof(u32) */
	u32 keylen;
	u32 keymode;
	struct crypto_ctx *ss;
};

int sun4i_ss_cipher_init(struct crypto_tfm *tfm);
int sun4i_ss_aes_setkey(struct crypto_ablkcipher *tfm, const u8 *key, unsigned int keylen);

int sun4i_ss_cbc_aes_encrypt(struct ablkcipher_request *areq);
int sun4i_ss_cbc_aes_decrypt(struct ablkcipher_request *areq);
