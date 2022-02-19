#include <Arduino.h>

bool ledState = false;

// Toggles a LED, used for onboard led here
// bool& is a reference, which allows the parameter variable
// to be altered by the function outside of the function
void toggleLed(bool &state, int pin)
{
  state = !state;                     // Invert LED state tracking value
  digitalWrite(pin, (state == HIGH)); // Write new value to LED
}

void setup()
{
  // put your setup code here, to run once:
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(9600);
  Serial.println("Serial initiated at 9600 baud.");
}

void loop()
{
  // put your main code here, to run repeatedly:
  toggleLed(ledState, LED_BUILTIN);
  delay(500);
}