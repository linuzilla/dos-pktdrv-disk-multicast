#include <stdlib.h>
#include <ctype.h>
#include <dos.h>
#include <stdio.h>
#include <bios.h>
#include <fcntl.h>
#include <string.h>

#include "netcphd.h"
#include "hdinfo.h"

#define TIME_OUT 600000L
static unsigned int  ctrl_base[4] = { 0x1f0, 0x170, 0xf0, 0x70 };
static unsigned int  dd[256];    // DiskData
static unsigned int  dd_off;     // DiskData offset
static unsigned int  bios_cyl[2], bios_head[2], bios_sec[2];

static char *getascii (unsigned int in_data [], int off_start, int off_end)
{
    static char ret_val [255];
    int         i, j, loop, loop1;

    for (loop = off_start, loop1 = 0; loop <= off_end; loop++) {
        ret_val [loop1++] = (char) (in_data [loop] / 256);  /* Get High byte */
        ret_val [loop1++] = (char) (in_data [loop] % 256);  /* Get Low byte */
    }
    ret_val [loop1] = '\0';  /* Make sure it ends in a NULL character */

    for (loop = loop1 - 1; (loop >= 0) && (ret_val[loop] == ' '); loop--)
        ret_val[loop] = '\0';
    loop++;

    for (loop1 = 0; (loop1 < loop) && (ret_val[loop1] == ' '); loop1++)
        ;

    if (loop1 != 0)
        for (i = 0, j = loop1; j <= loop; i++, j++)
            ret_val[i] = ret_val[j];

    return (ret_val);
}

int hdinfo(pktfmt *pkt)
{
    int    loop, di;
    int    cntr;          // Number of controllers we try
    long   retry;
    int    idx = 0;

    for (cntr = 0; cntr < 4; cntr++) {
        int  go_outer_loop = 0;
        for (loop = 0; loop < 2; loop++) {
            di = 0;
            retry = TIME_OUT;
            while ((di != 0x50) && (--retry))
                di = inp(ctrl_base[cntr] + HD_STATUS);
            if (! retry) {
                go_outer_loop = 1;
                break;
            }
            outp(ctrl_base[cntr] + HD_CURRENT, (loop == 0 ? 0xA0 : 0xB0));
            outp(ctrl_base[cntr] + HD_COMMAND, 0xEC);

            retry = TIME_OUT;

            di = 0;
            while ((di != 0x58) && (--retry))
                di = inp(ctrl_base[cntr] + HD_STATUS);
            if (! retry) {
                if (loop)
                    outp(ctrl_base[cntr] + HD_CURRENT, 0xA0);
                continue;
            }

            for (dd_off = 0; dd_off != 256; dd_off++)
                dd[dd_off] = inpw(ctrl_base[cntr] + HD_DATA);

            if (ctrl_base[cntr] == 0x1F0) {
                union REGS  registers;

                registers.h.ah = 0x8;
                registers.h.dl = 0x80 + loop;
                int86(0x13, &registers, &registers);

                if (! registers.x.cflag) {
                    bios_head[loop] = registers.h.dh + 1; /* Heads are from 0 */
                    bios_sec [loop] = registers.h.cl & 0x3F; /* sec is bits 5 - 0 */
                    bios_cyl [loop] = ((registers.h.cl & 0xC0) << 2) + registers.h.ch + 2; /* +1 because starts from 0 and +1 for FDISK leaving one out */
                }
            } else {
                bios_head[loop] = 0;
                bios_sec [loop] = 0;
                bios_cyl [loop] = 0;
            }

            pkt->b.toc[idx].drv      = 0x80 + loop;
            pkt->b.toc[idx].cylinder = bios_cyl [loop];
            pkt->b.toc[idx].head     = bios_head[loop];
            pkt->b.toc[idx].sector   = bios_sec [loop];
            pkt->b.toc[idx].len      = bios_cyl [loop];
            strcpy(pkt->b.toc[idx].signature, getascii(dd, 27, 46));
            idx++;

            printf("Model  Number: %s\n", getascii(dd, 27, 46));
            printf("Serial Number: %s\n", getascii(dd, 10, 19));
            printf("DISK  reports: CHS=%u/%u/%u (default), CHS=%i/%i/%i (current)\n",
                    dd[1], dd[3], dd[6],
                    (dd[53] & 1) ? dd[54] : -1, (dd[53] & 1) ? dd[55] : -1, (dd[53] & 1) ? dd[56] : -1);
            printf("BIOS  reports: CHS=%d/%d/%d\n", bios_cyl[loop], bios_head[loop], bios_sec[loop]);
            printf("Controller Revision: %-18s", getascii(dd, 23, 26));
            printf("Buffer Size         : %u kB\n", dd[21] >> 1);
            printf("# of ECC bytes     : %-18u", dd[22]);
            printf("DMA support         : %s\n", (dd[49] & 256) ? "Yes" : "No");
            printf("# of secs/interrupt: %-18u", 0xff & dd[47]);
            printf("Double Word Transfer: %s\n", dd[48] == 0 ? "No" : "Yes");
            printf("LBA support: %s", (dd[49] & 512) ? "Yes .... " : "No\n");
            if (dd[49] & 512) {
                printf("%6.1fMB of LBA addressable",
                    (((float)dd[61] * 65536L + (float) dd[60]) / 2048));
                if (dd[53] &1)
                    printf(" %6.1fMB in CHS mode\n",
                    (((float)dd[58] * 65536L + (float) dd[57]) / 2048));
                else
                    printf("\n");
            }
        }
        if (go_outer_loop)
            break;
    }
    return idx;
}
