/*  hdserv.c  -- */

#include <stdio.h>
#include <dos.h>
#include <bios.h>
#include <conio.h>
#include <alloc.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "netcphd.h"
#include "hdinfo.h"

#define TIMEOUT       3
#define DEFAULT_ATTR  0x1E

#define MAX_SECTOR_SIZE  63

/* Global Variables */


char                 myEtherAddr[6];
static int           verbose = 1, testonly = 0, passive = 0;


int  expect_pkt(pktfmt *pkt, int pktcmd, int sec);

int  copy_remote_harddisk(unsigned char *eth, int drv, int beg_cyl, int cyl, int head, int sec, int flag);
int  send_recv_and_write(const pktfmt *pkt);
int  partition_table(const unsigned char *eth, const int drv, int cylinder, int head, int sector, int *st_cyl, int *en_cyl);
int  send_and_recv(const pktfmt *pkt, pktfmt *pktin);
void writeTrack(int drv, int head, int cylinder, int sector, int len, void *buffer);
void reboot(void);


static const char *drvfcn[] = {
    "basic", "extended", "", "", "high-performance", "full"
};

static const char banner[] = "-\\|/";

char     *disk_buffer;
int      local_cylinder, local_head, local_sector;



int main(int argc, char *argv[])
{
    int                  vector, handle;
    int                  version, iclass, itype, inum;
    char                 dname[20];
    void interrupt       (*ctrl_c)(), (*ctrl_brk)();
    int                  drvno, retry;
    pktfmt               pkts, pkt;
    int                  i, select, erropt;
    int                  drv, cylinder, head, sector;
    int                  partition;
    int                  pktfcn;
    char                 ServerEtherAddr[6];
    int                  rebootFlag = 0;

    for (erropt = 0, i = 1; i < argc; i++) {
        int  j;

        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "--passive") == 0) {
                passive = 1;
                continue;
            }

            for (j = 1; argv[i][j] != '\0'; j++) {
                switch(argv[i][j]) {
                case 'i':
                    hdinfo(&pkts);
                    printf("\n%s CHS=%u/%u/%u\n\n", pkts.b.toc[0].signature,
                                                    pkts.b.toc[0].cylinder ,
                                                    pkts.b.toc[0].head     ,
                                                    pkts.b.toc[0].sector);
                    return 1;
                case 'v':
                    verbose++;
                    break;
                case 'q':
                    verbose = -100;
                    break;
                case 't':
                    testonly = 1;
                    break;
                default:
                    erropt = 1;
                }
            }
        } else {
            erropt = 1;
        }
    }

    if (erropt) {
        printf("usage: hdcopy [-i][-v][-q][-t][--passive]\n");
        return 1;
    }


    buffer_size = (unsigned int) (farcoreleft() / sizeof(pktfmt));

    if ((pktlen = calloc(buffer_size, sizeof(unsigned int))) == NULL) {
        printf("Out of memory !!\n");
        return 1;
    }

    if ((pktbuffer = farcalloc(buffer_size, sizeof(pktfmt))) == NULL) {
        printf("Out of memory !!\n");
        return 1;
    }

    if ((vector = initial_pktdrv()) == 0) {
        printf("Packet Driver not found\n");
        return 2;
    }

    if ((handle = access_type(1, 0xFFFF, 0, pkt_type_client, PKT_TYPE_LEN, (RECEIVER) receiver)) == 0) {
        printf("Packet Driver error: access_type\n");
        return 5;
    }

    // reset_interface(handle);

    if (get_address(handle, myEtherAddr, 6) == 0) {
        printf("Packet Driver error: can't get Ethernet Address\n");
        release_type(handle);
        return 6;
    }

    if ((pktfcn = driver_info(handle, &version, &iclass, &itype, &inum, dname)) == 0) {
        printf("Packet Driver error: can't get driver info\n");
        release_type(handle);
        return 7;
    }

    if (set_rcv_mode(handle, passive ? RCV_IF_BROAD : RCV_INTERFACE) == 0) {
         printf("Packet Driver error: can't set receive mode\n");
    }

    ctrl_c   = getvect(0x1B);
    ctrl_brk = getvect(0x23);
    setvect(0x1B, control_c);
    setvect(0x23, control_c);

    setattr(DEFAULT_ATTR, wherex(), wherey(), 80, 25);
    printf("\nHDcopy v%s (c) 1997 by Jiann-Ching Liu, Computer Center of N.C.U.\n\n", VERSION);
    printf("%s Packet Driver (Ver 1.%02d) found at 0x%02x\n"
           "Ethernet Address is: %s", dname, version, vector,
            print_ether((unsigned char *) myEtherAddr));
    printf("\nInterface clase: %d, type: %d, number: %d (%s function)\n\n",
           iclass, itype, inum, drvfcn[pktfcn-1]);

/*
 *  {
 *      char buffer[256]; int len;
 *
 *      strcpy(buffer, "abcdefg           ");
 *      if (get_multicast_list(buffer, &len) != 0) {
 *          int  i;
 *          printf("Multicast (%d):", len);
 *          for (i = 0; i < len; i += 6)
 *              printf(" %s", print_ether((unsigned char *) &buffer[i]));
 *          printf("\n");
 *      } else {
 *          printf("Can't get Multi-case address list.\n");
 *      }
 *  }
 */

    if ((drvno = hdinfo(&pkts)) == 0) {
        printf("local hard disk not found !!\n");
        goto cleanup;
    }
    printf("%d local drive found.\n", drvno);

    local_cylinder = pkts.b.toc[0].cylinder;
    local_head     = pkts.b.toc[0].head;
    local_sector   = pkts.b.toc[0].sector;

    printf("Data memory = %u, ", coreleft());
    if ((disk_buffer = calloc(512 * local_sector, sizeof(char))) == NULL) {
        printf("Out of memory !!\n");
        return 1;
    }
    printf("allocate %d disk cache, %u free.\n", 512 * local_sector,coreleft());

    if (local_sector > MAX_SECTOR_SIZE) {
        printf("Sector size %d > %d .... program need recompile\n", local_sector, MAX_SECTOR_SIZE);
        goto cleanup;
    }

    memcpy(pkts.h.dest, broadcast,       6);
    memcpy(pkts.h.src,  myEtherAddr,     6);
    memcpy(pkts.h.type, pkt_type_server, PKT_TYPE_LEN);
    pkts.h.cmd = passive ? CLIENT_REGIST : REQUEST_SERVER;
    fixchecksum(&pkts, sizeof(struct pkthdr));

    fprintf(stderr, "Searching for HD Server ...");
    flush_buffer();
    send_pkt(&pkts, sizeof(struct pkthdr));

    clear_keyboard_buffer();
    retry = 0;

    while (expect_pkt(&pkt, SERVER_REPLY, TIMEOUT) == 0) {
        if (kbhit() && (getch() == 27)) {
            fprintf(stderr, " skip.\n\n");
            goto cleanup;
        }
        if (passive) {
            retry = (retry + 1) %4;
            fprintf(stderr, "%c\b", banner[retry]);
        } else {
            fprintf(stderr, ".");
            if (retry++ > 15) {
                fprintf(stderr, " Timeout\n\n");
                goto cleanup;
            }
        }
        send_pkt(&pkts, sizeof(struct pkthdr));
    }

    fprintf(stderr, "found HD Server %s ( %s )\n\n", pkt.b.toc[0].sname,
                    print_ether((unsigned char *) pkt.h.src));

    if (passive) {
        pktfmt          fixpkt;
        int             len, x, y, lc;
        int             exitflag = 0;
        unsigned long   sn = 0L;

#ifdef MULTICAST
        memcpy(multicast, &pkt.b.toc[pkt.h.len], 6);

        if (memcmp(multicast, broadcast, 6) == 0) {
            printf("Broadcast mode enable.\n");
        } else if (memcmp(multicast, myEtherAddr, 6) == 0) {
            printf("Multicast address: %s (local)\n", print_ether((unsigned char *) multicast));
        } else {
            printf("Multicast address: %s    ", print_ether((unsigned char *) multicast));
            if ((((int) multicast[0]) & 2) == 1) { // real Multi-cast
                if (set_multicast_list(multicast, 6) == 0)
                    printf("can't set Multicast Address (%d)\n", get_pktdrv_err());

                if (set_rcv_mode(handle, RCV_IF_BR_LMUL) == 0) {
                    if (set_rcv_mode(handle, RCV_IF_BR_MUL) == 0) {
                        printf("\nPacket Driver error: can't set receive mode\n");
                        goto cleanup;
                    }
                    printf("enable all mullticast packets.\n");
                } else {
                    printf("enable limited mullticast packets.\n");
                }
            } else {
                if (set_rcv_mode(handle, RCV_PROMISCUOUS) == 0) {
                    printf("\nPacket Driver error: can't set promiscuous mode\n");
                    goto cleanup;
                } else {
                    printf("enable all packets. (promiscuous)\n");
                }
            }
        }
#endif

        memcpy(ServerEtherAddr, pkt.h.src,       6);
        memcpy(fixpkt.h.dest,   pkt.h.src,       6);
        memcpy(fixpkt.h.src,    myEtherAddr,     6);
        memcpy(fixpkt.h.type,   pkt_type_server, PKT_TYPE_LEN);
        fixpkt.h.cmd = SERVER_RESEND;

        fprintf(stderr, "Waiting for sync........... ");
        while ((len = read_pkt(&pkt)) != 0) {
            if ((memcmp(ServerEtherAddr, pkt.h.src, 6) == 0) &&
                (pkt.h.cmd == SERVER_DINFO)) {
                break;
            }
        }
        fprintf(stderr, "remote disk CHS=%u/%u/%u\n\n\n\n", pkt.h.cylinder, pkt.h.head, pkt.h.sector);

        if (pkt.h.sector != local_sector) {
            fprintf(stderr, "Remote disk and local disk must have same SECTOR size ... %u != %u\n", pkt.h.sector, local_sector);
            goto cleanup;
        }

        if (pkt.h.head != local_head) {
            fprintf(stderr, "Remote disk and local disk must have same HEAD size ... %u != %u\n", pkt.h.head, local_head);
            if (! testonly)
                goto cleanup;
            fprintf(stderr, "For testing only ... it still work .... ");
        }

        if (pkt.h.cylinder > local_cylinder) {
            fprintf(stderr, "Remote disk Cylinder %d > local disk %u\n", pkt.h.cylinder, local_cylinder);
            if (! testonly)
                goto cleanup;
            fprintf(stderr, "For testing only ... it still work .... ");
        }

        gotoxy(x = wherex(), y = wherey() - 3);
        lc = -1;

        while ((len = read_pkt(&pkt)) != 0) {
            if (memcmp(ServerEtherAddr, pkt.h.src, 6) != 0)
                continue;

            switch(pkt.h.cmd) {
            case SERVER_POST  :
                if (pkt.h.serial_no == sn) {
                    if (lc != pkt.h.cylinder) {
                        lc = pkt.h.cylinder;
                        gotoxy(x, y);
                        fprintf(stderr, "Cylinder %4u", lc);
                    }

                    sn++;
                    if (! testonly)
                        writeTrack(0x80, pkt.h.head, pkt.h.cylinder,
                                          pkt.h.sector, pkt.h.len, pkt.b.buffer);
                } else if (pkt.h.serial_no > sn) {
                    // packet lost handling .....
#ifdef DEBUG
                    fprintf(stderr, "\rPacket lost .... ");
#endif
                    fixpkt.h.serial_no = sn;
                    fixchecksum(&fixpkt, sizeof(struct pkthdr));
                    send_pkt(&fixpkt, sizeof(struct pkthdr));
                }
                break;
            case TRANS_ABORT:
                fprintf(stderr, "\n\nERROR: too many packet lost, dropped by server !!\n\n");
            case CLOSE_CONNECT:
                rebootFlag = 1;
                goto cleanup;
            }

            if (exitflag) break;
        }
    } else {
        while (1) {
            for (i = 0; i < pkt.h.len; i++) {
                fprintf(stderr, "%d - CHS=%u/%u/%u - %s\n",
                    i + 1,
                    pkt.b.toc[i].cylinder ,
                    pkt.b.toc[i].head     ,
                    pkt.b.toc[i].sector   ,
                    pkt.b.toc[i].signature);
            }
            fprintf(stderr, "\nPress ENTER to select, ESC to quit ...");
            select = do_select(pkt.h.len, 1, wherey() - pkt.h.len - 1, 79, DEFAULT_ATTR, 0x4A);

            if (! select) break;

            select--;
            drv      = pkt.b.toc[select].drv;
            cylinder = pkt.b.toc[select].cylinder;
            head     = pkt.b.toc[select].head;
            sector   = pkt.b.toc[select].sector;

            if ((head == local_head) && (sector == local_sector)) {

//              time_t     from, to;
                int        st_cyl = 0, end_cyl = 0;

                if ((partition = partition_table(pkt.h.src, drv, cylinder-1, head, sector, &st_cyl, &end_cyl)) == 0)
                    break;

                partition--;
                fprintf(stderr, "\rRemote Disk %s from %d to %d\n", pkt.b.toc[select].signature, st_cyl, end_cyl);

/*
                fprintf(stderr, ", %s CHS=%u/%u/%u\n",
                                 pkt.b.toc[select].signature,
                                 cylinder, head, sector);
*/

/*
                time(&from);
                printf("Begin   Time is: %s\n", ctime(&from));
*/
                fprintf(stderr, "Coping remote Cylinder 0 .... ");
                copy_remote_harddisk(pkt.h.src, drv, 0, 0, head, sector, 0);
                fprintf(stderr, "ok.\n");
                copy_remote_harddisk(pkt.h.src, drv, MAX(1, st_cyl), MIN(end_cyl, local_cylinder), head, sector, 1);
                fprintf(stderr, "\n");
/*
                time(&to);
                printf("\nCurrent Time is: %s, Elapsed time: %1.0f sec.\n", ctime(&to), difftime(to, from));
*/
                reboot();
                break;
            } else {
                fprintf(stderr, "\rError! Remote disk must have same head/sector size !!\n\n");
            }
        }
    }

cleanup:

    if (release_type(handle)) {
        printf("\nPacket Driver: release handle (%d)\n", handle);
    } else {
        printf("\nPacket Driver: can not release handle\n");
    }

    farfree(pktbuffer);
    free(disk_buffer);

    setvect(0x23, ctrl_brk);
    setvect(0x1B, ctrl_c);

    printf("\n%lu packets received by filter\n", pkt_received);
    printf("%lu packets dropped by kernel\n", pkt_dropped);

    if (rebootFlag) reboot();

    setattr(0x07, wherex(), wherey(), 80, 1);

    return 0;
}


int partition_table(const unsigned char *eth, const int drv, int cylinder, int head, int sector, int *st_cyl, int *en_cyl)
{
    pktfmt         pkts, pktin;
    int            ii, i, j;
    unsigned char  *ptr = &pktin.b.buffer[512 - 2 - 16 * 4];
    static char    partid  [4];
    static char    active  [4];
    static int     str_cyl [4];
    static int     str_head[4];
    static int     str_sec [4];
    static int     end_cyl [4];
    static int     end_head[4];
    static int     end_sec [4];
    static double  relative[4];
    static double  num_sec [4];


    memcpy(pkts.h.src,  myEtherAddr,     6);
    memcpy(pkts.h.dest, eth,             6);
    memcpy(pkts.h.type, pkt_type_server, PKT_TYPE_LEN);
    pkts.h.cmd      = REQUEST_IMAGE;
    pkts.h.drv      = drv;
    pkts.h.cylinder = 0;
    pkts.h.head     = 0;
    pkts.h.sector   = 1;
    pkts.h.len      = 1;

    fixchecksum(&pkts, sizeof(struct pkthdr));

    if (! send_and_recv(&pkts, &pktin))
        return 0;

    printf("\n\n"
"                      Starting          Ending          Relative     Number of\n"
"    System   Boot   Cyl. Head Sec.   Cyl. Head Sec.     Sectors       Sectors\n"
"-------------------------------------------------------------------------------\n"
"<Entire Disk>  -      0    0    0     %3d  %3d  %3d            0     %9lu\n",
    cylinder-1, head-1, sector, (unsigned long) cylinder * head * sector);

    for (ii = i = 0; ii < 4; ii++) {
        partid  [i] = ptr[i * 16 + 4];
        active  [i] = ptr[i * 16];
        str_cyl [i] = ptr[i * 16 + 3] + ( ptr[i * 16 + 2] >> 6 ) * 256;
        str_head[i] = ptr[i * 16 + 1];
        str_sec [i] = ptr[i * 16 + 2] & 0x3F;
        end_cyl [i] = ptr[i * 16 + 7] + ( ptr[i * 16 + 6] >> 6 ) * 256;
        end_head[i] = ptr[i * 16 + 5];
        end_sec [i] = ptr[i * 16 + 6] & 0x3F;
        relative[i] = *((double *) &ptr[i * 16 + 8]);
        num_sec [i] = *((double *) &ptr[i * 16 + 12]);

        for (j = 0; systemid[j].id != '\0'; j++) {
            if (partid[i] == systemid[j].id)
                break;
        }
        if (partid[i] == '\0')
            continue;

        printf("%12s   %s", systemid[j].name, (active[i] == '\x80') ? "A" : " ");
        printf("    %3d  %3d  %3d", str_cyl[i], str_head[i], str_sec[i]);
        printf("     %3d  %3d  %3d", end_cyl[i], end_head[i], end_sec[i]);
        printf("      %7lu     %9lu\n", relative[i], num_sec[i]);
        i++;
    }
    if (i) {
        int   sel;
        fprintf(stderr, "\nPress ENTER to select, ESC to quit ...");
        sel = do_select(i + 1 , 1, wherey() - i - 2, 79, DEFAULT_ATTR, 0x4A);
        switch (sel) {
        case  0:
            break;
        case  1:
            *st_cyl = 0;
            *en_cyl = cylinder-1;
            break;
        default:
            *st_cyl = str_cyl[sel-2];
            *en_cyl = end_cyl[sel-2];
            break;
        }
        return sel;
    }

    return 0;
}

int copy_remote_harddisk(unsigned char *eth, int drv, int beg_cyl, int cyl, int head, int sec, int flag)
{
    pktfmt     pkts;
    int        c, h, s, i, x, y, rate;
    time_t     begin, current;
    float      percent;

    if (flag) {
        printf("\n\n\n");
        gotoxy(x = wherex(), y = wherey() - 3);
        time(&begin);
    }

    memcpy(pkts.h.src,  myEtherAddr,     6);
    memcpy(pkts.h.dest, eth,             6);
    memcpy(pkts.h.type, pkt_type_server, PKT_TYPE_LEN);
    pkts.h.cmd = REQUEST_IMAGE;
    pkts.h.drv = drv;
    for (c = beg_cyl; c <= cyl + 1; c++) {
        pkts.h.cylinder = c;
        rate = (int) (percent = (c - beg_cyl) * 100. / (cyl - beg_cyl + 1)) * 79 / 100;

        if (flag) {
            time(&current);
            gotoxy(x, y);
            printf("Coping remote disk: Cylinder %u, %3.1f%% completed, Elapsed time: %3.1f sec(s)\n", c, percent, difftime(current, begin));
            for (i = 0; i < 79; i++)
                printf("%c", i < rate ? 'Û' : '°');
        }

        if (! (c <= cyl))
            break;

        for (h = 0; h < head; h++) {
            pkts.h.head = h;

            for (s = 1; s <= sec; s+= 2) {
                pkts.h.sector = s;
                pkts.h.len = (s == sec ? 1 : 2);

                fixchecksum(&pkts, sizeof(struct pkthdr));
                if (! send_recv_and_write(&pkts))
                    return 0;
            }
        }
    }
    return 1;
}

int send_and_recv(const pktfmt *pkt, pktfmt *pktin)
{
    int      readok;

    do {
        readok = 0;
        flush_buffer();
        send_pkt(pkt, sizeof(struct pkthdr));
        while (expect_pkt(pktin, SEND_IMAGE, TIMEOUT) == 0) {
            if (kbhit() && (getch() == 27))
                return 0;
            send_pkt(pkt, sizeof(struct pkthdr));
        }
        if ((pktin->h.drv      == pkt->h.drv     ) &&
            (pktin->h.cylinder == pkt->h.cylinder) &&
            (pktin->h.head     == pkt->h.head    ) &&
            (pktin->h.sector   == pkt->h.sector  ) &&
            (pktin->h.len      == pkt->h.len     ))
            readok = 1;
    } while (! readok);

    return 1;
}

int send_recv_and_write(const pktfmt *pkt)
{
    pktfmt   pktin;
    int      readok;
    int      retry = 0;

    do {
        readok = 0;
        flush_buffer();
        send_pkt(pkt, sizeof(struct pkthdr));
        while (expect_pkt(&pktin, SEND_IMAGE, TIMEOUT) == 0) {
            if (verbose > 2)
                fprintf(stderr, "retry %5d\b\b\b\b\b\b\b\b\b\b\b", retry+1);
            if (retry++ > 30) {
                if (verbose > 1)
                    fprintf(stderr, ".... Timeout\n\n");
                return 0;
            }
            if (kbhit() && (getch() == 27))
                return 0;
            send_pkt(pkt, sizeof(struct pkthdr));
        }
        if ((pktin.h.drv      == pkt->h.drv     ) &&
            (pktin.h.cylinder == pkt->h.cylinder) &&
            (pktin.h.head     == pkt->h.head    ) &&
            (pktin.h.sector   == pkt->h.sector  ) &&
            (pktin.h.len      == pkt->h.len     ))
            readok = 1;
    } while (! readok);

    if (! testonly)
        writeTrack(0x80, pktin.h.head, pktin.h.cylinder,
                    pktin.h.sector, pktin.h.len, pktin.b.buffer);

    return 1;
}

void writeTrack(int drv, int head, int cylinder, int sector, int len, void *buffer)
{
    memcpy(&disk_buffer[512 * (sector - 1)], buffer, len * 512);

    if (sector + len - 1 == local_sector)
        biosdisk(3, drv, head, cylinder, 1, local_sector, disk_buffer);
}

void reboot()
{
    union REGS  registers;

    clear_keyboard_buffer();
    fprintf(stderr, "\n\nPress Ctrl-Alt-DEL to reboot ... ");
    getch();
    __emit__(0xea, 0, 0, 0xff, 0xff);
    int86(0x19, &registers, &registers);
    return;
}
