#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define UART_PATH "/dev/serial0"
#define BAUDRATE B9600
#define CSV_PATH "uart_dataset.csv"

int main(){
    int uart_fd;
    struct termios options;
    char buffer[256];
    char expected[] = "HELLO1234";
 
    uart_fd = open(UART_PATH, O_RDWR | O_NOCTTY | O_NDELAY);
    if (uart_fd == -1){
	    perror("Failed to open UART device");
	    return -1;
    }
    printf("UART port opened: %s\n", UART_PATH);

    if(tcgetattr(uart_fd, &options) < 0){
        perror("Faliled to get UART attributes");
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
    printf("UART configured (9600 8N1)\n");

    FILE *fp = fopen(CSV_PATH, "a");

    if(!fp){
        perror("Failed to open CSV file");
        close(uart_fd);
        return -1;
    }
    printf("Logging to file : %s\n", CSV_PATH);

    while(1){
        memset(buffer, 0, sizeof(buffer));

        int bytes = read(uart_fd, buffer, sizeof(buffer)-1);

        if(bytes > 0){
            buffer[bytes] = '\0';

            char *result = strstr(buffer, expected) ? "OK" : "ERR";

            time_t now = time(NULL);
            struct tm *t = localtime(&now);
            char timestamp[64];
            strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
            fprintf(fp, "%s,%s,%s\n", timestamp, buffer, result);
            fflush(fp);

            printf("[%s] Received: %s -> %s\n", timestamp, buffer, result);
        }
        usleep(100000);
    }

    fclose(fp);
    close(uart_fd);
    close(uart_fd);

    return 0;
    
}
