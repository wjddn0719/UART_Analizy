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

// 한 줄 단위로 읽기 (개선)
int read_line(int fd, char *buf, int max_len) {
    int idx = 0;
    char c;
    int timeout = 0;

    while (idx < max_len - 1 && timeout < 100) {
        ssize_t n = read(fd, &c, 1);
        if (n > 0) {
            // \r과 \n 모두 처리
            if (c == '\n' || c == '\r') {
                if (idx > 0) break;  // 데이터가 있을 때만 종료
                continue;  // 빈 줄 무시
            }
            buf[idx++] = c;
            timeout = 0;  // 데이터 수신 시 타임아웃 리셋
        } else {
            usleep(1000);  // 1ms 대기
            timeout++;
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

    uart_fd = open(UART_PATH, O_RDWR | O_NOCTTY);
    if (uart_fd < 0) {
        perror("UART open error");
        return -1;
    }

    if (tcgetattr(uart_fd, &options) < 0) {
        perror("UART attr error");
        close(uart_fd);
        return -1;
    }

    // UART 설정
    cfsetispeed(&options, BAUDRATE);
    cfsetospeed(&options, BAUDRATE);
    options.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;
    
    // Read 설정: 최소 1바이트, 타임아웃 1초
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 10;

    tcflush(uart_fd, TCIOFLUSH);  // 송수신 버퍼 모두 flush
    tcsetattr(uart_fd, TCSANOW, &options);

    FILE *fp = fopen(CSV_PATH, "a");
    if (!fp) {
        perror("CSV open error");
        close(uart_fd);
        return -1;
    }

    printf("UART Communication Started...\n");
    sleep(2);  // 아두이노 초기화 대기

    // 초기 버퍼 클리어
    tcflush(uart_fd, TCIOFLUSH);

    while (1) {
        generate_random_packet(send_packet, packet_len);

        // 송신 버퍼 클리어
        tcflush(uart_fd, TCOFLUSH);
        
        // 패킷 전송
        write(uart_fd, send_packet, strlen(send_packet));
        write(uart_fd, "\n", 1);
        tcdrain(uart_fd);  // 전송 완료 대기

        // 응답 대기 (충분한 시간 확보)
        usleep(200000);  // 200ms

        // 한 줄 읽기
        int len = read_line(uart_fd, buffer, sizeof(buffer));
        
        if (len > 0) {
            // 공백 제거
            char *trimmed = buffer;
            while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
            
            char *end = trimmed + strlen(trimmed) - 1;
            while (end > trimmed && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
                *end = '\0';
                end--;
            }

            char *result = strcmp(trimmed, send_packet) == 0 ? "OK" : "ERR";

            time_t now = time(NULL);
            struct tm *t = localtime(&now);
            char timestamp[64];
            strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

            fprintf(fp, "%s,%s,%s,%.2f\n",
                    timestamp, result, send_packet, cable_length);
            fflush(fp);

            printf("[%s] SENT=%s | RECV=%s | %s\n",
                   timestamp, send_packet, trimmed, result);
        } else {
            printf("No response received\n");
        }

        // 다음 전송 전 버퍼 클리어
        tcflush(uart_fd, TCIFLUSH);
        usleep(100000);  // 100ms 간격
    }

    fclose(fp);
    close(uart_fd);
    return 0;
}