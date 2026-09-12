OEE Digitalization with IoT

Real-time OEE and TRS monitoring for a production line, built with an ESP32 simulation, MQTT, and a Node-RED dashboard. No physical hardware required.

Overview

This project digitalizes Overall Equipment Effectiveness (OEE) and Taux de Rendement Synthétique (TRS) for a production line. An ESP32 sensor node is simulated in Wokwi, publishes machine status over MQTT, and feeds a Node-RED dashboard that displays live OEE, availability, performance, and quality.

The goal is to replace manual, end-of-shift OEE calculation with a live system that shows production losses as they happen.

Why This Project

In most factories, OEE is calculated by hand at the end of a shift. By the time the number is ready, the losses have already happened. Operators cannot react, only report. This project shows how a lightweight IoT pipeline can make OEE visible in real time, using nothing but free software and a browser.

What It Does

It simulates a motor-control node on an ESP32. The node publishes machine state, cycle counts, and sensor readings over MQTT. A Node-RED flow subscribes to the topic, computes availability, performance, and quality, and displays OEE live on a dashboard.

Architecture

The system has three layers.

The first layer is the ESP32 node, simulated in Wokwi. It represents a machine on a production line. It knows whether it is running, stopped, or in fault, and it counts production cycles.

The second layer is the MQTT broker. It receives messages from the node and forwards them to any subscriber. In this project, the broker runs locally with Mosquitto, using the topic factory/line1/machine.

The third layer is Node-RED. It subscribes to the MQTT topic, applies the OEE logic, and renders a live dashboard.

OEE Calculation

OEE is the product of three factors.

Availability is run time divided by planned production time. It answers the question: how often was the machine actually running.

Performance is ideal cycle time multiplied by total count, divided by run time. It answers the question: how fast did the machine run compared to its ideal speed.

Quality is good count divided by total count. It answers the question: how many parts were produced correctly.

OEE equals availability multiplied by performance multiplied by quality.

Each of these is computed in the Node-RED flow from the simulated machine data.

Tech Stack

Wokwi is used for the ESP32 and circuit simulation. MQTT is used for data transport. Node-RED handles the flow logic and the dashboard. Arduino C++ is used for the ESP32 firmware. Mosquitto is the local MQTT broker.

Repository Contents

The wokwi folder contains the ESP32 firmware (sketch.ino), the circuit definition (diagram.json), and a list of required libraries (libraries.txt).

The node-red folder contains the exported Node-RED flow (flow.json).

The figures folder contains screenshots of the simulation and the dashboard.

The report folder contains a short summary of the project.

The docs folder contains the full academic report.

How to Run It

First, open Wokwi and create a new ESP32 project. Copy the contents of wokwi/sketch.ino into the editor and the contents of wokwi/diagram.json into the diagram tab. Press run. The simulated node will begin publishing messages.

Second, start an MQTT broker. The easiest way is to install Mosquitto locally and run it with default settings. A public broker can also be used for quick testing.

Third, install and run Node-RED. Open the editor in your browser, import the flow from node-red/flow.json, and deploy. The dashboard will be available at the /ui path of your Node-RED instance.

Once all three are running, the dashboard will update live as the simulated machine changes state.

Results

The simulated node publishes machine state, cycle counts, and sensor readings over MQTT. The Node-RED dashboard displays live OEE, availability, performance, and quality. The full pipeline runs end to end, from a simulated sensor to a real-time dashboard, without any physical hardware.

Screenshots of the Wokwi simulation, the Node-RED dashboard, and the OEE output are included in the figures folder.

Limitations

This project uses simulation only. No physical hardware was involved.

The OEE values are based on simulated data, not a real production line.

There is no historical storage. OEE is shown live but not stored over time. Adding a time-series database like InfluxDB would solve this.

The scope is a single machine. A full production line would require multiple nodes and topic coordination.

What I Learned

I learned how to design an end-to-end IoT pipeline, from sensor simulation to dashboard.

I learned how MQTT publish and subscribe works in practice, and how to structure topics.

I learned how to build a Node-RED flow that computes meaningful metrics in real time.

I learned how OEE is calculated and why availability, performance, and quality matter separately.

I learned why real-time visibility matters in manufacturing, and how much is lost when metrics are calculated only at the end of a shift.

Possible Extensions

Replacing the Wokwi simulation with a real ESP32 and real sensors.

Adding InfluxDB or another time-series database to store OEE history.

Extending the system to multiple machines on the same production line.

Adding alerts when OEE drops below a threshold.

Deploying Node-RED on a Raspberry Pi for true edge processing.

Author

Oumaima Yaakoubi
Instrumentation and Intelligent Systems, INSAT, Tunisia
LinkedIn: linkedin.com/in/oumaima-yaakoubi
GitHub: github.com/o-Yaakoubi

License

MIT License. See the LICENSE file for details.

Acknowledgments

This project was developed as part of a self-directed engineering portfolio, focused on instrumentation, embedded systems, and Industry 4.0 applications.