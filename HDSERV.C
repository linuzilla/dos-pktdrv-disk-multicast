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


// #define PASSIVE_SERVER

#define DEFAULT_ATTR  0x1E
#define SHOW_FREQ     7


#ifndef         PASSIVE_SERVER
#    define     ACTIVE_SERVER
#    ifndef     DEFAULT_SERVER_NAME
#        define DEFAULT_SERVER_NAME "CC-HDserv Act"
#    endif
#else
#    ifndef     DEFAULT_SERVER_NAME
#        define DEFAULT_SERVER_NAME "CC-HDserv Psv"
#    endif
#endif

// #define DEBUG

/* Global Variables */

unsigned char        myEtherAddr[6];
int                  verbose = 0;

#ifdef ACTIVE_SERVER
unsigned int         num_of_clients = 0;
#ifdef MULTICAST
int                  broadcast_flag = 0;
int                  multicast_flag = 0;
int                  use_first_connection = 1;
#endif
#endif

pktfmt               pktr, pkts;
unsigned int         pktrlen;
int                  start_cylinder, end_cylinder;
unsigned long        serno;

int partition_table(const int drv, int cylinder, int head, int sector, int *st_cyl, int *en_cyl);
int sending_cylinder(int drv, int beg_cyl, int cyl, int head, int sec, int track, int flag);

static const char *drvfcn[] = {
    "basic", "extended", "", "", "high-performance", "full"
};

int main(int argc, char *argv[])
{
    int                  vector, handle;
    int                  version, iclass, itype, inum;
    char                 dname[20], sname[80];
    void interrupt       (*ctrl_c)(), (*ctrl_brk)();
    int                  drvno, infopktlen, i, erropt;
    pktfmt               infopkt;
    unsigned long        counter = 0L;
    int                  x, y, pktfcn;
#ifdef ACTIVE_SERVER
    int                  key;
    int                  local_cylinder;
    int                  local_head;
    int                  local_sector;
#endif

    for (erropt = 0, i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            int  j;

#ifdef ACTIVE_SERVER
            if (strcmp(argv[i], "--broadcast") == 0) {
                use_first_connection = 0;
                broadcast_flag = 1;
                continue;
            }

#ifdef MULTICAST
            if (strcmp(argv[i], "--uni-cast") == 0) {
                use_first_connection = 1;
                continue;
            }
#endif

            if ((strcmp(argv[i], "--multicast") == 0) && (i + 1 < argc)) {
                char   e[6];
                i++;
                sscanf(argv[i], "%x:%x:%x:%x:%x:%x", &e[0], &e[1], &e[2], &e[3], &e[4], &e[5]);
//              printf("Multicast Address: %s\n", print_ether((unsigned char*) e));
                memcpy(multicast, e, 6);
                multicast_flag = broadcast_flag = 1;
                use_first_connection = 0;
                continue;
            }
#endif
            for (j = 1; argv[i][j] != '\0'; j++) {
                switch (argv[i][j]) {
                case 'v':
                    verbose++;
                    break;
                case 'h':
                default :
                    erropt = 1;
                    break;
                }
            }
        } else {
            break;
        }
    }

    if (erropt) {
        printf("\nHDserv v%s (c) 1997 by Jiann-Ching Liu, Computer Center of N.C.U.\n\n", VERSION);
#ifdef ACTIVE_SERVER
        printf("usage: hdserv [options] [server_name]\n"
               "options:\n"
               "   -h                display this help and exit\n"
               "   -v                verbose\n"
               "   --broadcast       broadcast transmition\n"
               "   --uni-cast        use first connected client as destnation address (default)\n"
               "   --multicast addr  user specify multicast address\n");
#else
        printf("usage: hdserver [-h] [-v] [server_name]\n");
#endif
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

    if (i < argc) {
        strncpy(sname, argv[i], SERVER_NAME_LEN-1);
    } else {
        strcpy(sname, DEFAULT_SERVER_NAME);
    }
    sname[SERVER_NAME_LEN-1] = '\0';

    printf("\nServer Name: \"%s\",   %d packet buffer allocated.\n", sname, buffer_size);

    if ((vector = initial_pktdrv()) == 0) {
        printf("Packet Driver not found\n");
        return 2;
    }

    if ((handle = access_type(1, 0xFFFF, 0, pkt_type_server, PKT_TYPE_LEN, (RECEIVER) receiver)) == 0) {
        printf("Packet Driver error: access_type\n");
        return 5;
    }

    if (get_address(handle, myEtherAddr, 6) == 0) {
        printf("Packet Driver error: can't get Ethernet Address\n");
        goto cleanup;
    }

#if defined(ACTIVE_SERVER) && defined(MULTICAST)
    if (! multicast_flag) {
        memcpy(multicast, (broadcast_flag ? broadcast : (char *) &myEtherAddr), 6);
//      multicast[0] = '\xff';    // Multicast ethernet address must start with ff
    }

//  if (! multicast_flag && broadcast_flag)
//      memcpy(multicast, broadcast, 6);
//  multicast[0] = '\xff';    // Multicast ethernet address must start with ff
#endif

    if ((pktfcn = driver_info(handle, &version, &iclass, &itype, &inum, dname)) == 0) {
        printf("Packet Driver error: can't get driver info\n");
        goto cleanup;
    }

    if (set_rcv_mode(handle, RCV_IF_BROAD) == 0) {
        printf("Packet Driver error: can't set receive mode\n");
    }

    ctrl_c   = getvect(0x1B);
    ctrl_brk = getvect(0x23);
    setvect(0x1B, control_c);
    setvect(0x23, control_c);

    setattr(DEFAULT_ATTR, wherex(), wherey(), 80, 25);

    printf("\n%s Packet Driver (Ver 1.%02d) found at 0x%02x\n"
           "Ethernet Address is: %s", dname, version, vector,
            print_ether((unsigned char *) myEtherAddr));
    printf("\nInterface clase: %d, type: %d, number: %d (%s function)\n\n",
           iclass, itype, inum, drvfcn[pktfcn-1]);

    if ((drvno = hdinfo(&infopkt)) == 0)
        goto cleanup;

#ifdef ACTIVE_SERVER
    {
        local_cylinder = infopkt.b.toc[0].cylinder;
        local_head     = infopkt.b.toc[0].head;
        local_sector   = infopkt.b.toc[0].sector;

        if (partition_table(0x80, local_cylinder - 1, local_head, local_sector, &start_cylinder, &end_cylinder) == 0)
            goto cleanup;
        printf("\rReady to copy from %d to %d\n", start_cylinder, end_cylinder);
    }
#endif

    memcpy(infopkt.b.toc[0].sname, sname, SERVER_NAME_LEN);
    infopkt.h.cmd = SERVER_REPLY;
    infopkt.h.len = drvno;
    infopktlen    = sizeof(struct pkthdr) +
                    sizeof(struct table_of_content) * drvno;

    memcpy(infopkt.h.src,    myEtherAddr,     6);
    memcpy(infopkt.h.type,   pkt_type_client, PKT_TYPE_LEN);


#if defined(ACTIVE_SERVER) && defined(MULTICAST)
    infopktlen += 6;
    memcpy(&infopkt.b.toc[infopkt.h.len], multicast,   6);
#endif

    fprintf(stderr, "\nWaiting for connection ............. ");
    x = wherex();
#ifdef ACTIVE_SERVER
    fprintf(stderr, "\n\nPress ENTER to Network copy, or press ESC to quit.\n");
#else
    fprintf(stderr, "\n\n\n");
#endif
    gotoxy(x, y = wherey() - 3);

#ifdef ACTIVE_SERVER
    while (! kbhit() || (((key = getch()) != 27) && (key != 13))) {
#else
    while (! kbhit() || getch() != 27) {
#endif
        if ((pktrlen = recv_pkt(&pktr)) != 0) {
            if (check_checksum(&pktr)) {
                gotoxy(x, y);
                fprintf(stderr, "%lu", ++counter);
                switch(pktr.h.cmd) {
#ifdef ACTIVE_SERVER
                case CLIENT_REGIST  :
#ifdef MULTICAST
                    if ((++num_of_clients == 1) && (! broadcast_flag) && (use_first_connection)) {
                        memcpy(multicast, pktr.h.src, 6);
                        memcpy(&infopkt.b.toc[infopkt.h.len], multicast,   6);
                    }
#endif
                    memcpy(infopkt.h.dest, pktr.h.src,  6);
                    fixchecksum(&infopkt, infopktlen);
                    send_pkt(&infopkt, infopktlen);
                    break;
                case CLIENT_UNREGIST :
                    num_of_clients--;
                    memcpy(infopkt.h.dest, pktr.h.src,  6);
                    fixchecksum(&infopkt, infopktlen);
                    send_pkt(&infopkt, infopktlen);
                    break;
#endif
#ifdef PASSIVE_SERVER
                case REQUEST_SERVER :
#ifdef DEBUG
                    printf("request server from %s\n", print_ether((unsigned char *) pktr.h.src));
#endif
                    memcpy(infopkt.h.dest, pktr.h.src,  6);

                    fixchecksum(&infopkt, infopktlen);
                    send_pkt(&infopkt, infopktlen);
                    break;
                case REQUEST_IMAGE  :
#ifdef DEBUG
                    printf("request image\n");
#endif
                    if (pktr.h.len < 1 || pktr.h.len > 2) {
                        pktr.h.cmd = REPLY_ERROR;
                    } else {
                        biosdisk(2, pktr.h.drv, pktr.h.head, pktr.h.cylinder,
                                    pktr.h.sector, pktr.h.len, pktr.b.buffer);
                        pktr.h.cmd = SEND_IMAGE;
                        pktrlen = pktr.h.len * 512 + sizeof(struct pkthdr);
                    }
                    memcpy(pktr.h.dest, pktr.h.src,      6);
                    memcpy(pktr.h.src,  myEtherAddr,     6);
                    memcpy(pktr.h.type, pkt_type_client, PKT_TYPE_LEN);
                    fixchecksum(&pktr, pktrlen);
                    send_pkt(&pktr, pktrlen);
                    break;
#endif
#ifdef DEBUG
                default:
                    fprintf(stderr, "unknow cmd %d\n", pktr.h.cmd);
                    break;
#endif
                }
#ifdef ACTIVE_SERVER
                fprintf(stderr, ", %d client(s) registed", num_of_clients);
#endif

#ifdef DEBUG
            } else {
                int                  i;
                unsigned char *ptr = (unsigned char *) &pktr;

                printf("Check sum error .... \n");
                for (i = 0; i < pktrlen; i++) {
                    printf("%02x ", ptr[i]);
                }
                printf("\n");
#endif
            }
        }
    }

#ifdef ACTIVE_SERVER
    gotoxy(1, y+2);
    fprintf(stderr, "                                                      ");
    gotoxy(x, y);
#endif

    printf("\n");

#ifdef ACTIVE_SERVER
    if (key == 13) {
        pktfmt   pkt;
#ifdef MULTICAST
        broadcast_flag = multicast_flag ? 0 : broadcast_flag;
        fprintf(stderr, "%scast Address: %s\n", broadcast_flag ? "Broad" : "Multi", print_ether((unsigned char *) multicast));
#endif
        fprintf(stderr, "Synchornization ..... ");

#ifdef MULTICAST
        memcpy(pkt.h.dest, multicast,   6);
#else
        memcpy(pkt.h.dest, broadcast,   6);
#endif
        memcpy(pkt.h.src,  myEtherAddr,     6);
        memcpy(pkt.h.type, pkt_type_client, PKT_TYPE_LEN);
        pkt.h.cylinder = end_cylinder;
        pkt.h.head     = local_head;
        pkt.h.sector   = local_sector;

        pkt.h.cmd = SERVER_DINFO;
        fixchecksum(&pkt, sizeof(struct pkthdr));
        for (i = 5; i >= 0; i--) {
            fprintf(stderr, "%3d\b\b\b", i);
            sleep(1);
            send_pkt(&pkt, sizeof(struct pkthdr));
        }
        fprintf(stderr, "ok.\n");

        serno = 0L;

        fprintf(stderr, "\rSending Track 0 (partition table) .... ");
        if (! sending_cylinder(0x80, 0, 0, local_head, local_sector, 1, 0))
            goto cleanup;
        fprintf(stderr, "\ok.");

        if (! sending_cylinder(0x80, start_cylinder, end_cylinder, local_head, local_sector, 0, 1))
            goto cleanup;

        pkt.h.cmd = CLOSE_CONNECT;

        fixchecksum(&pkt, sizeof(struct pkthdr));
        for (i = 0; i < 5; i++) {
            sleep(1);
            send_pkt(&pkt, sizeof(struct pkthdr));
        }
    }
#endif

cleanup:

    if (release_type(handle)) {
        printf("\n\nPacket Driver: release handle (%d)\n", handle);
    } else {
        printf("\n\nPacket Driver: can not release handle\n");
    }

    farfree(pktbuffer);

    setvect(0x23, ctrl_brk);
    setvect(0x1B, ctrl_c);

    printf("\n%lu packets received by filter\n", pkt_received);
    printf("%lu packets dropped by kernel\n", pkt_dropped);

    setattr(0x07, wherex(), wherey(), 80, 1);

    return 0;
}


int sending_cylinder(int drv, int beg_cyl, int cyl, int head, int sec, int track, int flag)
{
    int                       c, h, s, i, j, x, y, rate, len, sector, lh;
    time_t                    begin, current;
    float                     percent;
    unsigned long             sn, min_sn, serial;
    unsigned long             del;
    static int                scale = 3000;
    static unsigned int       peaceCounter = 0, peaceDelay = 3000;
    static unsigned long      timedelay = 100L, min_delay = 1L, max_delay = 0L;

    memcpy(pkts.h.src,  myEtherAddr, 6);
#ifdef MULTICAST
    memcpy(pkts.h.dest, multicast,   6);
#else
    memcpy(pkts.h.dest, broadcast,   6);
#endif
    memcpy(pkts.h.type, pkt_type_client, PKT_TYPE_LEN);
    pkts.h.cmd = SERVER_POST;

    if (flag) {
        printf("\n\n\n\n\n");
        gotoxy(x = wherex(), y = wherey() - 4);
        time(&begin);
    }

    sector = (sec + 1) / 2;

    serial = (track != 0) ? ((unsigned long) track * sector) : (((unsigned long) (cyl - beg_cyl + 1)) * head * sector);

    pkts.h.drv = drv;
    control_c_counter = 0;

    for (sn = 0L, lh = -1; sn < serial;) {
        s = (int) (sn % sector);
        h = (int) (((sn - s) / sector) % head);
        c = (int) ((sn - (h * sector) - s) / head / sector) + beg_cyl;
        s = s * 2 + 1;

        pkts.h.cylinder  = c;
        pkts.h.head      = h;
        pkts.h.sector    = s;
        pkts.h.serial_no = serno + sn;
        pkts.h.len       = (s == sec ? 1 : 2);
        percent = (float) sn / serial * 100.;

        rate = (int) (percent * 79 / 100);

        if (flag) {
            if  ((lh != h) && (h % SHOW_FREQ == (head-1) % SHOW_FREQ)) {
                double t, r;
                time(&current);

                if ((t = difftime(current, begin)) != 0) {
                    r = (((double) (c - beg_cyl) * head +  h ) * sec) / 2.0 / difftime(current, begin);
                } else {
                    r = 0.0;
                }
                gotoxy(x, y);
                printf("Cylinder=%u Head=%-4u %4.2f%%, %4.1fkb/s, Elapsed time: %2.0f sec(s), %-7lu\n",
                          c, h, percent, r, t, timedelay);
                for (i = 0; i < 79; i++)
                    fprintf(stderr, "%c", i < rate ? 'Û' : '°');
                printf("\n");
                lh = h;
            }
        } else if (verbose > 3) {
            fprintf(stderr, "\rS/N=%7lu, timedelay = %5lu, min_delay = %5lu CHS=%u/%u/%-10u", sn, timedelay, min_delay, c, h, s);
        }

        if (control_c_counter > 4)
            return 0;

        biosdisk(2, drv, h, c, s, pkts.h.len, pkts.b.buffer);
        len = pkts.h.len * 512 + sizeof(struct pkthdr);

        fixchecksum(&pkts, len);
        send_pkt(&pkts, len);
        min_sn = serno + sn;

        for (j = 0; j < 5; j++) {
            for (del = 0L; del < timedelay; del++) {
                while (recv_pkt(&pktr) != 0) {
                    if ((pktr.h.cmd == SERVER_RESEND) &&
                        (pktr.h.serial_no < min_sn)) {
                        if (pktr.h.serial_no >= serno) {
                            min_sn = pktr.h.serial_no;
                            fprintf(stderr, "\rRequest from: %s, Restart from s/n: %-10lu", print_ether((unsigned char *) pktr.h.src), min_sn);
                        } else {
                            pktfmt pkt;

                            memcpy(pkt.h.src,  myEtherAddr, 6);
                            memcpy(pkt.h.dest, pktr.h.dest, 6);
                            memcpy(pkt.h.type, pkt_type_client, PKT_TYPE_LEN);
                            pkt.h.cmd = TRANS_ABORT;

                            fixchecksum(&pkt, sizeof(struct pkthdr));
                            send_pkt(&pkt, sizeof(struct pkthdr));
                        }
                    }
                }
            }
            if (sn < serial-1) break;
            if (min_sn != serno + sn) break;
            send_pkt(&pkts, len);
            sleep(1);
        }

        if (min_sn == sn + serno) {
            if (timedelay >= min_delay + scale) {
                timedelay -= scale;
            } else {
                if (peaceCounter++ > peaceDelay) {
                    peaceCounter = 0;
                    max_delay = timedelay;

                    if (min_delay > scale) {
                        min_delay -= scale;
                    } else {
                        if (scale > 150) {
                            scale /= 2;
                        } else if (scale > 50) {
                            scale -= 10;
                        } else if (scale > 10) {
                            scale -= 5;
                        } else if (scale > 1) {
                            scale--;
                        }
                        // scale = (scale > 1) ? scale / 2 : scale;
                    }

                    if (verbose > 1)
                        fprintf(stderr, "\rdelay=%lu, min delay=%lu, scale=%d, peace=%d/%-10d", timedelay, min_delay, scale, peaceCounter, peaceDelay);
                }
            }
        } else {
            int   i;

            min_delay = timedelay;
            timedelay = timedelay + (10 * scale);

            if (timedelay > max_delay) {
                timedelay = (3 * max_delay + timedelay) / 4;

                if (scale > 150) {
                    scale /= 2;
                } else if (scale > 50) {
                    scale -= 10;
                } else if (scale > 10) {
                    scale -= 5;
                } else if (scale > 1) {
                    scale--;
                } else {
                    peaceDelay = (peaceDelay < 30000) ? peaceDelay + 1000 : peaceDelay;
                }
                // scale = (scale > 1) ? scale / 2 : scale;
            }

            if (verbose > 1)
                fprintf(stderr, "\rdelay=%lu, min delay=%lu, scale=%d, peace=%d/%-10d", timedelay, min_delay, scale, peaceCounter, peaceDelay);

            for (i = 0; i < 100; i++) {
                for (del = 0L; del < timedelay; del++) {
                    while (recv_pkt(&pktr) != 0) {
                        if ((pktr.h.cmd == SERVER_RESEND) &&
                            (pktr.h.serial_no < min_sn)) {
                            if (pktr.h.serial_no >= serno) {
                                min_sn = pktr.h.serial_no;
                            } else {
                                pktfmt pkt;

                                memcpy(pkt.h.src,  myEtherAddr, 6);
                                memcpy(pkt.h.dest, pktr.h.dest, 6);
                                memcpy(pkt.h.type, pkt_type_client, PKT_TYPE_LEN);
                                pkt.h.cmd = TRANS_ABORT;

                                fixchecksum(&pkt, sizeof(struct pkthdr));
                                send_pkt(&pkt, sizeof(struct pkthdr));
                            }
                        }
                    }
                }
            }

            if (sn < min_sn - serno) {
                fprintf(stderr, "Fatal error !!  Serial number error !!\n");
                return 0;
            }

            sn = min_sn - serno;
            peaceCounter = 0;

            continue;
        }
        sn++;
    }
    serno += sn;

    return 1;
}

int partition_table(const int drv, int cylinder, int head, int sector, int *st_cyl, int *en_cyl)
{
    unsigned char  buffer[512];

    int            ii, i, j;
    unsigned char  *ptr = &buffer[512 - 2 - 16 * 4];
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

    biosdisk(2, drv, 0, 0, 1, 1, buffer);

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

//  if (i)
    {
        int   sel, y;

        fprintf(stderr, "\nPress ENTER to select, ESC to quit ...\n\n\n");
        gotoxy(1, y = wherey() - 3);

        sel = do_select(i + 1 , 1, wherey() - i - 2, 79, DEFAULT_ATTR, 0x4A);

        gotoxy(1, y);
        fprintf(stderr, "                                            ");
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

//  return 0;
}
