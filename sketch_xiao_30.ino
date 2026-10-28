/*
  sensing-network capacitive sensing port for XIAO ESP32-C3

  Wiring:
    D2 / GPIO4 (send) -- resistor -- conductive object -- D3 / GPIO5 (receive)

  Based on the sensing-network / CapacitiveSensorR4 behavior:
    - CYCLES_PER_READING charge/discharge cycles per reported value
    - accumulated GPIO polling-loop counts, not microseconds
    - raw and smoothed values as: raw,smoothed

  Changes for the level-based (four-height) direction sensor:
    - 100 cycles per reading instead of 30: ~3x larger counts, better SNR,
      ~50 readings/s instead of ~175. Level separation matters more than rate.
    - No baseline subtraction on the board. The host tracks the baseline and
      recalibrates, so the board just reports the raw accumulated count.
    - A sensing timeout prints a line starting with '#' instead of -2, so a
      host that ignores comment lines never sees a bogus sample.
    - Send 'r' from the host to reset the smoothing filter.

  Adapted from CapacitiveSensorR4 by Takanori Fujiwara and S. Sandra Bae,
  which refers to Paul Badger's CapacitiveSensor library.
  Original license: CC BY-NC-SA 4.0.
*/

#include <Arduino.h>

constexpr uint8_t SEND_PIN = D2;
constexpr uint8_t RECEIVE_PIN = D3;
constexpr uint8_t CYCLES_PER_READING = 100;
constexpr uint32_t DISCHARGE_US = 100;
constexpr uint32_t EDGE_TIMEOUT_US = 50000;
constexpr float EMA_ALPHA = 0.35F;

class MedianEmaFilter
{
public:
  long update(long value)
  {
    history_[historyIndex_] = value;
    historyIndex_ = (historyIndex_ + 1) % 3;
    if (historyCount_ < 3)
    {
      ++historyCount_;
    }

    const long medianValue = historyCount_ < 3
                                 ? value
                                 : medianOfThree(history_[0], history_[1], history_[2]);
    if (!initialized_)
    {
      ema_ = static_cast<float>(medianValue);
      initialized_ = true;
    }
    else
    {
      ema_ += EMA_ALPHA * (static_cast<float>(medianValue) - ema_);
    }
    return static_cast<long>(ema_ + 0.5F);
  }

  void reset()
  {
    historyIndex_ = 0;
    historyCount_ = 0;
    initialized_ = false;
    ema_ = 0.0F;
  }

private:
  static long medianOfThree(long a, long b, long c)
  {
    if (a > b)
    {
      const long temp = a;
      a = b;
      b = temp;
    }
    if (b > c)
    {
      const long temp = b;
      b = c;
      c = temp;
    }
    if (a > b)
    {
      b = a;
    }
    return b;
  }

  long history_[3] = {0, 0, 0};
  uint8_t historyIndex_ = 0;
  uint8_t historyCount_ = 0;
  bool initialized_ = false;
  float ema_ = 0.0F;
};

class CapacitiveSensorESP32
{
public:
  CapacitiveSensorESP32(uint8_t sendPin, uint8_t receivePin)
      : sendPin_(sendPin), receivePin_(receivePin)
  {
  }

  void begin()
  {
    pinMode(sendPin_, OUTPUT);
    digitalWrite(sendPin_, LOW);
    pinMode(receivePin_, INPUT);
  }

  long readRaw(uint8_t cycles)
  {
    if (cycles == 0)
    {
      return 0;
    }

    uint32_t total = 0;
    for (uint8_t i = 0; i < cycles; ++i)
    {
      if (!senseOneCycle(total))
      {
        return -2;
      }
    }
    return static_cast<long>(total);
  }

private:
  bool senseOneCycle(uint32_t &total)
  {
    // Discharge both sides, matching CapacitiveSensorR4's sequence.
    noInterrupts();
    digitalWrite(sendPin_, LOW);
    pinMode(receivePin_, INPUT);
    pinMode(receivePin_, OUTPUT);
    digitalWrite(receivePin_, LOW);
    delayMicroseconds(DISCHARGE_US);
    pinMode(receivePin_, INPUT);
    digitalWrite(sendPin_, HIGH);
    interrupts();

    uint32_t edgeStart = micros();
    while (digitalRead(receivePin_) == LOW)
    {
      ++total;
      if (static_cast<uint32_t>(micros() - edgeStart) >= EDGE_TIMEOUT_US)
      {
        digitalWrite(sendPin_, LOW);
        return false;
      }
    }

    // Charge Receive fully, then count its HIGH-to-LOW discharge loops.
    noInterrupts();
    digitalWrite(receivePin_, HIGH);
    pinMode(receivePin_, OUTPUT);
    digitalWrite(receivePin_, HIGH);
    pinMode(receivePin_, INPUT);
    digitalWrite(sendPin_, LOW);
    interrupts();

    edgeStart = micros();
    while (digitalRead(receivePin_) == HIGH)
    {
      ++total;
      if (static_cast<uint32_t>(micros() - edgeStart) >= EDGE_TIMEOUT_US)
      {
        return false;
      }
    }

    return true;
  }

  uint8_t sendPin_;
  uint8_t receivePin_;
};

CapacitiveSensorESP32 sensor(SEND_PIN, RECEIVE_PIN);
MedianEmaFilter filter;

void setup()
{
  Serial.begin(115200);
  sensor.begin();
  delay(1000);
}

void loop()
{
  // Send "r" from the host to reset the smoothing filter.
  if (Serial.available() && Serial.read() == 'r')
  {
    filter.reset();
  }

  const long rawValue = sensor.readRaw(CYCLES_PER_READING);
  if (rawValue < 0)
  {
    // Comment line: hosts that parse "number,number" skip it.
    Serial.println("#timeout: receive pin did not toggle within 50 ms, check wiring");
    return;
  }

  const long smoothedValue = filter.update(rawValue);
  Serial.print(rawValue);
  Serial.print(',');
  Serial.println(smoothedValue);
}
