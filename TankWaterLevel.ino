/*
 * Water tank level meter
 * 
 * Uses Ultrasonic distance measuring device to detect level in water tank
 * 
 * Pin assingment:
 *  
 * Pin 3 = Software Serial Transmit
 * Pin 5 - Trigger Pin (out)
 * Pin 6 - Echo Pin (in)
 * 
 * Distance measure based on code from https://playground.arduino.cc/Code/SfLCD2/
 * 
 * 
 * Intended for 5V Arduino Pro Mini
 * 
 * D. Q. McDonald
 * December 2021
 */




#include <SendOnlySoftwareSerial.h>
#include <stdlib.h>

const int TX_PIN = 3;
const int TRIG_PIN = 5;
const int ECHO_PIN = 6;

SendOnlySoftwareSerial ss(TX_PIN);  // Tx pin
const int BUFF_SIZE = 21;
char buff[BUFF_SIZE];
char temp[10];

// Following are distances in cm
const double FULL_DISTANCE = 5.8;  // Height when tank is full
const double EMPTY_DISTANCE = 74.5; // Height when tank is empty

// 20x2 Serial Enabled LCD display
class SerialLCD {
public:
  SerialLCD(); 
  void displayScreen( const char* theText );
  void displayLine(int lineNum, const char *theText);
  void displayChar(int lineNum, int charNum, char theChar);
  void clear();
  void backlight(int Percentage);

private:
  int d_pin;

};


SerialLCD lcd;


long duration;
double distance_cm;
double level; 


void setup() {
 
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  Serial.begin(9600);
  lcd.clear();

}
void loop() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  duration = pulseIn(ECHO_PIN, HIGH);
  distance_cm = duration * 0.0340 / 2;
  Serial.print("Distance(cm) = ");
  Serial.println(distance_cm);

  //Calculate the % level
  level = 1 - (distance_cm-FULL_DISTANCE)/(EMPTY_DISTANCE-FULL_DISTANCE);
  level = level * 100.0;
  if (level > 100.0) {
    level = 100.0;
  }
  if( level < 0.0 ) {
    level = 0.0;
  }
  
  lcd.clear();
  
  dtostrf(level , 4, 1, temp); //first 2 is the width including the . (1.) and the 2nd 2 is the precision (.23)
  snprintf(buff,BUFF_SIZE, "Level  = %s %%", temp );
  lcd.displayLine(1, buff);
  
  dtostrf(distance_cm , 4, 1, temp); //first 2 is the width including the . (1.) and the 2nd 2 is the precision (.23)
  snprintf(buff,BUFF_SIZE, "Dist.  = %s cm", temp );
  
  
  lcd.displayLine(2, buff);
  delay(1000);
}
