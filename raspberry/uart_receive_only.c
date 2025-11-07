#include <stdio.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>

#define UART_PATH = "/dev/serial0";
#define BAUDRATE = B9600;

int main(){
    int uart_fd;
    struct termios options;
    char buffer[256];

    uart_fd = open(UART_PATH, O_RDWR | O_NOCTTY | O_NDELAY);

    if (uart_fd == -1){
	perror("Failed to open UART device");
	return -1;
     }

    
}
