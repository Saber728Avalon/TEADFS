#ifndef __NETLINK_H___
#define __NETLINK_H___




// create netlink. R3 communite
int teadfs_start_netlink(void);

// release netlink
void teadfs_release_netlink(void);

int teadfs_send_to_user(char* data, int size);
#endif


