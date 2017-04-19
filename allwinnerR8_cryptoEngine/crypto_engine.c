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
#include <linux/scatterwalk.h>
#include <linux/scatterlist.h>
#include <linux/delay.h>
#include <linux/reset.h>
#include <linux/interrupt.h>

#include "cryptoEngine.h"

struct crypto_alg crypto = {
	.cra_name = "ecb(aes)",
	.cra_driver_name = "ecb-aes-crypto",
	.cra_priority = 300,
	.cra_blocksize = AES_BLOCK_SIZE,
	.cra_flags = CRYPTO_ALG_TYPE_ABLKCIPHER,
	.cra_ctxsize = sizeof(struct crypto_tfm_ctx),
	.cra_module = THIS_MODULE,
	.cra_alignmask = 3,
	.cra_type = &crypto_ablkcipher_type, //To find
	.cra_init = sun4i_ss_cipher_init, //To do
	.cra_ablkcipher = {
		.min_keysize	= AES_MIN_KEY_SIZE, //To find
		.max_keysize	= AES_MAX_KEY_SIZE, //To find
		.ivsize		= AES_BLOCK_SIZE, //To find
		.setkey         = sun4i_ss_aes_setkey, //To do
		.encrypt        = sun4i_ss_ecb_aes_encrypt, //To do
		.decrypt        = sun4i_ss_ecb_aes_decrypt, //To do
	}
};

static int crypto_probe(struct platform_device *pdev)
{
	struct resource *res;
	u32 v;
	int err, i;
	unsigned long cr;
	const unsigned long clk_ahb = 24 * 1000 * 1000;
	const unsigned long clk_mod = 24 * 1000 * 1000;
	struct crypto_ctx *ss;
	
	
	if (!pdev->dev.of_node)
		return -ENODEV;
	
	ss = devm_kzalloc(&pdev->dev, sizeof(*ss), GFP_KERNEL);
	if(!ss)
		return -ENOMEM;
	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ss->base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(ss->base)) {
		dev_err(&pdev->dev, "Cannot request MMIO\n");
		return PTR_ERR(ss->base);
	}
	dev_dbg(&pdev->dev, "I/O remap done\n");
	
	ss->devclk = devm_clk_get(&pdev->dev, "mod");
	if (IS_ERR(ss->devclk)) {
		dev_err(&pdev->dev, "Problem with device clk, err:%d\n", err);
		return err;
	}
	dev_dbg(&pdev->dev, "Device clk done\n");
	
	ss->busclk = devm_clk_get(&pdev->dev, "ahb");
	if (IS_ERR(ss->busclk)) {
		dev_err(&pdev->dev, "Problem with bus clk, err:%d\n", err);
		return err;
	}
	dev_dbg(&pdev->dev, "Bus clk done\n");
	
	ss->reset = devm_reset_control_get_optional(&pdev->dev, "ahb");
	if (IS_ERR(ss->reset)) {
		dev_err(&pdev->dev, "Reset problem\n");
		return PTR_ERR(ss->reset);
	}
	
	err = clk_prepare_enable(ss->busclk);
	if (err != 0) {
		dev_err(&pdev->dev, "Cannot enable bus clk\n");
		return err;
	}
	
	err = clk_prepare_enable(ss->devclk);
	if (err != 0) {
		dev_err(&pdev->dev, "Cannot enable device clk\n");
		clk_disable_unprepare(ss->busclk);
		return err;//error_ssclk
	}
	
	err = clk_set_rate(ss->devclk, clk_mod);
	if(err != 0) {
		dev_err(&pdev->dev, "Cannot set device clk\n");
		clk_disable_unprepare(ss->devclk);
		clk_disable_unprepare(ss->busclk);
		return err;//error_clk
	}
	
	if (ss->reset) {
		err = reset_control_deassert(ss->reset);
		if (err) {
			dev_err(&pdev->dev, "Failed to deassert reset\n");
			clk_disable_unprepare(ss->devclk);
			clk_disable_unprepare(ss->busclk);
			return err;//error_clk
		}
	}
	cr = clk_get_rate(ss->busclk);
	dev_dbg(&pdev->dev, "Bus clk set at %lu (KHz)\n", cr/1000);
	
	writel(CE_CTL_CE_EN, ss->base + CE_CTL);
	v = readl(ss->base + CE_CTL);
	v = v >> 16;
	v = v & 0x07;
	dev_info(&pdev->dev ,"Die ID:%d", v);
	writel(0, ss->base + CE_CTL);
	
	ss->dev = &pdev->dev;
	
	spin_lock_init(&ss->slock);
	
	err = crypto_register_alg(&crypto);
	if(err != 0) {//error_alg
		dev_err(ss->dev, "Crypto register failed\n");
		crypto_unregister_alg(&crypto);
		if(ss->reset)
			reset_control_assert(ss->reset);
		clk_disable_unprepare(ss->devclk);
		clk_disable_unprepare(ss->busclk);
		return err;
	}
	return 0;
}
		

static int crypto_remove(struct platform_device *pdev)
{
	crypto_unregister_alg(crypto);
	writel(0, ss->base + CE_CTL);
	if (ss->reset)
		reset_control_assert(ss->reset);
	clk_disable_unprepare(ss->busclk);
	clk_disable_unprepare(ss->devclk);
	return 0;
}

static const struct of_device_id r8_crypto_of_match_table[] = {
	{	.compatible = "allwinner,r8-crypto" },
	{}
};

MODULE_DEVICE_TABLE(of, r8_crypto_of_match_table);

static struct platform_driver crypto_driver = {
	.probe	= crypto_probe,
	.remove	= crypto_remove,
	.driver	= {
		.name	= "crypto",
		.of_match_table	= r8_crypto_of_match_table,
	},
};

module_platform_driver(crypto_driver);

MODULE_DESCRIPTION("Allwinner R8 Crypto Engine");
MODULE_LICENCE("GPL");
MODULE_AUTHOR("Vishal Srivastava <vsriva10@asu.edu>");

