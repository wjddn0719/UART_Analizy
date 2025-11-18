void setup() {
    Serial.begin(9600);
    Serial.println("Arduino UART Ready");
}

void loop() {
    static String sentence = "";

    while (Serial.available() > 0) {
        char c = Serial.read();

        if (c == '\n') {
            if (sentence.length() > 0) {
                Serial.println(sentence);  // 순수 패킷만 반환
                Serial.flush();             // 출력 즉시 완료
                sentence = "";
            }
        } else {
            sentence += c;
        }
    }
}
