/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://chibios-gfx.com/license.html
 */

#ifndef _NT35510_H_
#define _NT35510_H_

#define NT35510_NOP     0x00
#define NT35510_SWRESET 0x01
#define NT35510_RDDID   0x04
#define NT35510_RDDST   0x09

#define NT35510_SLPIN   0x10
#define NT35510_SLPOUT  0x11
#define NT35510_PTLON   0x12
#define NT35510_NORON   0x13

#define NT35510_INVOFF  0x20
#define NT35510_INVON   0x21
#define NT35510_DISPOFF 0x28
#define NT35510_DISPON  0x29
#define NT35510_CASET   0x2A00
#define NT35510_RASET   0x2B00
#define NT35510_RAMWR   0x2C00
#define NT35510_RAMRD   0x2E00

#define NT35510_PTLAR   0x30
#define NT35510_COLMOD  0x3A
#define NT35510_MADCTL  0x3600

#define NT35510_FRMCTR1 0xB1
#define NT35510_FRMCTR2 0xB2
#define NT35510_FRMCTR3 0xB3
#define NT35510_INVCTR  0xB4
#define NT35510_DISSET5 0xB600

#define NT35510_PWCTR1  0xC0
#define NT35510_PWCTR2  0xC1
#define NT35510_PWCTR3  0xC2
#define NT35510_PWCTR4  0xC3
#define NT35510_PWCTR5  0xC4
#define NT35510_VMCTR1  0xC5

#define NT35510_RDID1   0xDA
#define NT35510_RDID2   0xDB
#define NT35510_RDID3   0xDC
#define NT35510_RDID4   0xDD

#define NT35510_PWCTR6  0xFC

#define NT35510_GMCTRP1 0xE0
#define NT35510_GMCTRN1 0xE1

#endif /* _NT35510_H_ */
