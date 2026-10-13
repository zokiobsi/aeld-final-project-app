#!/bin/bash

pi_ip=$1
#Mosquitto broker bridge startup
sudo systemctl stop mosquitto
cat > /tmp/bridge.conf <<EOF
listener 1883
allow_anonymous true

connection broker-bridge
address ${pi_ip}:1883
topic # in 0

start_type automatic
restart_timeout 10
cleansession false
bridge_protocol_version mqttv311
EOF

mosquitto -v -c /tmp/bridge.conf

#Python Foxglove-broker bridge startup
source /home/josh/workspace/aesd_final/.venv/bin/activate
python3 /home/josh/workspace/aesd_final/foxglove_mqtt_client.py