/*
 * Copyright (C) 2013-2015 Corentin LABBE <clabbe.montjoie@gmail.com>
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
 
#include "crypto_engine.h"

 /* Generic function that support SG with size not multiple of 4 */
static int sun4i_ss_opti_poll(struct ablkcipher_request *areq)
{
	struct crypto_ablkcipher *tfm = crypto_ablkcipher_reqtfm(areq);
	struct sun4i_tfm_ctx *op = crypto_ablkcipher_ctx(tfm);
	struct sun4i_ss_ctx *ss = op->ss;
	unsigned int ivsize = crypto_ablkcipher_ivsize(tfm);
	struct sun4i_cipher_req_ctx *ctx = ablkcipher_request_ctx(areq);
	u32 mode = ctx->mode;
	/* when activating SS, the default FIFO space is SS_RX_DEFAULT(32) */
	u32 rx_cnt = CE_RX_DEFAULT;
	u32 tx_cnt = 0;
	u32 spaces;
	u32 v;
	int err = 0;
	unsigned int i;
	unsigned int ileft = areq->nbytes;
	unsigned int oleft = areq->nbytes;
	unsigned int todo;
	struct sg_mapping_iter mi, mo;
	unsigned int oi, oo; /* offset for in and out */
	unsigned long flags;

	if (areq->nbytes == 0)
		return 0;

	if (!areq->info) {
		dev_err_ratelimited(ss->dev, "ERROR: Empty IV\n");
		return -EINVAL;
	}

	if (!areq->src || !areq->dst) {
		dev_err_ratelimited(ss->dev, "ERROR: Some SGs are NULL\n");
		return -EINVAL;
	}

	spin_lock_irqsave(&ss->slock, flags);

	for (i = 0; i < op->keylen; i += 4)
		writel(*(op->key + i / 4), ss->base + CE_KEY0 + i);

	if (areq->info) {
		for (i = 0; i < 4 && i < ivsize / 4; i++) {
			v = *(u32 *)(areq->info + i * 4);
			writel(v, ss->base + CE_IV0 + i * 4);
		}
	}
	writel(mode, ss->base + CE_CTL);

	sg_miter_start(&mi, areq->src, sg_nents(areq->src),
		       SG_MITER_FROM_SG | SG_MITER_ATOMIC);
	sg_miter_start(&mo, areq->dst, sg_nents(areq->dst),
		       SG_MITER_TO_SG | SG_MITER_ATOMIC);
	sg_miter_next(&mi);
	sg_miter_next(&mo);
	if (!mi.addr || !mo.addr) {
		dev_err_ratelimited(ss->dev, "ERROR: sg_miter return null\n");
		err = -EINVAL;
		goto release_ss;
	}

	ileft = areq->nbytes / 4;
	oleft = areq->nbytes / 4;
	oi = 0;
	oo = 0;
	do {
		todo = min3(rx_cnt, ileft, (mi.length - oi) / 4);
		if (todo > 0) {
			ileft -= todo;
			writesl(ss->base + CE_RXFIFO, mi.addr + oi, todo);
			oi += todo * 4;
		}
		if (oi == mi.length) {
			sg_miter_next(&mi);
			oi = 0;
		}

		spaces = readl(ss->base + CE_FCSR);
		rx_cnt = CE_FCSR_RXFIFO_WC(spaces);
		tx_cnt = CE_FCSR_TXFIFO_WC(spaces);

		todo = min3(tx_cnt, oleft, (mo.length - oo) / 4);
		if (todo > 0) {
			oleft -= todo;
			readsl(ss->base + CE_TXFIFO, mo.addr + oo, todo);
			oo += todo * 4;
		}
		if (oo == mo.length) {
			sg_miter_next(&mo);
			oo = 0;
		}
	} while (oleft > 0);

	if (areq->info) {
		for (i = 0; i < 4 && i < ivsize / 4; i++) {
			v = readl(ss->base + CE_IV0 + i * 4);
			*(u32 *)(areq->info + i * 4) = v;
		}
	}

release_ss:
	sg_miter_stop(&mi);
	sg_miter_stop(&mo);
	writel(0, ss->base + CE_CTL);
	spin_unlock_irqrestore(&ss->slock, flags);
	return err;
}


static int sun4i_ss_cipher_poll(struct ablkcipher_request *areq)
{
	struct crypto_ablkcipher *tfm = crypto_ablkcipher_reqtfm(areq);
	struct sun4i_tfm_ctx *op = crypto_ablkcipher_ctx(tfm);
	struct sun4i_ss_ctx *ss = op->ss;
	int no_chunk = 1;
	struct scatterlist *in_sg = areq->src;
	struct scatterlist *out_sg = areq->dst;
	unsigned int ivsize = crypto_ablkcipher_ivsize(tfm);
	struct sun4i_cipher_req_ctx *ctx = ablkcipher_request_ctx(areq);
	u32 mode = ctx->mode;
	/* when activating SS, the default FIFO space is CE_RX_DEFAULT(32) */
	u32 rx_cnt = CE_RX_DEFAULT;
	u32 tx_cnt = 0;
	u32 v;
	u32 spaces;
	int err = 0;
	unsigned int i;
	unsigned int ileft = areq->nbytes;
	unsigned int oleft = areq->nbytes;
	unsigned int todo;
	struct sg_mapping_iter mi, mo;
	unsigned int oi, oo;	/* offset for in and out */
	char buf[4 * CE_RX_MAX];/* buffer for linearize SG src */
	char bufo[4 * CE_TX_MAX]; /* buffer for linearize SG dst */
	unsigned int ob = 0;	/* offset in buf */
	unsigned int obo = 0;	/* offset in bufo*/
	unsigned int obl = 0;	/* length of data in bufo */
	unsigned long flags;

	if (areq->nbytes == 0)
		return 0;

	if (!areq->info) {
		dev_err_ratelimited(ss->dev, "ERROR: Empty IV\n");
		return -EINVAL;
	}

	if (!areq->src || !areq->dst) {
		dev_err_ratelimited(ss->dev, "ERROR: Some SGs are NULL\n");
		return -EINVAL;
	}

	/*
	 * if we have only SGs with size multiple of 4,
	 * we can use the SS optimized function
	 */
	while (in_sg && no_chunk == 1) {
		if ((in_sg->length % 4) != 0)
			no_chunk = 0;
		in_sg = sg_next(in_sg);
	}
	while (out_sg && no_chunk == 1) {
		if ((out_sg->length % 4) != 0)
			no_chunk = 0;
		out_sg = sg_next(out_sg);
	}

	if (no_chunk == 1)
		return sun4i_ss_opti_poll(areq);

	spin_lock_irqsave(&ss->slock, flags);

	for (i = 0; i < op->keylen; i += 4)
		writel(*(op->key + i / 4), ss->base + CE_KEY0 + i);

	if (areq->info) {
		for (i = 0; i < 4 && i < ivsize / 4; i++) {
			v = *(u32 *)(areq->info + i * 4);
			writel(v, ss->base + CE_IV0 + i * 4);
		}
	}
	writel(mode, ss->base + CE_CTL);

	sg_miter_start(&mi, areq->src, sg_nents(areq->src),
		       SG_MITER_FROM_SG | SG_MITER_ATOMIC);
	sg_miter_start(&mo, areq->dst, sg_nents(areq->dst),
		       SG_MITER_TO_SG | SG_MITER_ATOMIC);
	sg_miter_next(&mi);
	sg_miter_next(&mo);
	if (!mi.addr || !mo.addr) {
		dev_err_ratelimited(ss->dev, "ERROR: sg_miter return null\n");
		err = -EINVAL;
		goto release_ss;
	}
	ileft = areq->nbytes;
	oleft = areq->nbytes;
	oi = 0;
	oo = 0;

	while (oleft > 0) {
		if (ileft > 0) {
			/*
			 * todo is the number of consecutive 4byte word that we
			 * can read from current SG
			 */
			todo = min3(rx_cnt, ileft / 4, (mi.length - oi) / 4);
			if (todo > 0 && ob == 0) {
				writesl(ss->base + CE_RXFIFO, mi.addr + oi,
					todo);
				ileft -= todo * 4;
				oi += todo * 4;
			} else {
				/*
				 * not enough consecutive bytes, so we need to
				 * linearize in buf. todo is in bytes
				 * After that copy, if we have a multiple of 4
				 * we need to be able to write all buf in one
				 * pass, so it is why we min() with rx_cnt
				 */
				todo = min3(rx_cnt * 4 - ob, ileft,
					    mi.length - oi);
				memcpy(buf + ob, mi.addr + oi, todo);
				ileft -= todo;
				oi += todo;
				ob += todo;
				if (ob % 4 == 0) {
					writesl(ss->base + CE_RXFIFO, buf,
						ob / 4);
					ob = 0;
				}
			}
			if (oi == mi.length) {
				sg_miter_next(&mi);
				oi = 0;
			}
		}

		spaces = readl(ss->base + CE_FCSR);
		rx_cnt = CE_RXFIFO_WC(spaces);
		tx_cnt = CE_TXFIFO_WC(spaces);
		dev_dbg(ss->dev, "%x %u/%u %u/%u cnt=%u %u/%u %u/%u cnt=%u %u\n",
			mode,
			oi, mi.length, ileft, areq->nbytes, rx_cnt,
			oo, mo.length, oleft, areq->nbytes, tx_cnt, ob);

		if (tx_cnt == 0)
			continue;
		/* todo in 4bytes word */
		todo = min3(tx_cnt, oleft / 4, (mo.length - oo) / 4);
		if (todo > 0) {
			readsl(ss->base + CE_TXFIFO, mo.addr + oo, todo);
			oleft -= todo * 4;
			oo += todo * 4;
			if (oo == mo.length) {
				sg_miter_next(&mo);
				oo = 0;
			}
		} else {
			/*
			 * read obl bytes in bufo, we read at maximum for
			 * emptying the device
			 */
			readsl(ss->base + CE_TXFIFO, bufo, tx_cnt);
			obl = tx_cnt * 4;
			obo = 0;
			do {
				/*
				 * how many bytes we can copy ?
				 * no more than remaining SG size
				 * no more than remaining buffer
				 * no need to test against oleft
				 */
				todo = min(mo.length - oo, obl - obo);
				memcpy(mo.addr + oo, bufo + obo, todo);
				oleft -= todo;
				obo += todo;
				oo += todo;
				if (oo == mo.length) {
					sg_miter_next(&mo);
					oo = 0;
				}
			} while (obo < obl);
			/* bufo must be fully used here */
		}
	}
	if (areq->info) {
		for (i = 0; i < 4 && i < ivsize / 4; i++) {
			v = readl(ss->base + CE_IV0 + i * 4);
			*(u32 *)(areq->info + i * 4) = v;
		}
	}

release_ss:
	sg_miter_stop(&mi);
	sg_miter_stop(&mo);
	writel(0, ss->base + CE_CTL);
	spin_unlock_irqrestore(&ss->slock, flags);

	return err;
}

int sun4i_ss_cipher_init(struct crypto_tfm *tfm)
{
	struct sun4i_tfm_ctx *op = crypto_tfm_ctx(tfm);
	struct crypto_alg *alg = tfm->__crt_alg;
	struct sun4i_ss_alg_template *algt;

	memset(op, 0, sizeof(struct sun4i_tfm_ctx));

	algt = container_of(alg, struct sun4i_ss_alg_template, alg.crypto);
	op->ss = algt->ss;

	tfm->crt_ablkcipher.reqsize = sizeof(struct sun4i_cipher_req_ctx);

	return 0;
}

/* check and set the AES key, prepare the mode to be used */
int sun4i_ss_aes_setkey(struct crypto_ablkcipher *tfm, const u8 *key,
			unsigned int keylen)
{
	struct sun4i_tfm_ctx *op = crypto_ablkcipher_ctx(tfm);
	struct sun4i_ss_ctx *ss = op->ss;

	switch (keylen) {
	case 128 / 8:
		op->keymode = CE_CTL_KEYSIZE_AES128;
		break;
	case 192 / 8:
		op->keymode = CE_CTL_KEYSIZE_AES192;
		break;
	case 256 / 8:
		op->keymode = CE_CTL_KEYSIZE_AES256;
		break;
	default:
		dev_err(ss->dev, "ERROR: Invalid keylen %u\n", keylen);
		crypto_ablkcipher_set_flags(tfm, CRYPTO_TFM_RES_BAD_KEY_LEN);
		return -EINVAL;
	}
	op->keylen = keylen;
	memcpy(op->key, key, keylen);
	return 0;
}

/* CBC AES */
int sun4i_ss_cbc_aes_encrypt(struct ablkcipher_request *areq)
{
	struct crypto_ablkcipher *tfm = crypto_ablkcipher_reqtfm(areq);
	struct sun4i_tfm_ctx *op = crypto_ablkcipher_ctx(tfm);
	struct sun4i_cipher_req_ctx *rctx = ablkcipher_request_ctx(areq);

	rctx->mode = CE_CTL_CE_AES | CE_CTL_CE_CBC | CE_CTL_CE_EN | CE_CTL_CE_ENCRYPT |
		op->keymode;
	return sun4i_ss_cipher_poll(areq);
}

int sun4i_ss_cbc_aes_decrypt(struct ablkcipher_request *areq)
{
	struct crypto_ablkcipher *tfm = crypto_ablkcipher_reqtfm(areq);
	struct sun4i_tfm_ctx *op = crypto_ablkcipher_ctx(tfm);
	struct sun4i_cipher_req_ctx *rctx = ablkcipher_request_ctx(areq);

	rctx->mode = CE_CTL_CE_AES | CE_CTL_CE_CBC | CE_CTL_CE_EN | CE_CTL_CE_DECRYPT |
		op->keymode;
	return sun4i_ss_cipher_poll(areq);
}
