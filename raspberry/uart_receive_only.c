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

int main(int argc, char *argv[]){
    int uart_fd;
    struct termios options;
    char buffer[256];
    char send_packet[64];

    int packet_len = 10; 
    double cable_length = 0.0;

    srand(time(NULL));  

    if (argc > 1) cable_length = atof(argv[1]);

    printf("Cable Length: %.2fm\n", cable_length);

    uart_fd = open(UART_PATH, O_RDWR | O_NOCTTY | O_NDELAY);
    if (uart_fd == -1){
        perror("Failed to open UART device");
        return -1;
    }
    printf("UART opened: %s\n", UART_PATH);

    if(tcgetattr(uart_fd, &options) < 0){
        perror("Failed to get UART attributes");
        close(uart_fd);
        return -1;
    }

    cfsetispeed(&options, BAUDRATE);
    cfsetospeed(&options, BAUDRATE);

    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cflag |= (CLOCAL | CREAD);

    options.c_iflag = IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;

    tcflush(uart_fd, TCIFLUSH);
    tcsetattr(uart_fd, TCSANOW, &options);

    FILE *fp = fopen(CSV_PATH, "a");
    if(!fp){
        perror("Failed to open CSV file");
        close(uart_fd);
        return -1;
    }
    printf("Logging to CSV: %s\n", CSV_PATH);

    while(1){
        generate_random_packet(send_packet, packet_len);

        write(uart_fd, send_packet, strlen(send_packet));
        write(uart_fd, "\n", 1);

        // ★ 추가: 아두이노 응답 기다리기 (20ms)
        usleep(20000);

        memset(buffer, 0, sizeof(buffer));

        int bytes = read(uart_fd, buffer, sizeof(buffer)-1);

        if(bytes > 0){
            buffer[bytes] = '\0';

            char *result = strstr(buffer, send_packet) ? "OK" : "ERR";

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

        // tcdrain 제거됨
    }

    fclose(fp);
    close(uart_fd);
    return 0;
}
