/*
 * UART 통신 프로그램 (라즈베리파이 - 아두이노)
 * 
 * 동작 원리:
 * 1. 랜덤 문자열 생성
 * 2. UART로 아두이노에 전송
 * 3. 아두이노가 에코백(echo back)한 데이터 수신
 * 4. 송신/수신 데이터 비교하여 통신 품질 측정
 * 5. 결과를 CSV 파일에 저장
 * 
 * 사용법: ./program <케이블길이> <baudrate>
 * 예시: ./program 1.5 9600
 *       ./program 2.0 115200
 */

 #include <stdio.h>      // printf, fopen 등 표준 입출력
 #include <fcntl.h>      // open() - 파일/디바이스 열기
 #include <termios.h>    // UART 설정 구조체 및 함수
 #include <unistd.h>     // read, write, usleep 등 POSIX 함수
 #include <string.h>     // strcmp, strlen 등 문자열 함수
 #include <time.h>       // 타임스탬프용
 #include <stdlib.h>     // rand, atof 등
 
 // 라즈베리파이의 UART 디바이스 경로
 #define UART_PATH "/dev/serial0"
 
 // 데이터 저장할 CSV 파일 경로
 #define CSV_PATH "uart_dataset.csv"
 
 /*
  * Baudrate 값을 termios 상수로 변환
  * 
  * 파라미터: baudrate (숫자, 예: 9600)
  * 반환값: termios 상수 (예: B9600), 지원 안 하면 -1
  */
 speed_t get_baudrate_constant(int baudrate) {
     switch (baudrate) {
         case 9600:    return B9600;
         case 19200:   return B19200;
         case 38400:   return B38400;
         case 57600:   return B57600;
         case 115200:  return B115200;
         case 230400:  return B230400;
         default:      return -1;  // 지원하지 않는 baudrate
     }
 }
 
 /*
  * 헥스 덤프 함수 (디버깅용)
  */
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
 
 /*
  * UART에서 한 줄 읽기 함수
  */
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
 
 /*
  * 랜덤 패킷 생성 함수
  */
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
     // ========== 변수 선언 ==========
     int uart_fd;
     struct termios options;
     char buffer[256];
     char send_packet[64];
 
     int packet_len = 10;
     double cable_length = 0.0;
     int baudrate = 9600;  // 기본값
 
     srand(time(NULL));
 
     // ========== 명령줄 인자 처리 ==========
     /*
      * 사용법: ./program <케이블길이> [baudrate]
      * 
      * 예시:
      *   ./program 1.5          → 1.5m, 9600 bps
      *   ./program 1.5 115200   → 1.5m, 115200 bps
      */
     if (argc < 2) {
         printf("Usage: %s <cable_length> [baudrate]\n", argv[0]);
         printf("Example: %s 1.5 115200\n", argv[0]);
         printf("\nSupported baudrates: 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600\n");
         return -1;
     }
 
     cable_length = atof(argv[1]);  // 첫 번째 인자: 케이블 길이
     
     if (argc > 2) {
         baudrate = atoi(argv[2]);  // 두 번째 인자: baudrate
     }
 
     // Baudrate 유효성 검사
     speed_t baud_const = get_baudrate_constant(baudrate);
     if (baud_const == (speed_t)-1) {
         printf("Error: Unsupported baudrate %d\n", baudrate);
         printf("Supported: 9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600\n");
         return -1;
     }
 
     printf("===========================================\n");
     printf("Cable Length: %.2f m\n", cable_length);
     printf("Baudrate: %d bps\n", baudrate);
     printf("===========================================\n\n");
 
     // ========== UART 디바이스 열기 ==========
     printf("Opening UART: %s\n", UART_PATH);
     uart_fd = open(UART_PATH, O_RDWR | O_NOCTTY);
     if (uart_fd < 0) {
         perror("UART open error");
         return -1;
     }
     printf("UART opened successfully: fd=%d\n", uart_fd);
 
     // ========== UART 설정 ==========
     if (tcgetattr(uart_fd, &options) < 0) {
         perror("UART attr error");
         close(uart_fd);
         return -1;
     }
 
     // Baudrate 설정 (매개변수로 받은 값 사용)
     cfsetispeed(&options, baud_const);
     cfsetospeed(&options, baud_const);
     
     options.c_cflag = baud_const | CS8 | CLOCAL | CREAD;
     options.c_iflag = IGNPAR;
     options.c_oflag = 0;
     options.c_lflag = 0;
     options.c_cc[VMIN] = 0;
     options.c_cc[VTIME] = 10;
 
     tcflush(uart_fd, TCIOFLUSH);
     tcsetattr(uart_fd, TCSANOW, &options);
     printf("UART configured: %d 8N1\n", baudrate);
 
     // ========== CSV 파일 열기 ==========
     FILE *fp = fopen(CSV_PATH, "a");
     if (!fp) {
         perror("CSV open error");
         close(uart_fd);
         return -1;
     }
 
     // ========== 아두이노 초기화 대기 ==========
     printf("Waiting for Arduino initialization...\n");
     sleep(2);
     tcflush(uart_fd, TCIOFLUSH);
     printf("Starting communication loop...\n\n");
 
     // ========== 메인 통신 루프 ==========
     int loop_count = 0;
     while (1) {
         printf("\n========== Loop %d ==========\n", ++loop_count);
         
         generate_random_packet(send_packet, packet_len);
         tcflush(uart_fd, TCOFLUSH);
         
         printf("[SEND] Writing packet...\n");
         ssize_t written = write(uart_fd, send_packet, strlen(send_packet));
         write(uart_fd, "\n", 1);
         tcdrain(uart_fd);
         printf("[SEND] Written %zd bytes\n", written);
         print_hex("SENT", send_packet);
 
         printf("[RECV] Waiting for response (100ms)...\n");
         usleep(100000);
 
         int len = read_line(uart_fd, buffer, sizeof(buffer));
         
         if (len > 0) {
             print_hex("RECV_RAW", buffer);
             
             // Trim
             char *trimmed = buffer;
             while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
             char *end = trimmed + strlen(trimmed) - 1;
             while (end > trimmed && 
                    (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
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
 
             // CSV에 저장: 타임스탬프,결과,패킷,케이블길이,Baudrate
             fprintf(fp, "%s,%s,%s,%.2f,%d\n",
                     timestamp, result, send_packet, cable_length, baudrate);
             fflush(fp);
 
             printf("\n[RESULT] %s\n", result);
             printf("[LOG] %s,%s,%s,%.2f,%d\n",
                    timestamp, result, send_packet, cable_length, baudrate);
         } else {
             printf("[ERROR] No response received (len=%d)\n", len);
         }
 
         tcflush(uart_fd, TCIFLUSH);
         printf("\n[WAIT] 100ms before next loop...\n");
         usleep(100000);
     }
 
     fclose(fp);
     close(uart_fd);
     return 0;
 }