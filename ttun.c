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

// تابع ایجاد و تخصیص کارت شبکه مجازی (TUN)
int tun_alloc(char *dev) {
    struct ifreq ifr;
    int fd;

    // باز کردن دستگاه تونل در سیستم‌عامل
    if ((fd = open("/dev/net/tun", O_RDWR)) < 0) {
        perror("Error opening /dev/net/tun");
        exit(1);
    }

    memset(&ifr, 0, sizeof(ifr));
    
    // IFF_TUN: کار با بسته‌های لایه ۳ (IP)
    // IFF_NO_PI: حذف هدرهای اضافی سیستم‌عامل از بسته‌ها
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI; 
    
    if (*dev) {
        strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    }

    // ثبت رابط مجازی در کرنل لینوکس
    if (ioctl(fd, TUNSETIFF, (void *)&ifr) < 0) {
        perror("Error with ioctl TUNSETIFF");
        close(fd);
        exit(1);
    }
    
    strcpy(dev, ifr.ifr_name);
    return fd;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <local_port> <remote_ip> <remote_port>\n", argv[0]);
        exit(1);
    }

    int local_port = atoi(argv[1]);
    char *remote_ip = argv[2];
    int remote_port = atoi(argv[3]);

    char tun_name[IFNAMSIZ] = "tun0";
    int tun_fd = tun_alloc(tun_name);
    printf("[*] Interface %s created successfully.\n", tun_name);

    // ایجاد سوکت UDP برای انتقال داده‌ها در اینترنت
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    struct sockaddr_in local_addr, remote_addr;
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    local_addr.sin_port = htons(local_port);

    if (bind(sock_fd, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("Socket bind failed");
        exit(1);
    }

    memset(&remote_addr, 0, sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(remote_port);
    inet_pton(AF_INET, remote_ip, &remote_addr.sin_addr);

    printf("[*] Tunnel running. UDP bound to port %d. Forwarding to %s:%d\n", local_port, remote_ip, remote_port);

    int max_fd = (tun_fd > sock_fd) ? tun_fd : sock_fd;
    fd_set rd_set;
    char buffer[BUFFER_SIZE];

    // حلقه اصلی برنامه
    while (1) {
        FD_ZERO(&rd_set);
        FD_SET(tun_fd, &rd_set);
        FD_SET(sock_fd, &rd_set);

        // استفاده از select برای گوش دادن همزمان به TUN و UDP
        int ret = select(max_fd + 1, &rd_set, NULL, NULL, NULL);
        if (ret < 0) {
            perror("Select error");
            break;
        }

        // 1. اگر بسته جدیدی از کارت شبکه مجازی دریافت شد (ارسال شده از سیستم‌عامل)
        if (FD_ISSET(tun_fd, &rd_set)) {
            int nread = read(tun_fd, buffer, sizeof(buffer));
            if (nread > 0) {
                // ارسال بسته IP خام از طریق UDP به سرور/کلاینت مقابل
                sendto(sock_fd, buffer, nread, 0, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
            }
        }

        // 2. اگر بسته UDP جدیدی از شبکه (اینترنت) دریافت شد
        if (FD_ISSET(sock_fd, &rd_set)) {
            int nread = recvfrom(sock_fd, buffer, sizeof(buffer), 0, NULL, NULL);
            if (nread > 0) {
                // تزریق بسته دریافت شده به داخل کارت شبکه مجازی سیستم‌عامل
                write(tun_fd, buffer, nread);
            }
        }
    }

    close(tun_fd);
    close(sock_fd);
    return 0;
}