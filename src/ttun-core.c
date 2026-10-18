// src/ttun-core.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 2048
#define DEFAULT_PORT 5000

// Ensure all bytes are sent over the TCP stream
int send_all(int socket, void *buffer, size_t length) {
    char *ptr = (char*) buffer;
    while (length > 0) {
        int i = send(socket, ptr, length, MSG_NOSIGNAL);
        if (i < 1) return -1;
        ptr += i;
        length -= i;
    }
    return 0;
}

// Ensure exact number of bytes are read from the TCP stream
int recv_all(int socket, void *buffer, size_t length) {
    char *ptr = (char*) buffer;
    while (length > 0) {
        int i = recv(socket, ptr, length, 0);
        if (i < 1) return -1;
        ptr += i;
        length -= i;
    }
    return 0;
}

// Allocate virtual TUN interface
int tun_alloc(char *dev) {
    struct ifreq ifr;
    int fd;
    if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
        perror("[-] Failed to open /dev/net/tun");
        return -1;
    }
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI; 
    if (*dev) {
        strncpy(ifr.ifr_name, dev, IFNAMSIZ - 1);
        ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    }
    if (ioctl(fd, TUNSETIFF, (void *) &ifr) < 0) {
        perror("[-] ioctl TUNSETIFF failed");
        close(fd);
        return -1;
    }
    strncpy(dev, ifr.ifr_name, IFNAMSIZ - 1);
    dev[IFNAMSIZ - 1] = '\0';

    // Bring interface UP immediately
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock >= 0) {
        struct ifreq ifr_up;
        memset(&ifr_up, 0, sizeof(ifr_up));
        strncpy(ifr_up.ifr_name, dev, IFNAMSIZ - 1);
        if (ioctl(sock, SIOCGIFFLAGS, &ifr_up) >= 0) {
            ifr_up.ifr_flags |= (IFF_UP | IFF_RUNNING);
            ioctl(sock, SIOCSIFFLAGS, &ifr_up);
        }
        close(sock);
    }

    return fd;
}

// Resolve remote host/IP and connect (handles hostnames and IPv4)
int resolve_and_connect(const char *host, int port) {
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    struct addrinfo hints, *res, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(host, port_str, &hints, &res);
    if (status != 0) {
        fprintf(stderr, "[-] DNS resolution failed for %s: %s\n", host, gai_strerror(status));
        return -1;
    }

    int sock = -1;
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0) continue;
        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) {
            break;
        }
        close(sock);
        sock = -1;
    }
    freeaddrinfo(res);
    return sock;
}

// Main forwarding loop between TUN interface and TCP socket
void run_tunnel(int tun_fd, int sock_fd) {
    fd_set rd_set;
    char buffer[BUFFER_SIZE];

    while (1) {
        FD_ZERO(&rd_set);
        FD_SET(tun_fd, &rd_set);
        FD_SET(sock_fd, &rd_set);
        int max_fd = (tun_fd > sock_fd) ? tun_fd : sock_fd;

        if (select(max_fd + 1, &rd_set, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            break;
        }

        // 1. Read from TUN, write to TCP
        if (FD_ISSET(tun_fd, &rd_set)) {
            int nread = read(tun_fd, buffer, sizeof(buffer));
            if (nread > 0) {
                uint16_t len = htons((uint16_t)nread);
                if (send_all(sock_fd, &len, 2) < 0) break;
                if (send_all(sock_fd, buffer, nread) < 0) break;
            } else if (nread < 0 && errno != EAGAIN && errno != EINTR) {
                break;
            }
        }

        // 2. Read from TCP, write to TUN
        if (FD_ISSET(sock_fd, &rd_set)) {
            uint16_t len;
            if (recv_all(sock_fd, &len, 2) < 0) break;
            len = ntohs(len);
            if (len > sizeof(buffer) || len == 0) break;
            if (recv_all(sock_fd, buffer, len) < 0) break;
            if (write(tun_fd, buffer, len) < 0) {
                if (errno != EAGAIN && errno != EINTR && errno != EIO) break;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    // Disable stdout/stderr buffering for real-time systemd journal logs
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    signal(SIGPIPE, SIG_IGN);

    // Usage: ttun-core <role> <local_port> <remote_ip> <remote_port> <tun_name>
    if (argc < 6) {
        fprintf(stderr, "Usage: %s <role> <local_port> <remote_ip> <remote_port> <tun_name>\n", argv[0]);
        return 1;
    }

    char *role = argv[1]; // "server" or "client"
    int local_port = atoi(argv[2]);
    char *remote_ip = argv[3];
    int remote_port = atoi(argv[4]);
    char *tun_name = argv[5];

    // Sanitize ports (prevent binding to port 0 if empty or invalid)
    if (strcmp(role, "server") == 0) {
        if (local_port <= 0 || local_port > 65535) {
            printf("[!] Warning: Local port '%s' invalid or empty. Defaulting to %d\n", argv[2], DEFAULT_PORT);
            local_port = DEFAULT_PORT;
        }
    } else {
        if (remote_port <= 0 || remote_port > 65535) {
            printf("[!] Warning: Remote port '%s' invalid or empty. Defaulting to %d\n", argv[4], DEFAULT_PORT);
            remote_port = DEFAULT_PORT;
        }
    }

    int tun_fd = tun_alloc(tun_name);
    if (tun_fd < 0) return 1;

    int opt = 1;

    if (strcmp(role, "server") == 0) {
        int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_fd < 0) {
            perror("[-] socket creation failed");
            close(tun_fd);
            return 1;
        }

        setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#ifdef SO_REUSEPORT
        setsockopt(sock_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
#endif

        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(local_port);

        if (bind(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("[-] bind failed");
            close(sock_fd);
            close(tun_fd);
            return 1;
        }

        if (listen(sock_fd, 1) < 0) {
            perror("[-] listen failed");
            close(sock_fd);
            close(tun_fd);
            return 1;
        }

        while (1) {
            printf("[*] TTun TCP Server listening on port %d (%s)...\n", local_port, tun_name);
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int conn_fd = accept(sock_fd, (struct sockaddr*)&client_addr, &client_len);
            if (conn_fd < 0) {
                if (errno == EINTR) continue;
                perror("[-] accept failed");
                sleep(1);
                continue;
            }

            // Low latency TCP_NODELAY and keepalive
            setsockopt(conn_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
            setsockopt(conn_fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));

            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
            printf("[+] Client connected from %s:%d!\n", client_ip, ntohs(client_addr.sin_port));

            run_tunnel(tun_fd, conn_fd);

            printf("[-] Client disconnected. Waiting for new connection...\n");
            close(conn_fd);
        }

        close(sock_fd);
    } else {
        while (1) {
            printf("[*] TTun TCP Client connecting to %s:%d...\n", remote_ip, remote_port);
            int sock_fd = -1;
            while ((sock_fd = resolve_and_connect(remote_ip, remote_port)) < 0) {
                sleep(2);
            }

            // Low latency TCP_NODELAY and keepalive
            setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
            setsockopt(sock_fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));

            printf("[+] Connected to Server at %s:%d!\n", remote_ip, remote_port);

            run_tunnel(tun_fd, sock_fd);

            printf("[-] Disconnected from Server. Reconnecting in 2 seconds...\n");
            close(sock_fd);
            sleep(2);
        }
    }

    close(tun_fd);
    return 0;
}