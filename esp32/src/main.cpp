#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// Local configurations and secrets
#include "config.h"
#include "certs.h"

// TinyML Edge Impulse SDK headers
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

WiFiClientSecure espClientSecure;
PubSubClient client(espClientSecure);

// Synchronize system time using NTP (required for TLS certificate verification)
void set_clock() {
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    ei_printf("Waiting for NTP time sync: ");
    time_t now = time(nullptr);
    while (now < 8 * 3600 * 2) {
        delay(500);
        ei_printf(".");
        now = time(nullptr);
    }
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    ei_printf("\nTime synchronized! UTC time: %s", asctime(&timeinfo));
}

// Connect to WiFi network
void setup_wifi() {
    delay(10);
    ei_printf("\nConnecting to Wi-Fi SSID: %s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        ei_printf(".");
    }
    ei_printf("\nConnected to Wi-Fi! IP address: %s\n", WiFi.localIP().toString().c_str());
}

// Reconnect to Azure IoT Hub via secure MQTT
void reconnect() {
    while (!client.connected()) {
        ei_printf("Attempting secure MQTT connection to Azure IoT Hub...\n");
        
        // Connect using Device ID as Client ID, Username, and SAS Token as Password
        if (client.connect(AZURE_DEVICE_ID, AZURE_USERNAME, AZURE_SAS_TOKEN)) {
            ei_printf("Connected to Azure IoT Hub!\n");
        } else {
            ei_printf("Connection failed, rc=%d. Retrying in 5 seconds...\n", client.state());
            delay(5000);
        }
    }
}

Adafruit_MPU6050 mpu;

#define I2C_SDA 8
#define I2C_SCL 9

#define FREQUENCY_HZ        100
#define INTERVAL_MS         (1000 / (FREQUENCY_HZ + 1))

static unsigned long last_interval_ms = 0;
float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
size_t feature_ix = 0;

int raw_feature_get_data(size_t offset, size_t length, float *out_ptr) {
    memcpy(out_ptr, features + offset, length * sizeof(float));
    return 0;
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10); 

    setup_wifi();
    set_clock();

    // Set secure TLS context
    espClientSecure.setCACert(azure_ca_cert);

    // Setup MQTT server and port
    client.setServer(AZURE_HOST, AZURE_PORT);

    Wire.begin(I2C_SDA, I2C_SCL);

    if (!mpu.begin(0x68, &Wire)) {
        ei_printf("Failed to find MPU6050 vibration sensor!\n");
        while (1) delay(10);
    }

    mpu.setAccelerometerRange(MPU6050_RANGE_16_G);
    mpu.setGyroRange(MPU6050_RANGE_2000_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_184_HZ);
    
    ei_printf("-------------------------------------------\n");
    ei_printf("VIBRATION DIAGNOSTICS SYSTEM STARTED...\n");
    ei_printf("-------------------------------------------\n");
}

void loop() {
    if (!client.connected()) {
        reconnect();
    }
    client.loop();

    if (millis() > last_interval_ms + INTERVAL_MS) {
        last_interval_ms = millis();

        sensors_event_t a, g, temp;
        mpu.getEvent(&a, &g, &temp);

        // Fill features buffer
        features[feature_ix++] = a.acceleration.x;
        features[feature_ix++] = a.acceleration.y;
        features[feature_ix++] = a.acceleration.z;
        features[feature_ix++] = g.gyro.x;
        features[feature_ix++] = g.gyro.y;
        features[feature_ix++] = g.gyro.z;

        if (feature_ix >= EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
            feature_ix = 0;
            
            ei_impulse_result_t result = { 0 };
            signal_t features_signal;
            features_signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
            features_signal.get_data = &raw_feature_get_data;

            EI_IMPULSE_ERROR res = run_classifier(&features_signal, &result, false);
            if (res != 0) {
                ei_printf("TinyML Classifier error: %d\n", res);
                return;
            }

            ei_printf("---[ MACHINE VIBRATION DIAGNOSIS RESULT ]---\n");
            
            float anomaly_val = 0.0;
            float normal_val = 0.0;

            for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
                float confidence = result.classification[ix].value;
                ei_printf(" %s:\t%.2f %%\n", result.classification[ix].label, confidence * 100.0);
                if (strcmp(result.classification[ix].label, "anomaly") == 0) {
                    anomaly_val = confidence;
                } else if (strcmp(result.classification[ix].label, "normal") == 0) {
                    normal_val = confidence;
                }
            }

            // Determine status including "warning" (medium risk)
            const char* status = "normal";
            if (anomaly_val > 0.8) {
                status = "anomaly";
                ei_printf("\n>>> [CRITICAL] VIBRATION ANOMALY DETECTED! <<<\n\n");
            } else if (anomaly_val > 0.3) {
                status = "warning";
                ei_printf("\n>>> [WARNING] VIBRATION INSTEBILITY DETECTED! <<<\n\n");
            }

            // Report-by-Exception (RBE) & Throttling:
            // Send payload immediately on status change OR every 20 seconds
            static const char* last_status = "normal";
            static unsigned long last_publish_time = 0;
            unsigned long current_time = millis();
            
            bool status_changed = (strcmp(status, last_status) != 0);
            bool interval_elapsed = (current_time - last_publish_time >= 20000); // 20 seconds

            if (status_changed || interval_elapsed) {
                last_publish_time = current_time;
                last_status = status;

                // Create JSON telemetry payload
                char payload[150];
                snprintf(payload, sizeof(payload), 
                    "{\"anomaly\":%.4f,\"normal\":%.4f,\"status\":\"%s\"}", 
                    anomaly_val, normal_val, status);

                // Build Azure telemetry topic: devices/{device_id}/messages/events/
                char telemetry_topic[80];
                snprintf(telemetry_topic, sizeof(telemetry_topic), "devices/%s/messages/events/", AZURE_DEVICE_ID);
                
                ei_printf("Publishing to Azure (Reason: %s): %s\n", 
                    status_changed ? "Status Changed" : "Interval Elapsed", payload);
                
                client.publish(telemetry_topic, payload);
            }
            ei_printf("\n");
        }
    }
}
