#ifndef FT_MALCOLM_H
# define FT_MALCOLM_H

#include <stdio.h>
#include <sys/socket.h>   // recvfrom, socklen_t, struct sockaddr
#include <netinet/in.h>   // struct sockaddr_in, in_addr
#include <arpa/inet.h>    // inet_addr
#include <signal.h>
#include <net/if_arp.h>
#include <unistd.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <ifaddrs.h>
#include <net/if.h>

int	error_msg(char *str);
void print_usage(void);
int error_parse_args(int ac, char **av);
int is_ethernet_ipv4_arp(const unsigned char *buf, size_t len);
int is_correct_ip(const char *ip_from, const char *stolen_ip,const unsigned char *buf);
int is_correct_mac(const char *mac_from, const unsigned char *buf);
int arp_reply(int fd, char **av, int ifindex);

#define TRUE 1
#define FALSE 0

#endif
