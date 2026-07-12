import os
import json
import logging
import time
from azure.eventhub import EventHubConsumerClient
import paho.mqtt.client as mqtt

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(levelname)s] %(name)s: %(message)s'
)
logger = logging.getLogger("azure-bridge")

# Read configuration from environment
EVENTHUB_CONN_STR = os.getenv("AZURE_EVENTHUB_CONNECTION_STRING")
EVENTHUB_NAME = os.getenv("AZURE_EVENTHUB_NAME")
CONSUMER_GROUP = os.getenv("AZURE_EVENTHUB_CONSUMER_GROUP", "$Default")

MQTT_BROKER = os.getenv("MQTT_BROKER", "mosquitto")
MQTT_PORT = int(os.getenv("MQTT_PORT", 1883))
MQTT_TOPIC = os.getenv("MQTT_TOPIC", "NovaTech/Warsaw/Assembly/Line1/EdgeAI/Anomaly")

# Initialize MQTT Client
mqtt_client = mqtt.Client()

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        logger.info(f"Connected to Mosquitto UNS broker at {MQTT_BROKER}:{MQTT_PORT}")
    else:
        logger.error(f"Failed to connect to Mosquitto broker, return code {rc}")

def on_disconnect(client, userdata, rc):
    logger.warning(f"Disconnected from Mosquitto broker with code {rc}. Reconnecting...")
    try:
        client.reconnect()
    except Exception as e:
        logger.error(f"Failed to reconnect to Mosquitto broker: {e}")

mqtt_client.on_connect = on_connect
mqtt_client.on_disconnect = on_disconnect

# Helper to safely extract the IoT Hub device ID from system properties
def get_device_id(system_properties):
    if not system_properties:
        return "unknown"
    
    # Try different key types (bytes/str) depending on SDK version
    for key in [b'iothub-connection-device-id', 'iothub-connection-device-id']:
        if key in system_properties:
            val = system_properties[key]
            if isinstance(val, bytes):
                return val.decode('utf-8')
            return str(val)
    return "unknown"

# Event Hub message handler
def on_event(partition_context, event):
    try:
        # Get raw event payload
        body_str = event.body_as_str(encoding='UTF-8')
        logger.info(f"Received event from Partition {partition_context.partition_id}")
        
        # Parse payload as JSON
        data = json.loads(body_str)
        
        # Extract system metadata
        device_id = get_device_id(event.system_properties)
        enqueued_time = event.enqueued_time
        
        # Build unified payload
        payload = {
            "deviceId": device_id,
            "predictions": data,
            "enqueuedTimeUtc": str(enqueued_time)
        }
        
        # Publish to Mosquitto (Unified Namespace)
        payload_json = json.dumps(payload)
        mqtt_client.publish(MQTT_TOPIC, payload_json, qos=1)
        logger.info(f"Forwarded telemetry to Mosquitto [{MQTT_TOPIC}]: {payload_json}")
        
        # Periodically update partition checkpoint (helps resume reading if container restarts)
        partition_context.update_checkpoint(event)
        
    except json.JSONDecodeError as je:
        logger.error(f"Failed to parse event body as JSON: {je}. Raw: {event.body_as_str()}")
    except Exception as e:
        logger.error(f"Error handling event: {e}")

def main():
    if not EVENTHUB_CONN_STR:
        logger.critical("AZURE_EVENTHUB_CONNECTION_STRING is not set. Exiting.")
        return
    if not EVENTHUB_NAME:
        logger.warning("AZURE_EVENTHUB_NAME is not set (it should be defined if not included directly in the connection string).")

    # Connect to Mosquitto broker
    logger.info(f"Connecting to Mosquitto broker at {MQTT_BROKER}...")
    try:
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
        mqtt_client.loop_start()
    except Exception as e:
        logger.critical(f"Could not connect to Mosquitto broker: {e}. Exiting.")
        return

    # Start Event Hub listener
    logger.info("Initializing Azure Event Hub Consumer Client...")
    consumer_client = EventHubConsumerClient.from_connection_string(
        conn_str=EVENTHUB_CONN_STR,
        consumer_group=CONSUMER_GROUP,
        eventhub_name=EVENTHUB_NAME
    )
    
    try:
        with consumer_client:
            logger.info("Listening for events from Azure Event Hub...")
            consumer_client.receive(
                on_event=on_event,
                starting_position="@latest"  # Listen only to new events enqueued after start
            )
    except KeyboardInterrupt:
        logger.info("Keyboard interrupt received. Shutting down...")
    except Exception as e:
        logger.critical(f"Fatal error in Event Hub client: {e}")
    finally:
        logger.info("Stopping MQTT client loop...")
        mqtt_client.loop_stop()
        mqtt_client.disconnect()
        logger.info("Shutdown complete.")

if __name__ == "__main__":
    main()