
/*
   Water tank level meter and pump controller

   Uses Ultrasonic distance measuring device to detect level in water tank. Data is displayed on
   a TFT 2.2' display. Also controls the irrigation pump via a relay - the value of the attached
   potentionmeter controls the time for irrigation.

   Pin assingment:

   Pin  4 - Relay control pin
   Pin  5 - Trigger Pin (out)
   Pin  6 - Echo Pin (in)
   Pin  7 - Push button
   Pin  9 - TFT D/C
   Pin 10 - TFT C/S
   Pin 11 - TFT MOSI
   Pin 13 - TFT SCK

   Pin A0 - Potentiometer input

   Distance measure based on code from https://playground.arduino.cc/Code/SfLCD2/


   Intended for an Arduino Uno

   D. Q. McDonald
   December 2021
*/




#include <stdlib.h>
#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"
#include <Bounce2.h>
#include <arduino-timer.h>


const int RELAY_PIN = 4;
const int TRIG_PIN = 5;
const int ECHO_PIN = 6 ;
const int BUTTON_PIN = 7;
const int TIMER_POT_PIN = A0;
const long int UPDATE_INTERVAL = 1000 * 120l; // Update display every 120 seconds
const int MAX_PUMP_TIME = 3600; // Pump on for at most an hour
const int AUTO_HOUR_FREQ = 48*60;  // Turn pump automatically every two days - time in minutes
const long int AUTO_PUMP_INTERVAL = 1000 * 60l; // Check for auto-pump every minute
const double MIN_LEVEL_FOR_AUTO = 10.0;

const int TFT_DC_PIN = 9;
const int TFT_CS_PIN = 10;

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS_PIN, TFT_DC_PIN);

double distance_cm = 0;

double measure_distance(void);
double calculate_level_percentage( double distance );
long int calculate_pump_on_time(void);

double level = 13;

int current_timer_pot_val;
int last_timer_pot_val = -1;
bool pump_on = false;

int auto_pump_min_counter = 0; // The number of minutes since we've auto watered.

bool do_update = false;
void update_display(void);
bool update_cb(void*);
void pump_button_pressed(void);
void change_pump_state( bool pump_state ) ;
bool pump_cb(void*);
bool auto_pump_cb(void*);

Timer<1, millis> update_timer;
Timer<1, millis> pump_timer;
Timer<1, millis> auto_timer;

// Following are distances in cm
const double FULL_DISTANCE = 8.8; // Height when tank is full
const double EMPTY_DISTANCE = 74.5; // Height when tank is empty

// Maximum number of attempts to get a good distance:
const int MAX_DISTANCE_ATTEMPTS = 10;

Bounce debouncer = Bounce();


void setup() {

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  pinMode(TIMER_POT_PIN, INPUT);

  // Setup the button with an internal pull-up :
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // After setting up the button, setup the Bounce instance :
  debouncer.attach(BUTTON_PIN);
  debouncer.interval(25); // interval in ms

  Serial.begin(9600);
  Serial.println("Water Tank Meter");

  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(0xDDDD);
  tft.setTextColor(ILI9341_BLUE);
  tft.setCursor(50, 50);
  tft.setTextSize(4);
  tft.println("Irrigation");
  tft.setCursor(50, 110);
  tft.println("Controller");
  tft.setCursor(110, 160);
  tft.println("v1.0");
  delay(3000);

  // Force an update of the data
  do_update = true;

  // Start with pump off
  pump_on = false;
  digitalWrite( RELAY_PIN, LOW);

  update_timer.every(UPDATE_INTERVAL, update_cb);

  auto_timer.every(AUTO_PUMP_INTERVAL, auto_pump_cb );
}



void loop() {



  // Check  value of pot:
  current_timer_pot_val = analogRead(TIMER_POT_PIN);
  if ( abs(current_timer_pot_val - last_timer_pot_val ) > 3 && (not pump_on )) {
    last_timer_pot_val = current_timer_pot_val;
    Serial.print("Timer pot val = ");
    Serial.println(current_timer_pot_val);
    do_update = true;
  }

  // Update the timers:
  update_timer.tick();
  pump_timer.tick();
  auto_timer.tick();

  if ( do_update ) {
    update_display();
  }

  // Update the Bounce instance :
  debouncer.update();

  // Get the updated value :
  //int value = debouncer.read();

  if ( debouncer.fell() ) {
    pump_button_pressed();
  }


  delay(10);
}

/* Use the ultrasonic sensors to measure the distance to the water in cm, this value is returned */
double measure_distance(void)
{

  long duration;
  double distance = 50;


  int count = 0;
  bool done = false;
  while ( not done ) {


    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(45);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(11);
    digitalWrite(TRIG_PIN, LOW);
    duration = pulseIn(ECHO_PIN, HIGH);
    Serial.print("Duration = ");
    Serial.println(duration);
    distance = duration * 0.0340 / 2;

    if ( distance > 0.05  or count > MAX_DISTANCE_ATTEMPTS ) {
      done = true;
    }
    count ++;
  }

  return distance;
}

/** Given the distance to the water in cm, calculate the % of the total capacity this represents and
    return that value
*/
double calculate_level_percentage( double distance )
{
  double lvl_perc;
  //Calculate the % level
  lvl_perc = 1 - (distance - FULL_DISTANCE) / (EMPTY_DISTANCE - FULL_DISTANCE);
  lvl_perc = lvl_perc * 100.0;
  if (lvl_perc > 100.0) {
    lvl_perc = 100.0;
  }
  if ( lvl_perc < 0.0 ) {
    lvl_perc = 0.0;
  }

  return lvl_perc;
}


void update_display(void) {


  Serial.println("");
  Serial.println("Update display starting");

  distance_cm = measure_distance();
  Serial.print("Distance(cm) = ");
  Serial.println(distance_cm);

  level = calculate_level_percentage(distance_cm);
  Serial.print("Level(%) = ");
  Serial.println(level);

  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(0xDDDD);

  int rect_width = 280;
  int rect_height  = 60;
  int rect_x = 20;
  int rect_y = 20;
  tft.drawRect( rect_x, rect_y, rect_width, rect_height, ILI9341_BLUE);

  rect_width = (int)((level / 100.0) * rect_width);
  Serial.print("Rect width = ");
  Serial.println(rect_width);
  tft.fillRect( rect_x, rect_y, rect_width, rect_height, ILI9341_BLUE);
  tft.setCursor(120, rect_y + rect_height + 5);
  tft.setTextColor(ILI9341_BLUE);
  tft.setTextSize(4);
  tft.print((int)level);
  tft.println("%");

  if ( not pump_on ) {
    tft.setCursor(20, 140);
    tft.setTextSize(3);
    tft.setTextColor(ILI9341_DARKGREEN);
    tft.print("Pump time:");
    int on_time = (calculate_pump_on_time() / 60) + 1;
    tft.print(on_time);
    tft.println(" min");
    tft.setCursor(20, 180);
    tft.setTextSize(3);
    tft.setTextColor(ILI9341_PURPLE);
    tft.print("Auto time:");
    int auto_time = AUTO_HOUR_FREQ - auto_pump_min_counter;
    tft.print(int(auto_time/60)+1);
    tft.println(" hrs");
    Serial.print("Minutes to auto = ");
    Serial.println(int(auto_time));
    tft.setCursor(20, 210);
    tft.setTextSize(2);
    tft.println(auto_pump_min_counter);
    
  } else {
    rect_width = 280;
    rect_height  = 60;
    rect_x = 20;
    rect_y = 130;
    tft.drawRect( rect_x, rect_y, rect_width, rect_height, ILI9341_ORANGE);
    long int time_left = pump_timer.ticks() / 1000;
    Serial.print("Timer ticks = ");
    Serial.println(time_left);
    long int pump_on_time = calculate_pump_on_time();
    Serial.print("Time on = ");
    Serial.println(pump_on_time);
    rect_width = int( (float)(time_left) / (calculate_pump_on_time()) * rect_width);
    Serial.print("Rect width = ");
    Serial.println(rect_width);
    tft.fillRect( rect_x, rect_y, rect_width, rect_height, ILI9341_ORANGE);
    tft.setCursor(120, rect_y + rect_height + 5);
    tft.setTextColor(ILI9341_ORANGE);
    tft.setTextSize(4);
    tft.print((int)(time_left / 60.0) + 1);
    tft.println(" min");
  }

  Serial.println("Update display done");
  Serial.println("");
  do_update = false;
  return;
}

void pump_button_pressed(void) {
  // Handle the push button for the pump being pressed
  Serial.println("Button pressed");
  pump_on = ! pump_on;
  change_pump_state( pump_on );
  do_update = true;
  delay(300);
}

void change_pump_state( bool pump_state ) {
  // Turn the pump on or off depending on the state of "pump_state "
  pump_timer.cancel();
  if ( pump_state ) {
    digitalWrite(RELAY_PIN, HIGH );
    Serial.println("Turning pump on");

    long int on_time = calculate_pump_on_time() * 1000;
    Serial.print("Timer on time =");
    Serial.println(on_time);
    pump_timer.in(on_time, pump_cb);
  } else {
    Serial.println("Turning pump off");
    digitalWrite(RELAY_PIN, LOW );
  }

}

bool update_cb(void*) {
  // Call back for update timer
  do_update = true;
  return true;
}

bool auto_pump_cb(void*) {
  // A
  auto_pump_min_counter = auto_pump_min_counter + 1;

  // If time is up and level if greater than 10% then pump now:
  if ( (auto_pump_min_counter >= AUTO_HOUR_FREQ) && (level > MIN_LEVEL_FOR_AUTO)) {
    auto_pump_min_counter = 0;
    pump_on = true;
    change_pump_state(pump_on);
    do_update = true;
  }
  return true;
}

bool pump_cb(void*) {
  // Callback for pump timer
  Serial.println("Pump timer finished");
  pump_on = false;
  change_pump_state(pump_on);
  do_update = true;
  // Reset auto watering:
  auto_pump_min_counter = 0;
  return false;
}

long int calculate_pump_on_time(void) {
  // Calculate the time the pump should be on based on the
  // current position of the pot. Return time in seconds
  double frac = (1024 - current_timer_pot_val)  / 1024.0;
  long int on_time =  (int)(frac * MAX_PUMP_TIME);
  //Serial.print("Pump on time = ");
  //Serial.println(on_time);
  return on_time;
}
