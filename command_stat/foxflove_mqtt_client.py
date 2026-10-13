import time
import threading
import paho.mqtt.client as mqtt
import foxglove
from foxglove.messages import Log, LogLevel, Timestamp

HEARTBEAT_SLEEP_SEC = 10

class FoxgloveLogger:
    def __init__(self, topic = 'logger', name = 'mqtt-bridge'):
        self.topic = topic
        self.name = name

    def _log(self,text, level):
        foxglove.log(
                self.topic,
                Log(
                    timestamp=Timestamp.from_epoch_secs(time.time()),
                    level=level,
                    message=text,
                    name=self.name,
                ),
                log_time=time.time_ns(),
            )
        
    def info(self, text):
        level = LogLevel.Info
        self._log(text, level)

    def warning(self, text):
        level = LogLevel.Warning
        self._log(text, level)

    def debug(self, text):
        level = LogLevel.Debug
        self._log(text, level)

    def error(self, text):
        level = LogLevel.Error
        self._log(text, level)


def heartbeat():
    while True:
        logger.info("still kickin'")
        time.sleep(HEARTBEAT_SLEEP_SEC)


def on_connect(client, userdata, flags, reason_code, properties):
    logger.info(f"connected to broker: {reason_code}")
    client.subscribe("#")


def on_message(client, userdata, msg):
    try:
        logger.debug(f'Received message {msg.payload} on {msg.topic}')
        value = float(msg.payload.decode())
    except ValueError:
        logger.warning(f"non-numeric payload on {msg.topic}: {msg.payload!r}")
        return
    foxglove.log("/" + msg.topic, {"temp_c": value}, log_time=time.time_ns())


if __name__ == "__main__":
    logger = FoxgloveLogger()
    foxglove.start_server(name="aeld-command-station", host="127.0.0.1", port=8765)
    threading.Thread(target=heartbeat, daemon=True).start()

    mqtt_client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message
    mqtt_client.connect("localhost", 1883)
    mqtt_client.loop_forever()