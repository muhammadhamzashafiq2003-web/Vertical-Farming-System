# Vertical-Farming-System
The Vertical Farming System is an ESP32-based smart monitoring setup that tracks soil moisture, water flow, TDS, and temperature/humidity. It provides real-time data every 5 seconds, enabling efficient irrigation, nutrient management, and optimized plant growth for sustainable urban farming.
## Sensors:
1. # Temperature and Humidity sensor:
   The DHT11 sensor measures ambient temperature and humidity for the Vertical Farming System. In this project, VCC connects to 3.3V, GND to GND, and the DATA pin is connected to ESP32 GPIO     22, providing real-time readings to optimize plant growth.
2. # Soil Moisture:
   The soil moisture sensor monitors water content in the growing medium. In this project, VCC connects to 3.3V, GND to GND, and the analog output (AO) connects to ESP32 GPIO 34, enabling       real-time readings to maintain optimal soil hydration for plants.
3. # TDS Sensor:
   The TDS sensor measures the concentration of dissolved nutrients in water to ensure optimal plant growth. In this project, VCC connects to 3.3V, GND to GND, and the analog output (AO)         connects to ESP32 GPIO 35, providing real-time readings for nutrient management.
4. # Water Flow Meter
     The water flow sensor (YF-S201) monitors the volume and rate of water delivered to plants. In this project, VCC connects to 5V, GND to GND, and the pulse output connects to ESP32 GPIO       21, allowing real-time flow monitoring to optimize irrigation.
