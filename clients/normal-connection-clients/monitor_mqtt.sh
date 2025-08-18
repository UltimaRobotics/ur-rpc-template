#!/bin/bash

# MQTT Monitor Script
# This script monitors all client communication on the MQTT broker

echo "MQTT Traffic Monitor"
echo "==================="
echo "Monitoring all client traffic on localhost:1883"
echo "Press Ctrl+C to stop"
echo ""

# Monitor all client topics
mosquitto_sub -h localhost -p 1883 -t "clients/+/+" -v