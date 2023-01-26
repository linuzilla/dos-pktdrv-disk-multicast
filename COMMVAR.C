#include "netcphd.h"

const char    broadcast[] = { -1, -1, -1, -1, -1, -1 };

#ifdef MULTICAST
unsigned char  multicast[] = { 255, 0, 0, 0, 0, 0 };
#endif
