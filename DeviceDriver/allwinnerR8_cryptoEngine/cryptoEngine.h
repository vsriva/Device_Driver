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

#define BASE_ADDRESS 0x01C15000

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



