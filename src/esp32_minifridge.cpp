#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include <Arduino.h>

#include <iostream>
#include <deque>
#include <numeric>
#include <string>
#include <algorithm>
#include <sstream>
#include <iterator>

const bool debugLogging = false;

// PWM setup
const int pwmPin = 5;
const int pwmFrequency = 5000; // In Hz
const int pwmChannel = 0;
const int pwmResolution = 8;

// DHT11 temperature/humidity sensor setup
const int dht11DataPin = 18;
float temperature = 0.0f; // In celsius
float humidity = 0.0f; // In % relative humidity

DHT dht11(dht11DataPin, DHT11);

std::deque<float> recentTemperatures{}; // Container for recently measured temperature values
std::deque<float> recentHumidities{}; // Container for recently measured humidity values
int nToKeep = 20; // How many values should be kept in the temperature/humidity deque for calculating trends
int numRecentMeasurements = 0; // Keeps track of how many measurements have been taken recently, this is needed for analysing the current temperature/humidity trend (rising, falling, not changing)

// Feedback Loop control
// The reason 'correctionFactor' isn't 0.0f by default then going positive/negative is that it'd require an extra function
// to convert 0.0f as equilibrium value to a 50% duty cycle equilibrium value of 128 ( to keep the temperature stable at w/e value it is currently at)
int correctionFactor = 128; // Default = 128 for a 50% PWM duty cycle
int lastCorrectionFactor = correctionFactor;
float targetTemperature = 04.00f; // 4 degrees Celsius

void adjustPWMDutyCycle(int channel, int &correctionFactor, int &lastCorrectionFactor)
{
  Serial.print("Correction Factor: ");
  Serial.println(correctionFactor);
  Serial.print("Last Correction Factor: ");
  Serial.println(lastCorrectionFactor);
  if (correctionFactor == lastCorrectionFactor)
    return;

  // ledc is a stupid name for a PWM library since it is derived more than what it *actually* does: providing a PWM signal, but that is its name in the ESP32 standard library
  // There is also MCPWM, but this is designed for motor control with feedback at low frequencies, while peltier modules need a higher PWM frequency to be somewhat efficient
  ledcWrite(channel, correctionFactor);
  lastCorrectionFactor = correctionFactor;

  delay(15);
}

void logDHT11DataToSerial(const float &temperature, const float &humidity)
{
  Serial.print(F("Humidity: "));
  Serial.print(humidity);
  Serial.println(F("%"));

  Serial.print(F("Temperature: "));
  Serial.print(temperature);
  Serial.println(F("??C"));
}

// Sums all elements in the deque and returns them as a float
float sumDeque(const std::deque<float> &deque)
{
  float sum = 0.0f;

  for (auto &n : deque)
  {
    sum += n;
  }

  return sum;
}

// Positive result -> upwards
// Negative result -> downwards
// 0.0f -> unchanged
float calculateTrend(const std::deque<float> &input)
{
  float summedInput = sumDeque(input);
  int inputSize = static_cast<int>(input.size()); // Necessary, otherwise all calculations below will return garbage values
  auto multipliedData = 0;
  auto summedIndex = 0;
  auto squaredIndex = 0;

  // Scary math magics start here for calculating a negative, neutral, or positive trend value
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

// Removes first element in collection if the size of the collection exceeds the user-set size limit
// For example, if only 10 temperature measurements should be kept, this function will drop the oldest measurement once an 11th measurement is added
void removeOldestMeasurement(std::deque<float> &deque, int sizeLimit)
{
  if (deque.size() > sizeLimit)
  {
    deque.pop_front();
  }
}

// Updates the container holding measurements, purging its respective first element 
// if adding a new value would exceed the container's set size limit 'sizeLimit'
void trackMeasurements(std::deque<float> &container, const float &value, int sizeLimit)
{
  if (isnan(value))
  {
    Serial.println("Invalid measurement received: " + String(value, /*decimal places*/ 2));
    return;
  }
  container.push_back(value);
  removeOldestMeasurement(container, sizeLimit);
}

std::string dequeToString(const std::deque<float> input)
{
  return std::string(input.begin(), input.end());
}

void trendAnalysis(const std::deque<float> temperatures, const std::deque<float> humidities, int sizeLimit, int &numRecentMeasurements)
{
  // Skip analysis until at least 'sizeLimit' measurements have been taken
  if (numRecentMeasurements < sizeLimit)
  {
    Serial.println("Not enough measurements recorded yet, skipping analysis...");
    return;
  }

  float temperatureTrend = calculateTrend(temperatures);
  float humidityTrend = calculateTrend(humidities);

  if (temperatureTrend < 0.0f)  {
    Serial.print("Temperature is falling, trend = ");
  }
  if (temperatureTrend == 0.0f)  {
    Serial.print("Temperature doesn't change, trend = ");
  }
  if (temperatureTrend > 0.0f)  {
    Serial.print("Temperature is rising, trend = ");
  }

  Serial.println(temperatureTrend);
  Serial.println(dequeToString(temperatures).c_str());

  if (humidityTrend < 0.0f)  {
    Serial.print("Humidity is falling, trend = ");
  }
  if (humidityTrend == 0.0f)  {
    Serial.print("Humidity doesn't change, trend = ");
  }
  if (humidityTrend > 0.0f)  {
    Serial.print("Humidity is rising, trend = ");
  }

  Serial.println(humidityTrend);
  Serial.println(dequeToString(humidities).c_str());

  numRecentMeasurements = 0;
}

void pollDHT11(std::deque<float> &temperatures, std::deque<float> &humidities, float &temperature, float &humidity, const int &sizeLimit)
{
  delay(2000); // Wait 2 seconds between polls to give the sensor time to measure

  humidity = dht11.readHumidity();
  temperature = dht11.readTemperature(); // In Celsius

  trackMeasurements(humidities, humidity, sizeLimit);
  trackMeasurements(temperatures, temperature, sizeLimit);

  if (isnan(humidity))  {
    Serial.println("Humidity reading failed!");
  }
  if (isnan(temperature))  {
    Serial.println("Temperature reading failed!");
  }
}

// Increases correction factor (= PWM duty cycle) depending on the temperature trend
void adjustCorrectionFactor(const float &targetValue, float &actualValue, int &correctionFactor, int lowerLimit, int upperLimit, int stepSize)
{
  // Correction factor is proportional to the relation of actualValue vs. targetValue, more power = more cooling
  if (actualValue > targetValue)
  {
    if (correctionFactor >= lowerLimit)
      correctionFactor -= stepSize;
  }

  if (actualValue < targetValue)
  {
    if (correctionFactor <= upperLimit)
      correctionFactor += stepSize;
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

  dht11.begin(); // Initializes the DHT11 temperature+humidity sensor, pulls up for 55us by default
}

void loop()
{
  // put your main code here, to run repeatedly

  pollDHT11(recentTemperatures, recentHumidities, temperature, humidity, nToKeep); // Measures temperature & humidity, stores them in containers for monitoring of recent temperature/humidity trends
  logDHT11DataToSerial(temperature, humidity);
  trendAnalysis(recentTemperatures, recentHumidities, nToKeep, numRecentMeasurements);
  adjustCorrectionFactor(targetTemperature, temperature, correctionFactor, /*lowerLimit*/ 60, /*upperLimit*/ 185, /*stepSize*/ 10);
  adjustPWMDutyCycle(pwmChannel, correctionFactor, lastCorrectionFactor);
  numRecentMeasurements++;
  delay(3000);
}