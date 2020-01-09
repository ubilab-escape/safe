#define S1 17
#define S2 16
#define S3 5
#define S4 18
#define S5 27
#define S6 14
#define S7 13
#define S8 12
#define LED_S 32
#define MEASUREMENT_PIN A0
unsigned int adc_val = 0;
unsigned int s1_val = 0;
unsigned int s2_val = 0;
unsigned int s3_val = 0;
unsigned int s4_val = 0;
unsigned int s5_val = 0;
unsigned int s6_val = 0;
unsigned int s7_val = 0;
unsigned int s8_val = 0;
void setup() {
  pinMode(S1,INPUT_PULLUP);
  pinMode(S2,INPUT_PULLUP);
  pinMode(S3,INPUT_PULLUP);
  pinMode(S4,INPUT_PULLUP);
  pinMode(S5,INPUT_PULLUP);
  pinMode(S6,INPUT_PULLUP);
  pinMode(S7,INPUT_PULLUP);
  pinMode(S8,INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(S1), testSolve, CHANGE);
  attachInterrupt(digitalPinToInterrupt(S2), testSolve, CHANGE);
  attachInterrupt(digitalPinToInterrupt(S3), testSolve, CHANGE);
  attachInterrupt(digitalPinToInterrupt(S4), testSolve, CHANGE);
  attachInterrupt(digitalPinToInterrupt(S5), testSolve, CHANGE);
  attachInterrupt(digitalPinToInterrupt(S6), testSolve, CHANGE);
  attachInterrupt(digitalPinToInterrupt(S7), testSolve, CHANGE);
  attachInterrupt(digitalPinToInterrupt(S8), testSolve, CHANGE);
  
  pinMode(LED_S,OUTPUT);
  digitalWrite(LED_S,LOW);
  Serial.begin(9600);
}

void loop() {
  //adc_val = analogRead(MEASUREMENT_PIN);
}

void testSolve(){
  Serial.print("S1: "); s1_val = digitalRead(S1); Serial.println(s1_val);
  Serial.print("S2: "); s2_val = digitalRead(S2); Serial.println(s2_val);
  Serial.print("S3: "); s3_val = digitalRead(S3); Serial.println(s3_val);
  Serial.print("S4: "); s4_val = digitalRead(S4); Serial.println(s4_val);
  Serial.print("S5: "); s5_val = digitalRead(S5); Serial.println(s5_val);
  Serial.print("S6: "); s6_val = digitalRead(S6); Serial.println(s6_val);
  Serial.print("S7: "); s7_val = digitalRead(S7); Serial.println(s7_val);
  Serial.print("S8: "); s8_val = digitalRead(S8); Serial.println(s8_val);
  if(!s2_val && !s3_val && !s6_val && !s8_val && s1_val && s4_val && s5_val && s7_val){
    Serial.println("Puzzle Switches Solved");
    delay(10);
  }
  digitalWrite(LED_S,(!s2_val && !s3_val && !s6_val && !s8_val && s1_val && s4_val && s5_val && s7_val));
  delay(500);
} 
