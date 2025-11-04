void setup(){
    Serial.begin(9600);
    Serial.println("==Arduino UART Ready!==");
}

void loop(){
    if( Serial.available() ){
        String input = Serial.readStringUntil('\n');
	Serial.print("Sending: ");
	Serial.println(input);
	delay(1000);
    }

}
