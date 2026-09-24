// src/ttun-core.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 2048

// Ensure all bytes are sent over the TCP stream
int send_all(int socket, void *buffer, size_t length) {
    char *ptr = (char*) buffer;
    while (length > 0) {
        int i = send(socket, ptr, length, 0);
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
    if ((fd = open("/dev/net/tun", O_RDWR)) < 0) return -1;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI; 
    if (*dev) strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    if (ioctl(fd, TUNSETIFF, (void *) &ifr) < 0) { close(fd); return -1; }
    strcpy(dev, ifr.ifr_name);
    return fd;
}

int main(int argc, char *argv[]) {
    // Usage: ttun-core <role> <local_port> <remote_ip> <remote_port> <tun_name>
    if (argc < 6) return 1;

    char *role = argv[1]; // "server" or "client"
    int local_port = atoi(argv[2]);
    char *remote_ip = argv[3];
    int remote_port = atoi(argv[4]);
    char *tun_name = argv[5];

    int tun_fd = tun_alloc(tun_name);
    if (tun_fd < 0) exit(1);

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    int conn_fd = -1;

    if (strcmp(role, "server") == 0) {
        int opt = 1;
        setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(local_port);
        
        bind(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
        listen(sock_fd, 1);
        printf("[*] TTun TCP Server listening on port %d...\n", local_port);
        
        conn_fd = accept(sock_fd, NULL, NULL);
        printf("[+] Client connected!\n");
    } else {
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(remote_port);
        inet_pton(AF_INET, remote_ip, &server_addr.sin_addr);
        
        printf("[*] TTun TCP Client connecting to %s:%d...\n", remote_ip, remote_port);
        while (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            close(sock_fd);
            sleep(2); // Keep retrying if server is offline
            sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        }
        conn_fd = sock_fd;
        printf("[+] Connected to Server!\n");
    }

    fd_set rd_set;
    char buffer[BUFFER_SIZE];

    // Main TCP forwarding loop
    while (1) {
        FD_ZERO(&rd_set);
        FD_SET(tun_fd, &rd_set);
        FD_SET(conn_fd, &rd_set);
        int max_fd = (tun_fd > conn_fd) ? tun_fd : conn_fd;
        
        if (select(max_fd + 1, &rd_set, NULL, NULL, NULL) < 0) break;
        
        // 1. Read from TUN, write to TCP
        if (FD_ISSET(tun_fd, &rd_set)) {
            int nread = read(tun_fd, buffer, sizeof(buffer));
            if (nread > 0) {
                uint16_t len = htons(nread); // Prefix packet with its length
                if (send_all(conn_fd, &len, 2) < 0) break;
                if (send_all(conn_fd, buffer, nread) < 0) break;
            }
        }
        
        // 2. Read from TCP, write to TUN
        if (FD_ISSET(conn_fd, &rd_set)) {
            uint16_t len;
            if (recv_all(conn_fd, &len, 2) < 0) break; // Read length prefix
            len = ntohs(len);
            if (len > sizeof(buffer)) break; // Prevent buffer overflow
            if (recv_all(conn_fd, buffer, len) < 0) break; // Read exact packet
            write(tun_fd, buffer, len);
        }
    }

    close(tun_fd);
    close(conn_fd);
    if(sock_fd != conn_fd) close(sock_fd);
    return 1; // Return 1 so Systemd (Restart=always/on-failure) will auto-restart the service
}