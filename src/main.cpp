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
const int pwmFrequency = 5000;
const int pwmChannel = 0;
const int pwmResolution = 8;

// DHT11 temperature/humidity sensor setup
const int dht11DataPin = 18;
float temperature = 0.0f;
float humidity = 0.0f;

DHT dht11(dht11DataPin, DHT11);

std::deque<float> recentTemperatures{};
std::deque<float> recentHumidities{};
int nToKeep = 20; // How many values should be kept in the temperature/humidity deque for calculating trends
int iterationCounter = 0;

// Feedback Loop control
int correctionFactor = 128; // 50% PWM
int lastCorrectionFactor = correctionFactor;
float targetTemperature = 04.00f; // 4 degrees Celsius

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

void pwmDrive(int channel, int &correctionFactor, int &lastCorrectionFactor)
{
  Serial.print("Correction Factor: ");
  Serial.println(correctionFactor);
  Serial.print("Last Correction Factor: ");
  Serial.println(lastCorrectionFactor);
  if (correctionFactor == lastCorrectionFactor)
    return;

  ledcWrite(channel, correctionFactor);
  lastCorrectionFactor = correctionFactor;

  delay(15);
}

void SerialLogDHT11(const float &temperature, const float &humidity)
{
  Serial.print(F("Humidity: "));
  Serial.print(humidity);
  Serial.println(F("%"));

  Serial.print(F("Temperature: "));
  Serial.print(temperature);
  Serial.println(F("°C"));
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

// Removes first element in collection if the size of the collection exceeds the user-set size limit
// For example, if only 10 temperature measurements should be kept, this function will drop the oldest measurement
// if it would cause the collection to grow larger than the limit set by nToKeep
void removeOldestMeasurement(std::deque<float> &deque, int nthElement)
{
  if (deque.size() > nthElement)
  {
    deque.pop_front();
  }
}

// Updates the containers holding temperature and humidity values, purging their respective first element if they would exceed their set container size limit 'nthElement'
void trackMeasurements(std::deque<float> &container, const float &value, int nthElement)
{
  if (isnan(value))
  {
    return;
  }
  container.push_back(value);
  removeOldestMeasurement(container, nthElement);
}

char *dequeToString(const std::deque<float> input)
{
  int size = input.size();

  char buffer[size * 8]; // 3 digits, a dot, 2 digits, a comma, space = 8 chars

  for (auto &element : input)
  {
    dtostrf(element, 5, 2, buffer);
  }

  return buffer;
}

void trendAnalysis(const std::deque<float> temperatures, const std::deque<float> humidities, int nthElement, int &iterationCounter)
{
  if (iterationCounter < nthElement)
  {
    return;
  }

  float temperatureTrend = calculateTrend(temperatures);
  float humidityTrend = calculateTrend(humidities);

  if (temperatureTrend < 0.0f)
  {
    Serial.print("Temperature is falling, trend = ");
  }
  if (temperatureTrend == 0.0f)
  {
    Serial.print("Temperature doesn't change, trend = ");
  }
  if (temperatureTrend > 0.0f)
  {
    Serial.print("Temperature is rising, trend = ");
  }

  Serial.println(temperatureTrend);

  Serial.println(dequeToString(temperatures));

  if (humidityTrend < 0.0f)
  {
    Serial.print("Humidity is falling, trend = ");
  }
  if (humidityTrend == 0.0f)
  {
    Serial.print("Humidity doesn't change, trend = ");
  }
  if (humidityTrend > 0.0f)
  {
    Serial.print("Humidity is rising, trend = ");
  }

  Serial.println(humidityTrend);

  Serial.println(dequeToString(humidities));

  iterationCounter = 0;
}

void pollDHT11(std::deque<float> &temperatures, std::deque<float> &humidities, float &temperature, float &humidity, const int &nthElement)
{
  delay(2000); // Wait 2 seconds between polls to give the sensor time to measure

  humidity = dht11.readHumidity();
  temperature = dht11.readTemperature(); // In Celsius

  trackMeasurements(humidities, humidity, nthElement);
  trackMeasurements(temperatures, temperature, nthElement);

  if (isnan(humidity))
  {
    Serial.println("Humidity reading failed!");
  }
  if (isnan(temperature))
  {
    Serial.println("Temperature reading failed!");
  }
}

// Increases correction factor (= PWM duty cycle) depending on the temperature trend
void correctionLoop(const float &targetTemperature, float &actualTemperature, int &correctionFactor)
{
  // Correction factor is proportional to the relation of actualTemperature vs. targetTemperature, more power = more cooling
  if (actualTemperature > targetTemperature)
  {
    if (correctionFactor >= 60)
      correctionFactor -= 10;
  }

  if (actualTemperature < targetTemperature)
  {
    if (correctionFactor <= 185)
      correctionFactor += 10;
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

  dht11.begin(); // Pulls up for 55us by default
}

void loop()
{
  // put your main code here, to run repeatedly
  // pwmFade(pwmChannel);
  pollDHT11(recentTemperatures, recentHumidities, temperature, humidity, nToKeep); // Measures temperature & humidity, stores them in containers for monitoring of recent temperature/humidity trends
  SerialLogDHT11(temperature, humidity);
  trendAnalysis(recentTemperatures, recentHumidities, nToKeep, iterationCounter);
  correctionLoop(targetTemperature, temperature, correctionFactor);
  pwmDrive(pwmChannel, correctionFactor, lastCorrectionFactor);
  iterationCounter++;
  delay(3000);
}