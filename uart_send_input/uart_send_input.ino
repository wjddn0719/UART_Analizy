void setup() {
    Serial.begin(230400);
    while (!Serial) {
        ; // 시리얼 포트 준비 대기
    }
    Serial.setTimeout(100);
    delay(1000);
}

void loop() {
    if (Serial.available() > 0) {
        String received = Serial.readStringUntil('\n');
        
        // 공백 및 제어문자 제거
        received.trim();
        
        if (received.length() > 0) {
            // 순수 패킷만 반환 (println 대신 print + \n)
            Serial.print(received);
            Serial.print('\n');
            Serial.flush();
        }
    }
}