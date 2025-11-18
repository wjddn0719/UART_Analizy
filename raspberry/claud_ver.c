/*
 * UART 통신 프로그램 (라즈베리파이 - 아두이노)
 * 
 * 동작 원리:
 * 1. 랜덤 문자열 생성
 * 2. UART로 아두이노에 전송
 * 3. 아두이노가 에코백(echo back)한 데이터 수신
 * 4. 송신/수신 데이터 비교하여 통신 품질 측정
 * 5. 결과를 CSV 파일에 저장
 */

 #include <stdio.h>      // printf, fopen 등 표준 입출력
 #include <fcntl.h>      // open() - 파일/디바이스 열기
 #include <termios.h>    // UART 설정 구조체 및 함수
 #include <unistd.h>     // read, write, usleep 등 POSIX 함수
 #include <string.h>     // strcmp, strlen 등 문자열 함수
 #include <time.h>       // 타임스탬프용
 #include <stdlib.h>     // rand, atof 등
 
 // 라즈베리파이의 UART 디바이스 경로 (/dev/ttyS0, /dev/ttyAMA0 등도 가능)
 #define UART_PATH "/dev/serial0"
 
 // 통신 속도 (9600 bps = 초당 약 960바이트)
 #define BAUDRATE B9600
 
 // 데이터 저장할 CSV 파일 경로
 #define CSV_PATH "uart_dataset.csv"
 
 /*
  * 헥스 덤프 함수 (디버깅용)
  * 
  * 목적: 문자열을 16진수로 출력하여 보이지 않는 제어문자(\r, \n 등) 확인
  * 
  * 예시 출력:
  * SENT [len=5]: 41 42 43 0D 0A | "ABC<0D><0A>"
  *                               보이는 문자  제어문자
  */
 void print_hex(const char *label, const char *str) {
     // 레이블과 문자열 길이 출력
     printf("%s [len=%zu]: ", label, strlen(str));
     
     // 각 바이트를 16진수로 출력 (예: 41 = 'A')
     for (int i = 0; str[i] != '\0'; i++) {
         printf("%02X ", (unsigned char)str[i]);
     }
     
     printf("| \"");
     
     // 실제 문자 출력 (출력 가능한 문자는 그대로, 제어문자는 <16진수>로)
     for (int i = 0; str[i] != '\0'; i++) {
         if (str[i] >= 32 && str[i] <= 126)  // 출력 가능한 ASCII 범위
             printf("%c", str[i]);
         else
             printf("<%02X>", (unsigned char)str[i]);  // \r=<0D>, \n=<0A> 등
     }
     printf("\"\n");
 }
 
 /*
  * UART에서 한 줄 읽기 함수
  * 
  * UART는 바이트 스트림으로 데이터가 오기 때문에 "한 줄"이라는 개념이 없음
  * 이 함수는 \n 또는 \r을 만날 때까지 바이트를 읽어서 한 줄로 만듦
  * 
  * 파라미터:
  *   fd: UART 파일 디스크립터
  *   buf: 읽은 데이터를 저장할 버퍼
  *   max_len: 버퍼 최대 크기
  * 
  * 반환값: 읽은 바이트 수 (0 = 타임아웃)
  */
 int read_line(int fd, char *buf, int max_len) {
     int idx = 0;        // 현재 버퍼에 저장된 문자 개수
     char c;             // 읽은 문자 하나 저장
     int timeout = 0;    // 타임아웃 카운터 (200ms까지 대기)
 
     printf("[DEBUG] Starting read_line...\n");
 
     // 최대 길이 또는 타임아웃까지 반복
     while (idx < max_len - 1 && timeout < 200) {
         // UART에서 1바이트 읽기 (non-blocking)
         ssize_t n = read(fd, &c, 1);
         
         if (n > 0) {  // 데이터를 성공적으로 읽음
             // 읽은 바이트를 16진수와 문자로 출력 (디버깅용)
             printf("[DEBUG] Read byte: 0x%02X ('%c')\n", (unsigned char)c, 
                    (c >= 32 && c <= 126) ? c : '?');
             
             // 줄바꿈 문자(\n) 또는 캐리지 리턴(\r) 처리
             if (c == '\n' || c == '\r') {
                 if (idx > 0) {  // 이미 데이터가 있으면 한 줄 완성
                     printf("[DEBUG] Line end detected, idx=%d\n", idx);
                     break;  // 읽기 종료
                 }
                 continue;  // 빈 줄이면 무시하고 계속
             }
             
             // 일반 문자는 버퍼에 저장
             buf[idx++] = c;
             timeout = 0;  // 데이터를 받았으므로 타임아웃 리셋
         } else {
             // 데이터가 없으면 1ms 대기 후 재시도
             usleep(1000);
             timeout++;
         }
     }
     
     // 문자열 끝에 NULL 문자 추가 (C 문자열 규칙)
     buf[idx] = '\0';
     
     printf("[DEBUG] read_line complete: idx=%d, timeout=%d\n", idx, timeout);
     return idx;  // 읽은 바이트 수 반환
 }
 
 /*
  * 랜덤 패킷 생성 함수
  * 
  * 목적: 테스트용 랜덤 문자열 생성 (알파벳 대소문자 + 숫자)
  * 
  * 파라미터:
  *   buf: 생성된 문자열을 저장할 버퍼
  *   len: 생성할 문자열 길이
  */
 void generate_random_packet(char *buf, int len) {
     // 사용할 문자 집합 (대문자 26개 + 소문자 26개 + 숫자 10개 = 62개)
     static const char charset[] =
         "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
         "abcdefghijklmnopqrstuvwxyz"
         "0123456789";
 
     // 랜덤하게 문자 선택하여 버퍼에 저장
     for (int i = 0; i < len; i++) {
         buf[i] = charset[rand() % (sizeof(charset) - 1)];
     }
     
     // 문자열 끝에 NULL 문자 추가
     buf[len] = '\0';
 }
 
 int main(int argc, char *argv[]) {
     // ========== 변수 선언 ==========
     int uart_fd;                 // UART 파일 디스크립터 (디바이스를 가리키는 번호)
     struct termios options;      // UART 설정을 담는 구조체
     char buffer[256];            // 수신 데이터 저장 버퍼
     char send_packet[64];        // 송신할 패킷 버퍼
 
     int packet_len = 10;         // 패킷 길이 (10자)
     double cable_length = 0.0;   // 케이블 길이 (CSV에 기록용)
     int baudrate = 9600;         // 실제 baud rate 값 (CSV에 기록용)
 
     // 랜덤 시드 초기화 (현재 시간 기반)
     srand(time(NULL));
 
     // 명령줄 인자로 케이블 길이 입력받기 (예: ./program 1.5)
     if (argc > 1) {
         cable_length = atof(argv[1]);  // 문자열을 실수로 변환
     }
 
     // ========== UART 디바이스 열기 ==========
     printf("Opening UART: %s\n", UART_PATH);
     
     /*
      * open() 함수로 UART 디바이스 열기
      * 
      * O_RDWR: 읽기/쓰기 모드
      * O_NOCTTY: 터미널 제어 안 함 (프로그램이 백그라운드에서도 동작)
      * 
      * 반환값: 파일 디스크립터 (성공 시 3 이상, 실패 시 -1)
      */
     uart_fd = open(UART_PATH, O_RDWR | O_NOCTTY);
     if (uart_fd < 0) {
         perror("UART open error");  // 에러 메시지 출력
         return -1;
     }
     printf("UART opened successfully: fd=%d\n", uart_fd);
 
     // ========== UART 현재 설정 읽기 ==========
     /*
      * tcgetattr(): 현재 UART 설정을 options 구조체에 저장
      * 설정을 수정하기 전에 먼저 현재 설정을 읽어야 함
      */
     if (tcgetattr(uart_fd, &options) < 0) {
         perror("UART attr error");
         close(uart_fd);
         return -1;
     }
 
     // ========== UART 설정 ==========
     /*
      * UART 통신 파라미터 설정
      * - Baud rate: 9600 bps
      * - Data bits: 8 bit
      * - Parity: None
      * - Stop bits: 1 bit
      * 
      * 이걸 "9600 8N1"이라고 표현함
      */
     
     // 입력/출력 속도 설정 (9600 bps)
     cfsetispeed(&options, BAUDRATE);
     cfsetospeed(&options, BAUDRATE);
     
     /*
      * c_cflag: Control flags (하드웨어 설정)
      * 
      * BAUDRATE: 통신 속도
      * CS8: 8비트 데이터
      * CLOCAL: 모뎀 제어선 무시 (로컬 연결)
      * CREAD: 수신 활성화
      */
     options.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
     
     /*
      * c_iflag: Input flags (입력 처리 방식)
      * IGNPAR: 패리티 에러 무시
      */
     options.c_iflag = IGNPAR;
     
     /*
      * c_oflag: Output flags (출력 처리 방식)
      * 0: 출력 처리 없음 (Raw 모드)
      */
     options.c_oflag = 0;
     
     /*
      * c_lflag: Local flags (로컬 모드)
      * 0: Canonical 모드 비활성화 (Raw 모드)
      *    → 데이터를 있는 그대로 전송/수신
      */
     options.c_lflag = 0;
     
     /*
      * c_cc: Control characters (특수 제어 설정)
      * 
      * VMIN: read()가 반환하기 위한 최소 바이트 수
      *       0 = 데이터 없어도 바로 반환 (non-blocking)
      * 
      * VTIME: read() 타임아웃 (0.1초 단위)
      *        10 = 1초 타임아웃
      */
     options.c_cc[VMIN] = 0;
     options.c_cc[VTIME] = 10;
 
     /*
      * tcflush(): UART 버퍼 비우기
      * 
      * TCIOFLUSH: 송신/수신 버퍼 모두 비움
      * → 이전에 남아있던 데이터 제거 (중요!)
      */
     tcflush(uart_fd, TCIOFLUSH);
     
     /*
      * tcsetattr(): 설정 적용
      * 
      * TCSANOW: 즉시 적용
      */
     tcsetattr(uart_fd, TCSANOW, &options);
     printf("UART configured: 9600 8N1\n");
 
     // ========== CSV 파일 열기 ==========
     /*
      * fopen()으로 CSV 파일 열기
      * "a" 모드: append (파일 끝에 추가, 없으면 생성)
      */
     FILE *fp = fopen(CSV_PATH, "a");
     if (!fp) {
         perror("CSV open error");
         close(uart_fd);
         return -1;
     }
 
     // ========== 아두이노 초기화 대기 ==========
     printf("Waiting for Arduino initialization...\n");
     sleep(2);  // 2초 대기 (아두이노가 부팅하고 시리얼 준비될 때까지)
     
     // 버퍼에 남아있을 수 있는 데이터 제거
     tcflush(uart_fd, TCIOFLUSH);
     printf("Starting communication loop...\n\n");
 
     // ========== 메인 통신 루프 ==========
     int loop_count = 0;
     while (1) {
         printf("\n========== Loop %d ==========\n", ++loop_count);
         
         // ===== 1단계: 랜덤 패킷 생성 =====
         generate_random_packet(send_packet, packet_len);
 
         // ===== 2단계: 송신 버퍼 클리어 =====
         /*
          * tcflush(fd, TCOFLUSH): 송신(Transmit Output) 버퍼 비우기
          * 
          * 왜 필요한가?
          * - 이전 루프에서 전송 중인 데이터가 남아있을 수 있음
          * - 깨끗한 상태에서 새 데이터 전송하기 위해
          */
         tcflush(uart_fd, TCOFLUSH);
         
         // ===== 3단계: 패킷 전송 =====
         printf("[SEND] Writing packet...\n");
         
         /*
          * write(): UART로 데이터 전송
          * 
          * 반환값: 실제로 쓴 바이트 수
          * 
          * 주의: write()가 반환되어도 데이터가 실제로 전송된 것은 아님!
          *       데이터는 송신 버퍼에 들어가고, 하드웨어가 천천히 전송함
          */
         ssize_t written = write(uart_fd, send_packet, strlen(send_packet));
         write(uart_fd, "\n", 1);  // 줄바꿈 문자 전송 (아두이노가 한 줄로 인식)
         
         /*
          * tcdrain(): 송신 완료까지 대기
          * 
          * 송신 버퍼의 모든 데이터가 실제로 UART를 통해 전송될 때까지 블로킹
          * 이게 없으면 데이터가 전송 중인데 다음 동작을 할 수 있음
          */
         tcdrain(uart_fd);
         
         printf("[SEND] Written %zd bytes\n", written);
         print_hex("SENT", send_packet);  // 전송한 데이터 확인
 
         // ===== 4단계: 응답 대기 =====
         /*
          * usleep(): 마이크로초 단위 대기
          * 300000 μs = 300 ms = 0.3초
          * 
          * 왜 필요한가?
          * 1. 아두이노가 데이터를 받아서 처리할 시간 필요
          * 2. 아두이노가 응답을 보낼 시간 필요
          * 3. 응답 데이터가 UART 수신 버퍼에 도착할 시간 필요
          * 
          * 9600 bps에서 10바이트 전송 시간 ≈ 10ms
          * 하지만 아두이노 처리 시간 + 여유를 위해 300ms 설정
          */
         printf("[RECV] Waiting for response (300ms)...\n");
         usleep(300000);
 
         // ===== 5단계: 응답 읽기 =====
         /*
          * read_line()으로 한 줄 읽기
          * \n을 만날 때까지 읽어서 buffer에 저장
          */
         int len = read_line(uart_fd, buffer, sizeof(buffer));
         
         if (len > 0) {  // 데이터를 받았으면
             print_hex("RECV_RAW", buffer);  // 받은 원본 데이터 확인
             
             // ===== 6단계: 문자열 정리 (Trim) =====
             /*
              * Trim: 문자열 앞뒤의 공백/제어문자 제거
              * 
              * 왜 필요한가?
              * - 아두이노에서 보낸 데이터에 공백이나 \r이 포함될 수 있음
              * - strcmp()는 정확히 일치해야 하므로 불필요한 문자 제거 필요
              */
             
             // 앞쪽 공백 제거
             char *trimmed = buffer;
             while (*trimmed == ' ' || *trimmed == '\t') 
                 trimmed++;  // 포인터를 앞으로 이동
             
             // 뒤쪽 공백/제어문자 제거
             char *end = trimmed + strlen(trimmed) - 1;  // 문자열 끝 포인터
             while (end > trimmed && 
                    (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
                 *end = '\0';  // NULL 문자로 대체 (문자열 끝 당기기)
                 end--;        // 포인터를 뒤로 이동
             }
             
             print_hex("RECV_TRIMMED", trimmed);  // 정리된 데이터 확인
 
             // ===== 7단계: 문자열 비교 =====
             /*
              * strcmp(): 두 문자열 비교
              * 
              * 반환값:
              *   0: 완전히 동일
              *   양수: 첫 번째 문자열이 더 큼 (사전순)
              *   음수: 두 번째 문자열이 더 큼
              * 
              * 예시:
              * strcmp("ABC", "ABC") = 0
              * strcmp("ABC", "ABD") < 0  (C < D)
              * strcmp("ABD", "ABC") > 0  (D > C)
              */
             int cmp_result = strcmp(trimmed, send_packet);
             printf("strcmp(RECV, SENT) = %d\n", cmp_result);
             
             // 결과 판정: 0이면 OK, 아니면 ERR
             char *result = (cmp_result == 0) ? "OK" : "ERR";
 
             // ===== 8단계: 타임스탬프 생성 =====
             /*
              * 현재 시간을 "YYYY-MM-DD HH:MM:SS" 형식으로 만들기
              */
             time_t now = time(NULL);                    // 현재 시간 (초 단위)
             struct tm *t = localtime(&now);             // 로컬 시간으로 변환
             char timestamp[64];
             strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
 
             // ===== 9단계: CSV 파일에 기록 =====
             /*
              * CSV 형식: 타임스탬프, 결과(OK/ERR), 전송 패킷, 케이블 길이, Baud Rate
              * 예: 2025-11-18 14:30:45,OK,AbCdEfGhIj,1.50,9600
              */
             fprintf(fp, "%s,%s,%s,%.2f,%d\n",
                     timestamp, result, send_packet, cable_length, baudrate);
             
             /*
              * fflush(): 버퍼를 디스크에 즉시 쓰기
              * 
              * 왜 필요한가?
              * - fprintf()는 메모리 버퍼에만 쓰고 나중에 디스크에 씀
              * - 프로그램이 갑자기 종료되면 데이터 손실 가능
              * - fflush()로 즉시 저장하면 데이터 보존
              */
             fflush(fp);
 
             // ===== 10단계: 결과 출력 =====
             printf("\n[RESULT] %s\n", result);
             printf("[LOG] %s,%s,%s,%.2f,%d\n",
                    timestamp, result, send_packet, cable_length, baudrate);
         } else {
             // 타임아웃: 응답을 받지 못함
             printf("[ERROR] No response received (len=%d)\n", len);
         }
 
         // ===== 11단계: 수신 버퍼 클리어 =====
         /*
          * tcflush(fd, TCIFLUSH): 수신(Transmit Input) 버퍼 비우기
          * 
          * 왜 필요한가?
          * - 타이밍이 안 맞아서 늦게 도착한 데이터가 있을 수 있음
          * - 다음 루프에서 이전 데이터를 읽지 않도록 깨끗하게 비움
          * - 이게 없으면 "문자열이 밀리는" 현상 발생!
          */
         tcflush(uart_fd, TCIFLUSH);
         
         // ===== 12단계: 다음 루프 대기 =====
         printf("\n[WAIT] 500ms before next loop...\n");
         usleep(500000);  // 0.5초 대기
     }
 
     // ========== 종료 처리 (실제로는 무한루프라 실행 안 됨) ==========
     fclose(fp);        // CSV 파일 닫기
     close(uart_fd);    // UART 디바이스 닫기
     return 0;
 }