#ifndef __PKTDRV_H_
#define __PKTDRV_H_

#define RCV_OFF         (1)  /* turn off receiver                            */
#define RCV_INTERFACE   (2)  /* receiver only packets sent to this interface */
#define RCV_IF_BROAD    (3)  /* mode 2 plus broadcast packets                */
#define RCV_IF_BR_LMUL  (4)  /* mode 3 plus limited multicast packets        */
#define RCV_IF_BR_MUL   (5)  /* mode 3 plus all multicast packets            */
#define RCV_PROMISCUOUS (6)  /* all packets (promiscuous mode)               */

#define PKT_BAD_HANDLE     ( 1)
#define PKT_NO_CLASS       ( 2)
#define PKT_NO_TYPE        ( 3)
#define PKT_NO_NUMBER      ( 4)
#define PKT_BAD_TYPE       ( 5)
#define PKT_NO_MULTICAST   ( 6)
#define PKT_CANT_TERMINATE ( 7)
#define PKT_BAD_MODE       ( 8)
#define PKT_NO_SPACE       ( 9)
#define PKT_TYPE_INUSE     (10)
#define PKT_BAD_COMMAND    (11)
#define PKT_CANT_SEND      (12)
#define PKT_CANT_SET       (13)
#define PKT_BAD_ADDRESS    (14)
#define PKT_CANT_RESET     (15)

struct _ArpPkt {
    unsigned char htype[2];
    unsigned char ptype[2];
    unsigned char hsize;
    unsigned char psize;
    unsigned char op[2];
    unsigned char sea[6];
    unsigned char sip[4];
    unsigned char tea[6];
    unsigned char tip[4];
    unsigned char pad[18];
};

typedef struct {
    unsigned char da[6];
    unsigned char sa[6];
    unsigned char type[2];
    union {
        unsigned char   data[1500];
        struct _ArpPkt  arp;
    } t;
} InPkt;


typedef void interrupt far (*RECEIVER)(unsigned bp, unsigned di, unsigned si,
                                       unsigned ds, unsigned es, unsigned dx,
                                       unsigned cx, unsigned bx, unsigned ax);

/* typedef int (far *RECEIVER)(void); */

int driver_info(int handle, int far *version, int far *iclass,
                int far *itype, int far *inumber, void far *name);

int initial_pktdrv(void);
int access_type(int if_class, int if_type, int if_number, const char far *tp,
                unsigned typelen, RECEIVER receiver);
int release_type(int handle);
int send_pkt(void far *buffer, unsigned length);
int terminate(int handle);
int get_address(int handle, void far *buf, int len);
int reset_interface(int handle);
int set_rcv_mode(int handle, int mode);
int get_rcv_mode(int handle, int far *mode);
int set_multicast_list(void far *addrlist, unsigned length);
int get_multicast_list(void far *addrlist, int far *len);

/* --------------------------------------- */
int get_pktdrv_err(void);
/* --------------------------------------- */
int farmemcpy(void far *tar, const void far *src, int len);
int farmemcmp(const void far *tar, const void far *src, int len);

#endif
