/* Rev:$Revision: 1.1 $ */
/* Copyright $Date: 2009/11/05 00:00:38 $ */ 
/*******************************************************************************
 *
 *   Copyright 2007 NetBurner, Inc.  ALL RIGHTS RESERVED
 *
 *   Permission is hereby granted to purchasers of NetBurner hardware to use or
 *   modify this computer program for any use as long as the resultant program
 *   is only executed on NetBurner provided hardware.
 *
 *   No other rights to use this program or its derivatives in part or in whole
 *   are granted.
 *
 *   It may be possible to license this or other NetBurner software for use on
 *   non-NetBurner hardware. Please contact licensing@netburner.com for more
 *   information.
 *
 *   NetBurner makes no representation or warranties with respect to the
 *   performance of this computer program, and specifically disclaims any
 *   responsibility for any damages, special or consequential, connected with
 *   the use of this program.
 *
 *   NetBurner, Inc.
 *   5405 Morehouse Drive
 *   San Diego, CA  92121
 *   http://www.netburner.com
 *
 ******************************************************************************/

/* Select the card type */
#ifndef _CARDTYPE_H
#define _CARDTYPE_H

/* 
  Change between devices by uncommenting one or the other. You cannot
  use both types at the same time. Whenever you change card types, you
  should do a make clean on your project. 
  
  Warning! You must have CFC bus interface hardware on your platform or
  the code will repeatedly trap. If you are getting traps you will need to
  perform an application download using the monitor program to recover. 
*/
#define USE_MMC         // SD/MMC cards
//#define USE_CFC    // Compact Flash cards

#if (defined USE_CFC)
#define EXT_FLASH_DRV_NUM (CFC_DRV_NUM)
#else
#define EXT_FLASH_DRV_NUM (MMC_DRV_NUM)
#endif

#endif   /* _CARDTYPE_H */
 
