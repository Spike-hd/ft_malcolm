#include "../includes/ft_malcolm.h"


int	error_msg(char *str) {
	fprintf(stderr, "%s", str);
	return 1;
}

void print_usage(void)
{
	char *usage = "Usage: sudo ./ft_malcolm <source_ip> <source_mac> <target_ip> <target_mac>\n";
	error_msg(usage);
}

static int is_valid_mac(const char *mac) {
    int i;

    i = 0;
    while (i < 17) {
        if (i % 3 == 2) {
            if (mac[i] != ':')
                return FALSE;
        }
        else if (!((mac[i] >= '0' && mac[i] <= '9')
                || (mac[i] >= 'a' && mac[i] <= 'f')
                || (mac[i] >= 'A' && mac[i] <= 'F')))
            return FALSE;
        i++;
    }
    return mac[i] == '\0';
}

int error_parse_args(int ac, char **av) {
    struct in_addr address;

    if (ac != 5) {
        error_msg("Error: Wrong number of arguments.\n");
        print_usage();
        return TRUE;
    }
    if (inet_pton(AF_INET, av[1], &address) != 1
        || inet_pton(AF_INET, av[3], &address) != 1) {
        error_msg("Error: source_ip and target_ip must be valid IPv4 addresses.\n");
        print_usage();
        return TRUE;
    }
    if (!is_valid_mac(av[2]) || !is_valid_mac(av[4])) {
        error_msg("Error: MAC addresses must use the format XX:XX:XX:XX:XX:XX.\n");
        print_usage();
        return TRUE;
    }
    return FALSE;
}


int is_ethernet_ipv4_arp(const unsigned char *buf, size_t len) {
    if (!buf || len < 42)
        return FALSE;

    return (buf[12] == 0x08 && buf[13] == 0x06  // 0x806 = ARP
        && buf[14] == 0x00 && buf[15] == 0x01   // 0x0001 = Ethernet
        && buf[16] == 0x08 && buf[17] == 0x00   // 0x800 = IPv4
        && buf[18] == 6 && buf[19] == 4         // MAC de 6 et IP de 4
        && buf[20] == 0x00 && buf[21] == 0x01); // 0x0001 on est sur une demande, pas une reponse
}

int is_correct_ip(const char *ip_from, const char *stolen_ip, const unsigned char *buf) {
    struct in_addr sender;
    struct in_addr target;
    const unsigned char *sender_bytes;
    const unsigned char *target_bytes;
    int i;


    if (inet_pton(AF_INET, ip_from, &sender) != 1 || inet_pton(AF_INET, stolen_ip, &target) != 1)
        return FALSE;
    sender_bytes = (const unsigned char *)&sender.s_addr;
    target_bytes = (const unsigned char *)&target.s_addr;
    i = 0;
    while (i < 4) {
        if (buf[28 + i] != sender_bytes[i] || buf[38 + i] != target_bytes[i])
            return FALSE;
        i++;
    }
    return TRUE;
}


static int hex_value(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return c - 'A' + 10;
}

int is_correct_mac(const char *mac_from, const unsigned char *buf) {
    unsigned char byte;
    int i;

    i = 0;
    while (i < 6) {
        byte = hex_value(mac_from[i * 3]) * 16 + hex_value(mac_from[i * 3 + 1]);
        if (buf[22 + i] != byte)
            return FALSE;
        i++;
    }

    i = 0;
    while (i < 6) {
        if (buf[i] != 0xff)
            return FALSE;
        i++;
    }
    return TRUE;
}
