#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

Adafruit_MPU6050 mpu;

// Definicja pinów I2C dla ESP32-S3
#define I2C_SDA 8
#define I2C_SCL 9

void setup() {
  Serial.begin(115200);
  while (!Serial)
    delay(10); 

  Serial.println("Inicjalizacja testu MPU6050 z ESP32-S3 (PlatformIO)...");

  // Inicjalizacja I2C
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!mpu.begin(0x68, &Wire)) {
    Serial.println("Nie udało się znaleźć układu MPU6050. Sprawdź okablowanie!");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPU6050 znaleziony prawidłowo!");

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  
  Serial.println("=========================================");
  Serial.println("Inicjalizacja zakończona.");
  delay(100);
}

void loop() {
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  Serial.print("Akcelerometria [m/s^2] | X: ");
  Serial.print(a.acceleration.x, 2);
  Serial.print(" | Y: ");
  Serial.print(a.acceleration.y, 2);
  Serial.print(" | Z: ");
  Serial.println(a.acceleration.z, 2);

  Serial.print("Żyroskop [rad/s]       | X: ");
  Serial.print(g.gyro.x, 2);
  Serial.print(" | Y: ");
  Serial.print(g.gyro.y, 2);
  Serial.print(" | Z: ");
  Serial.println(g.gyro.z, 2);

  Serial.println("-----------------------------------------");
  delay(1000);
}
