#include <stdio.h>
#include <string.h>
#include <conio.h>
#include <dos.h>
#include <time.h>

#include "netcphd.h"

const char             pkt_type_server[] = "\x19\x97\s";
const char             pkt_type_client[] = "\x19\x98\c";

unsigned int           buffer_size = 0, buffer_front = 0, buffer_rear = 0;
unsigned int           buffer_data = 0;
pktfmt huge            *pktbuffer;
unsigned int           *pktlen;
unsigned long          pkt_received = 0L, pkt_dropped = 0L;
volatile unsigned int  control_c_counter = 0;
static int             buffer_copy_complete = 1;



void fixchecksum(pktfmt *pkt, int len)
{
    unsigned char *ptr = (unsigned char *) pkt;
    unsigned int  sum  = 0, i;

    pkt->h.length = len;
    pkt->h.checksum = 0;

    for (i = 0; i < len; i++)
        sum += ptr[i];

    pkt->h.checksum = sum;
}

int check_checksum(pktfmt *pkt)
{
    unsigned int  sum;

    sum = pkt->h.checksum;
    fixchecksum(pkt, pkt->h.length);
#ifdef DEBUG
    if (sum != pkt->h.checksum) {
        printf("\nchecksum = %d, should be %d\n", sum, pkt->h.checksum);
    }
#endif
    return (sum == pkt->h.checksum);
}

void clear_keyboard_buffer()
{
    while (kbhit())
        getch();
}

char *print_ether(const unsigned char *buf)
{
    static char  ether[20];
    sprintf(ether, "%02x:%02x:%02x:%02x:%02x:%02x",
            buf[0], buf[1], buf[2], buf[3], buf[4], buf[5]);
    return ether;
}

#pragma warn -parm
void interrupt far control_c(unsigned bp, unsigned di, unsigned si,
                             unsigned ds, unsigned es, unsigned dx,
                             unsigned cx, unsigned bx, unsigned ax)
{
    control_c_counter++;
}

volatile int recv_pkt(pktfmt *pkt)
{
    if (buffer_data) {
        int   len = pktlen[buffer_front];
        farmemcpy((char far *) pkt, &pktbuffer[buffer_front], len);
        buffer_front = (buffer_front + 1) % buffer_size;
        disable();
        buffer_data--;
        enable();
        return len;
    }
    return 0;
}

int read_pkt(pktfmt *pkt)
{
    volatile int   len;

    control_c_counter = 0;

    do {
        while ((len = recv_pkt(pkt)) == 0) {
            if (control_c_counter > 1)
                return 0;
        }
    } while (! check_checksum(pkt));
    return len;
}

int expect_pkt(pktfmt *pkt, int pktcmd, int timedelay)
{
    int     len;
    time_t  current, last;

    while (1) {
        last = time(NULL);

        while ((len = recv_pkt(pkt)) == 0) {
             current = time(NULL);
             if (difftime(current, last) > timedelay)
                 return 0;
        }

        if ((pkt->h.cmd == pktcmd) && check_checksum(pkt)) {
            return len;
#ifdef DEBUG
        } else {
            printf("\ncmd = %d (%d), checksum = %d\n", pkt->h.cmd, pktcmd, pkt->h.checksum);
#endif
        }

    }
}

void interrupt far receiver (unsigned bp, unsigned di, unsigned si,
                             unsigned ds, unsigned es, unsigned dx,
                             unsigned cx, unsigned bx, unsigned ax)
{
    if (! ax) {                    /*   AX == 0  (request a buffer)   */
        if ((buffer_data < buffer_size) && (cx <= sizeof(pktfmt))) {
            register unsigned   acc;
            es = FP_SEG(&pktbuffer[buffer_rear]);
            di = FP_OFF(&pktbuffer[buffer_rear]);
            acc = di >> 4;
            es += acc;
            di &= 0xF;
            pktlen[buffer_rear] = cx;
            buffer_rear = (buffer_rear + 1) % buffer_size;
            pkt_received++;
            buffer_copy_complete = 0;
        } else {
            es = di = 0;
            pkt_dropped++;
        }
    } else {                        /*   AX == 1  (copy completed)     */
        buffer_copy_complete = 1;
        buffer_data++;
    }
}


void flush_buffer()
{
    disable();
    buffer_data = 0;
    buffer_front = buffer_rear;
    if (! buffer_copy_complete)
        buffer_front = (buffer_front + buffer_size - 1) % buffer_size;
    enable();
}
#pragma warn +parm
