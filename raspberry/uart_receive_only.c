#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#define UART_PATH "/dev/serial0"
#define BAUDRATE B9600
#define CSV_PATH "uart_dataset.csv"

// 한 줄 단위로 읽기
int read_line(int fd, char *buf, int max_len) {
    int idx = 0;
    char c;

    while (idx < max_len - 1) {
        if (read(fd, &c, 1) > 0) {
            if (c == '\n') break;
            buf[idx++] = c;
        }
    }
    buf[idx] = '\0';
    return idx;
}

// 랜덤 패킷 생성
void generate_random_packet(char *buf, int len) {
    static const char charset[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789";

    for (int i = 0; i < len; i++) {
        buf[i] = charset[rand() % (sizeof(charset) - 1)];
    }
    buf[len] = '\0';
}

int main(int argc, char *argv[]) {
    int uart_fd;
    struct termios options;
    char buffer[256];
    char send_packet[64];

    int packet_len = 10;
    double cable_length = 0.0;

    srand(time(NULL));

    if (argc > 1) {
        cable_length = atof(argv[1]);
    }

    uart_fd = open(UART_PATH, O_RDWR | O_NOCTTY | O_NDELAY);
    if (uart_fd < 0) {
        perror("UART open error");
        return -1;
    }

    if (tcgetattr(uart_fd, &options) < 0) {
        perror("UART attr error");
        close(uart_fd);
        return -1;
    }

    cfsetispeed(&options, BAUDRATE);
    cfsetospeed(&options, BAUDRATE);
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;

    tcflush(uart_fd, TCIFLUSH);
    tcsetattr(uart_fd, TCSANOW, &options);

    FILE *fp = fopen(CSV_PATH, "a");
    if (!fp) {
        perror("CSV open error");
        close(uart_fd);
        return -1;
    }

    while (1) {
        generate_random_packet(send_packet, packet_len);

        // 패킷 전송
        write(uart_fd, send_packet, strlen(send_packet));
        write(uart_fd, "\n", 1);

        // 응답 대기
        usleep(20000);

        // 한 줄 읽기
        if (read_line(uart_fd, buffer, sizeof(buffer)) > 0) {
            char *result = strcmp(buffer, send_packet) == 0 ? "OK" : "ERR";

            time_t now = time(NULL);
            struct tm *t = localtime(&now);
            char timestamp[64];
            strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

            fprintf(fp, "%s,%s,%s,%.2f\n",
                    timestamp, result, send_packet, cable_length);
            fflush(fp);

            printf("[%s] SENT=%s | RECV=%s | %s\n",
                   timestamp, send_packet, buffer, result);
        }
    }

    fclose(fp);
    close(uart_fd);
    return 0;
}
