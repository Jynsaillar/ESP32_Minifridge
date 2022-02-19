#include <Arduino.h>

// PWM setup
const int pwmPin = 5;
const int pwmFrequency = 5000;
const int pwmChannel = 0;
const int pwmResolution = 8;

void pwmFade(int channel)
{
  // Fade in
  for (int dutyCycle = 0; dutyCycle < 255; dutyCycle++)
  {
    // Changing the LED brightness with PWM
    ledcWrite(channel, dutyCycle);
    delay(15);
  }
  // Fade out
  for (int dutyCycle = 255; dutyCycle > 0; dutyCycle--)
  {
    // Changing the LED brightness with PWM
    ledcWrite(channel, dutyCycle);
    delay(15);
  }
}

void setup()
{
  // put your setup code here, to run once:

  // PWM setup
  ledcSetup(pwmChannel, pwmFrequency, pwmResolution);
  ledcAttachPin(pwmPin, pwmChannel);

  Serial.begin(9600);
  Serial.println("Serial initiated at 9600 baud.");
}

void loop()
{
  // put your main code here, to run repeatedly
  pwmFade(pwmChannel);
}