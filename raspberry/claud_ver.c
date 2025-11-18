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

// 헥스 덤프 함수 (디버깅용)
void print_hex(const char *label, const char *str) {
    printf("%s [len=%zu]: ", label, strlen(str));
    for (int i = 0; str[i] != '\0'; i++) {
        printf("%02X ", (unsigned char)str[i]);
    }
    printf("| \"");
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] >= 32 && str[i] <= 126)
            printf("%c", str[i]);
        else
            printf("<%02X>", (unsigned char)str[i]);
    }
    printf("\"\n");
}

// 한 줄 단위로 읽기 (개선 + 디버깅)
int read_line(int fd, char *buf, int max_len) {
    int idx = 0;
    char c;
    int timeout = 0;

    printf("[DEBUG] Starting read_line...\n");

    while (idx < max_len - 1 && timeout < 200) {
        ssize_t n = read(fd, &c, 1);
        if (n > 0) {
            printf("[DEBUG] Read byte: 0x%02X ('%c')\n", (unsigned char)c, 
                   (c >= 32 && c <= 126) ? c : '?');
            
            // \r과 \n 모두 처리
            if (c == '\n' || c == '\r') {
                if (idx > 0) {
                    printf("[DEBUG] Line end detected, idx=%d\n", idx);
                    break;
                }
                continue;
            }
            buf[idx++] = c;
            timeout = 0;
        } else {
            usleep(1000);
            timeout++;
        }
    }
    buf[idx] = '\0';
    printf("[DEBUG] read_line complete: idx=%d, timeout=%d\n", idx, timeout);
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

    printf("Opening UART: %s\n", UART_PATH);
    uart_fd = open(UART_PATH, O_RDWR | O_NOCTTY);
    if (uart_fd < 0) {
        perror("UART open error");
        return -1;
    }
    printf("UART opened successfully: fd=%d\n", uart_fd);

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
    
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 10;

    tcflush(uart_fd, TCIOFLUSH);
    tcsetattr(uart_fd, TCSANOW, &options);
    printf("UART configured: 9600 8N1\n");

    FILE *fp = fopen(CSV_PATH, "a");
    if (!fp) {
        perror("CSV open error");
        close(uart_fd);
        return -1;
    }

    printf("Waiting for Arduino initialization...\n");
    sleep(2);
    tcflush(uart_fd, TCIOFLUSH);
    printf("Starting communication loop...\n\n");

    int loop_count = 0;
    while (1) {
        printf("\n========== Loop %d ==========\n", ++loop_count);
        
        generate_random_packet(send_packet, packet_len);

        // 송신 버퍼 클리어
        tcflush(uart_fd, TCOFLUSH);
        
        // 패킷 전송
        printf("[SEND] Writing packet...\n");
        ssize_t written = write(uart_fd, send_packet, strlen(send_packet));
        write(uart_fd, "\n", 1);
        tcdrain(uart_fd);
        printf("[SEND] Written %zd bytes\n", written);
        print_hex("SENT", send_packet);

        // 응답 대기
        printf("[RECV] Waiting for response (300ms)...\n");
        usleep(300000);

        // 한 줄 읽기
        int len = read_line(uart_fd, buffer, sizeof(buffer));
        
        if (len > 0) {
            print_hex("RECV_RAW", buffer);
            
            // 공백 제거
            char *trimmed = buffer;
            while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
            
            char *end = trimmed + strlen(trimmed) - 1;
            while (end > trimmed && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
                *end = '\0';
                end--;
            }
            
            print_hex("RECV_TRIMMED", trimmed);

            int cmp_result = strcmp(trimmed, send_packet);
            printf("strcmp(RECV, SENT) = %d\n", cmp_result);
            
            char *result = (cmp_result == 0) ? "OK" : "ERR";

            time_t now = time(NULL);
            struct tm *t = localtime(&now);
            char timestamp[64];
            strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);

            fprintf(fp, "%s,%s,%s,%.2f\n",
                    timestamp, result, send_packet, cable_length);
            fflush(fp);

            printf("\n[RESULT] %s\n", result);
            printf("[LOG] %s,%s,%s,%.2f\n",
                   timestamp, result, send_packet, cable_length);
        } else {
            printf("[ERROR] No response received (len=%d)\n", len);
        }

        // 버퍼 클리어
        tcflush(uart_fd, TCIFLUSH);
        printf("\n[WAIT] 500ms before next loop...\n");
        usleep(500000);
    }

    fclose(fp);
    close(uart_fd);
    return 0;
}