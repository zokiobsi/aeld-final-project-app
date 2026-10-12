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

