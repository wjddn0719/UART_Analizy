void setup() {
    Serial.begin(9600);              // UNO에서는 Serial1 없음
    Serial.println("== Arduino UART Sentence Receiver Ready ==");
}

void loop() {
    static String sentence = "";

    while (Serial.available() > 0) {
        char c = Serial.read();

        if (c == '\n' || c == '\r') {
            if (sentence.length() > 0) {
                sentence.trim();

                Serial.print("RECV: ");
                Serial.println(sentence);

                sentence = "";
            }
        } else {
            sentence += c;
        }
    }
}
