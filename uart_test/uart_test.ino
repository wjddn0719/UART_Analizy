void setup() {
    Serial.begin(9600);
    while (!Serial);
    Serial.println("== Arduino UART Sentence Receiver Ready ==");
  }
  
  void loop() {
    static String sentence = "";
  
    // 수신된 모든 바이트를 하나씩 읽기
    while (Serial.available() > 0) {
      char c = Serial.read();
  
      // 줄바꿈(엔터) 문자 중 하나 감지하면 문장 완성
      if (c == '\n' || c == '\r') {
        if (sentence.length() > 0) { // 빈 문자열 무시
          sentence.trim();  // 앞뒤 공백 제거
          Serial.print("RECEIVED: ");
          Serial.println(sentence);
          sentence = ""; // 다음 문장 위해 초기화
        }
      } else {
        sentence += c;  // 아직 줄바꿈 안 나왔으면 누적
      }
    }
  }
  