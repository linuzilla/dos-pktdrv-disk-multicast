#ifndef __NET_COPY_HD__H_
#define __NET_COPY_HD__H_

#include "pktdrv.h"

#define REQUEST_SERVER  (9)
#define SERVER_REPLY    (1)

#define REQUEST_IMAGE   (2)
#define SEND_IMAGE      (3)

#define REPLY_ERROR     (4)

#define CLIENT_REGIST   (6)
#define CLIENT_UNREGIST (7)

#define SERVER_POST     (10)
#define CLOSE_CONNECT   (11)
#define SERVER_DINFO    (12)
#define SERVER_RESEND   (13)
#define TRANS_ABORT     (14)


#define MULTICAST
// #define PKT_TYPE        "\x19\x97"
#define PKT_TYPE_LEN    3
#define VERSION         "0.12"


#define MIN(x,y)  (((x) > (y)) ? (y) : (x))
#define MAX(x,y)  (((x) > (y)) ? (x) : (y))

struct pkthdr {
    unsigned char    dest[6];
    unsigned char    src[6];
    unsigned char    type[PKT_TYPE_LEN];

    unsigned int     cmd;
    unsigned int     length;
    unsigned int     checksum;
    unsigned long    serial_no;
    unsigned int     drv;
    unsigned int     cylinder;
    unsigned int     head;
    unsigned int     sector;
    unsigned int     len;
};

#define SERVER_NAME_LEN  (15)

struct table_of_content {
    unsigned int     drv;
    unsigned int     cylinder;
    unsigned int     head;
    unsigned int     sector;
    unsigned int     len;
    char             sname[SERVER_NAME_LEN];
    char             signature[80];
};

typedef struct {
    struct pkthdr    h;
    union {
        unsigned char             buffer[1500];
        struct table_of_content   toc[10];
    } b;
} pktfmt;


extern unsigned int         buffer_size, buffer_front, buffer_rear;
extern unsigned int         buffer_data;
extern pktfmt huge          *pktbuffer;
extern unsigned int         *pktlen;
extern unsigned long        pkt_received, pkt_dropped;
extern volatile unsigned int   control_c_counter;
extern const char           broadcast[];
extern unsigned char        multicast[];

extern const char           pkt_type_server[];
extern const char           pkt_type_client[];

struct __system_id {
    char   id;
    char   *name;
};

extern struct __system_id   systemid[];


void interrupt far   receiver  (unsigned bp, unsigned di, unsigned si,
                                unsigned ds, unsigned es, unsigned dx,
                                unsigned cx, unsigned bx, unsigned ax);
void interrupt far   control_c (unsigned bp, unsigned di, unsigned si,
                                unsigned ds, unsigned es, unsigned dx,
                                unsigned cx, unsigned bx, unsigned ax);

volatile int  recv_pkt(pktfmt *pkt);
int  read_pkt(pktfmt *pkt);
int  expect_pkt(pktfmt *pkt, int pktcmd, int timedelay);
void flush_buffer(void);


void fixchecksum(pktfmt *pkt, int len);
int  check_checksum(pktfmt *pkt);
void clear_keyboard_buffer(void);
char *print_ether(const unsigned char *buf);

void setattr(int attr, int x, int y, int width, int height);
int do_select(const int num, const int x, const int y, const int len, const int attr, const int bar);

#endif
