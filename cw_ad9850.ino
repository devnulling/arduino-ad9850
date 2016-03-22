//todo: add original source of file to header
//Has the ability to send morse code from the serial port
#define WPM 20
#define pttOut 13
#define pwmOut 5
#define toneFrequency 400  //Hz
#define W_CLK 8       // Pin 8 - connect to AD9850 module word load clock pin (CLK)
#define FQ_UD 9       // Pin 9 - connect to freq update pin (FQ)
#define DATA 10       // Pin 10 - connect to serial data load pin (DATA)
#define RESET 11      // Pin 11 - connect to reset pin (RST).
#define txfrequency 14015000

byte morseLookup[] = {
                      //Letters
                      B01000001,B10001000,B10001010,B01100100,B00100000,B10000010,
                      B01100110,B10000000,B01000000,B10000111,B01100101,B10000100,
                      B01000011,B01000010,B01100111,B10000110,B10001101,B01100010,
                      B01100000,B00100001,B01100001,B10000001,B01100011,B10001001,
                      B10001011,B10001100,
                      //Numbers
                      B10111111,B10101111,B10100111,B10100011,B10100001,B10100000,
                      B10110000,B10111000,B10111100,B10111110,B10111110,
                      //Slash
                      B10110010
                    };


void setup(){
  Serial.begin(9600);
  Serial.print("Loading...");
  pinMode(pttOut,OUTPUT);
  pinMode(pwmOut,OUTPUT);
  pinMode(4,OUTPUT);
  digitalWrite(4,LOW);
  pinMode(3,OUTPUT);
  digitalWrite(3,HIGH);
  setupDDS();
  delay(500);
  Serial.println(" complete");
}

void loop(){
  if(Serial.available()){sendSerialMessage();}
  transmitString("73 de callsign");
  delay(5000);
  transmitString("QST de callsign");
  delay(5000);
}

// transfers a byte, a bit at a time, LSB first to the 9850 via serial DATA line
void tfr_byte(byte data)
{
  for (int i=0; i<8; i++, data>>=1) {
    digitalWrite(DATA, data & 0x01);
    pulseHigh(W_CLK);   //after each bit sent, CLK is pulsed high
  }
}

void sendFrequency(double frequency) {// frequency calc from datasheet page 8 = <sys clock> * <frequency tuning word>/2^32
  int32_t freq = frequency * 4294967295/125000000;  // note 125 MHz clock on 9850
  for (int b=0; b<4; b++, freq>>=8) {
    tfr_byte(freq & 0xFF);
  }
  tfr_byte(0x000);   // Final control byte, all 0 for 9850 chip
  pulseHigh(FQ_UD);  // Done!  Should see output
}

void pulseHigh(int pin){
  digitalWrite(pin, HIGH);
  digitalWrite(pin, LOW);
}

void setupDDS(){
  pinMode(FQ_UD, OUTPUT);
  pinMode(W_CLK, OUTPUT);
  pinMode(DATA, OUTPUT);
  pinMode(RESET, OUTPUT);
  pulseHigh(RESET);
  pulseHigh(W_CLK);
  pulseHigh(FQ_UD);  // this pulse enables serial mode - Datasheet page 12 figure 10
}

void sendSerialMessage(){
  delay(10);
  char message[64];
  int length = 0;
  
  while(Serial.available() && length < 64){
    message[length] = Serial.read();
    length++;
    message[length] = '\0';
  }
   transmitString(message);
}

void transmitString(char* message){
  for(int i = 0; message[i] != '\0'; i++){
    Serial.print(message[i]);
    transmitChar(message[i]);
  }
  Serial.println();
  wordSpace();
}

void transmitChar(char character){
  int lookupValue;
  if(character > 64 && character < 91){  //Capital Letter (0-25)
    lookupValue = character - 65;
  }
  else if(character > 96 && character < 123){  //Lower Case Letter (0-25)
    lookupValue = character - 97;
  }
  else if(character > 47 && character < 58){   //Number (26-36)
    lookupValue = character - 48 + 26;
  }
  else if(character == 47){  // slash (37)
    lookupValue = 37;
  }
  else if(character == 32){  // space
    wordSpace();
    return;
  }
  else{
    return;  //Invalid Character
  }
  byte length = (morseLookup[lookupValue] & B11100000) >> 5;
  byte pattern = morseLookup[lookupValue] & B00011111;
  byte mask = 1 << length-1;
  for(int i = 0; i < length; i++){
    if(mask & morseLookup[lookupValue]){
      dash();
    }
    else{
      dot();
    }
    mask = mask >> 1;
  }
  charSpace();
}

void dot(){
  digitalWrite(pttOut,HIGH);
  sendFrequency(txfrequency);
  tone(pwmOut,toneFrequency);
  delay(1200/WPM);
  digitalWrite(pttOut,LOW);
  sendFrequency(0);
  noTone(pwmOut);
  delay(1200/WPM);
}

void dash(){
  digitalWrite(pttOut,HIGH);
  sendFrequency(txfrequency);
  tone(pwmOut,toneFrequency);
  delay(3 * 1200 / WPM);
  digitalWrite(pttOut,LOW);
  sendFrequency(0);
  noTone(pwmOut);
  delay(1200 / WPM);
}

void charSpace(){
  delay(2 * 1200 / WPM);
}

void wordSpace(){
  delay(7 * 1200/WPM);
}
