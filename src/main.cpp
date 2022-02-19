#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <Arduino.h>

#include <iostream>
#include <vector>
#include <numeric>

const bool debugLogging = false;

// PWM setup
const int pwmPin = 5;
const int pwmFrequency = 5000;
const int pwmChannel = 0;
const int pwmResolution = 8;

// DHT11 temperature/humidity sensor setup
const int dht11DataPin = 18;
float temperature = 0.0f;
float humidity = 0.0f;

DHT dht11(dht11DataPin, DHT11);

std::vector<float> lastMeasuredTemperatureN{};
std::vector<float> lastMeasuredHumidityN{};
int nToKeep = 10; // How many values should be kept in the temperature/humidity vector for calculating trends

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

void pollDHT11(float &temperature, float &humidity)
{
  delay(2000); // Wait 2 seconds between polls to give the sensor time to measure

  humidity = dht11.readHumidity();
  temperature = dht11.readTemperature(); // In Celsius

  if (isnan(humidity))
  {
    Serial.println("Humidity reading failed!");
  }
  if (isnan(temperature))
  {
    Serial.println("Temperature reading failed!");
  }
}

void SerialLogDHT11(float temperature, float humidity)
{
  Serial.print(F("Humidity: "));
  Serial.print(humidity);
  Serial.println(F("%"));

  Serial.print(F("Temperature: "));
  Serial.print(temperature);
  Serial.println(F("°C"));
}

// Sums all elements in the vector and returns them as a float
float sumVector(const std::vector<float> &vector)
{
  float sum = 0.0f;

  for (auto &n : vector)
  {
    sum += n;
  }

  return sum;
}

// Positive result -> upwards
// Negative result -> downwards
// 0.0f -> unchanged
float calculateTrend(const std::vector<float> &input)
{
  float summedInput = sumVector(input);
  int inputSize = static_cast<int>(input.size());
  auto multipliedData = 0;
  auto summedIndex = 0;
  auto squaredIndex = 0;

  // Scary math magics start here
  int index = 0;
  for (auto &n : input)
  {
    index++;
    multipliedData += index * n;
    summedIndex += index;
    squaredIndex += index * index;
  }

  auto numerator = (inputSize * multipliedData) - (summedInput * summedIndex);
  auto denominator = (inputSize * squaredIndex) - (summedIndex * summedIndex);

  if (debugLogging)
  {
    Serial.println("Input Size");
    Serial.println(inputSize);

    Serial.println("Summed Input");
    Serial.println(summedInput);

    Serial.println("Index");
    Serial.println(index);
    Serial.println("Multiplied Data");
    Serial.println(multipliedData);
    Serial.println("Summed Index");
    Serial.println(summedIndex);
    Serial.println("Squared Index");
    Serial.println(squaredIndex);
    Serial.println("Numerator");
    Serial.println(numerator);
    Serial.println("Left Half");
    Serial.println(inputSize * multipliedData);
    Serial.println("Right Half");
    Serial.println(summedInput * summedIndex);

    Serial.println("Denominator");
    Serial.println(denominator);
    Serial.println("Left Half");
    Serial.println(inputSize * squaredIndex);
    Serial.println("Right Half");
    Serial.println(summedIndex * summedIndex);
  }

  if (denominator != 0)
  {
    return numerator / denominator;
  }

  return 0.0f; // By default, return value for an unchanging input set (stagnant set, no trend measurable)
}

// Drops values for last
void purgeLastNCache()
{
}

void setup()
{
  // put your setup code here, to run once:

  // PWM setup
  ledcSetup(pwmChannel, pwmFrequency, pwmResolution);
  ledcAttachPin(pwmPin, pwmChannel);

  Serial.begin(9600);
  Serial.println("Serial initiated at 9600 baud.");

  dht11.begin(); // Pulls up for 55us by default
}

void loop()
{
  // put your main code here, to run repeatedly
  // pwmFade(pwmChannel);
  // pollDHT11(temperature, humidity);
  // SerialLogDHT11(temperature, humidity);
}