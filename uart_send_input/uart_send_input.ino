void setup(){
    Serial.begin(9600);
    Serial.println("==Arduino UART Ready!==");
}

void loop(){
	if(Serial.available() > 0){
		String c = Serial.readString();
		Serial.print("Echo : ");
		Serial.println(c);
	}
}
