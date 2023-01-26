#ifndef __HDINFO_H_
#define __HDINFO_H_

/* Hd controller regs. Ref: IBM AT Bios-listing */
#define HD_DATA         0x0   /* _CTL when writing */
#define HD_ERROR        0x1   /* see err-bits */
#define HD_NSECTOR      0x2   /* nr of sectors to read/write */
#define HD_SECTOR       0x3   /* starting sector */
#define HD_LCYL         0x4   /* starting cylinder */
#define HD_HCYL         0x5   /* high byte of starting cyl */
#define HD_CURRENT      0x6   /* 101dhhhh , d=drive, hhhh=head */
#define HD_STATUS       0x7   /* see status-bits */
#define HD_PRECOMP HD_ERROR     /* same io address, read=error, write=precomp */
#define HD_COMMAND HD_STATUS    /* same io address, read=status, write=cmd */

#define HD_CMD          0x206


/* Bits of HD_STATUS */
#define ERR_STAT        0x01
#define INDEX_STAT      0x02
#define ECC_STAT        0x04    /* Corrected error */
#define DRQ_STAT        0x08
#define SEEK_STAT       0x10
#define WRERR_STAT      0x20
#define READY_STAT      0x40
#define BUSY_STAT       0x80

#include "netcphd.h"

int hdinfo(pktfmt *pkt);

#endif
