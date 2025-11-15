void setup() {
    Serial1.begin(9600);          // GPIO TX/RX UART
    Serial1.println("== Arduino UART Sentence Receiver Ready ==");
}

void loop() {
    static String sentence = "";

    // Serial1로 들어오는 모든 바이트 처리
    while (Serial1.available() > 0) {
        char c = Serial1.read();

        // 문장 끝: LF('\n') 또는 CR('\r')
        if (c == '\n' || c == '\r') {
            if (sentence.length() > 0) {
                sentence.trim();   // 양쪽 공백 제거

                // 받은 문장 그대로 다시 전송 (에코)
                Serial1.print("RECV: ");
                Serial1.println(sentence);

                sentence = "";     // 다음 문장을 위한 초기화
            }
        } else {
            // 버퍼에 문자 추가
            sentence += c;
        }
    }
}
