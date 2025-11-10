void setup(){
    Serial.begin(9600);
    while(!Serial);
    Serial.println("==Arduino UART Ready!==");
}

void loop(){
    if(Serial.available()){
	int c = Serial.read();
	Serial.print("READ : ");
	Serial.write(c);
	Serial.println();
    }
}
