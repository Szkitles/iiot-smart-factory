# Smart Factory 4.0 - Architecture Diagram

Below is the comprehensive architecture diagram. Mendix acts as the business application (IT layer), Ignition acts as the local SCADA (OT layer), and we have integrated a new secure **Edge-Cloud OT-IT Bridge** featuring a physical ESP32 (running TinyML) and Microsoft Azure.

---

## System Architecture Diagram

```mermaid
graph TD
    %% Style Definitions
    classDef broker fill:#ff9e4a,stroke:#333,stroke-width:2px,color:black;
    classDef plc fill:#4db8ff,stroke:#333,stroke-width:2px,color:black;
    classDef scada fill:#ff7eb6,stroke:#333,stroke-width:2px,color:black;
    classDef twin fill:#85e085,stroke:#333,stroke-width:2px,color:black;
    classDef db fill:#d9d9d9,stroke:#333,stroke-width:2px,color:black;
    classDef app fill:#c299ff,stroke:#333,stroke-width:2px,color:black;
    classDef cicd fill:#ffdb58,stroke:#333,stroke-width:2px,color:black;
    classDef azure fill:#0078d4,stroke:#333,stroke-width:2px,color:white;
    classDef python fill:#ffd43b,stroke:#333,stroke-width:2px,color:black;
    classDef edge fill:#e06666,stroke:#333,stroke-width:2px,color:white;

    %% CI/CD Section
    subgraph "CI/CD PIPELINE (e.g., GitHub Actions)"
        CI["Automated Build & Deploy<br>Docker Images & Unity WebGL"]:::cicd
    end

    %% Unified Namespace Hub
    subgraph "00. UNIFIED NAMESPACE (HUB)"
        Mosquitto["Mosquitto MQTT Broker<br>(Docker)"]:::broker
    end

    %% Edge Layer (OT)
    subgraph "01. EDGE & DATA SOURCES"
        Codesys1["Virtual PLC Codesys (Line 1)<br>(Docker)"]:::plc
        CodesysN["Virtual PLC Codesys (Line N)<br>(Docker)"]:::plc
    end

    %% Edge AI Node (Physical)
    subgraph "01a. EDGE AI MONITORING"
        ESP32["ESP32 Microcontroller (TinyML)<br>Vibration Anomaly Detection"]:::edge
    end

    %% Azure Cloud IoT Layer
    subgraph "02a. MICROSOFT AZURE CLOUD"
        IoTHub["Azure IoT Hub<br>(MQTT + SAS Authentication)"]:::azure
        EventHubs["Event Hubs Endpoint<br>(Built-in Message Routing)"]:::azure
    end

    %% Local Bridge Container
    subgraph "02b. SECURE OT-IT DATA BRIDGE"
        PythonBridge["Python Microservice Container<br>(azure-eventhub & paho-mqtt)"]:::python
    end

    %% Database Layer
    subgraph "02. DATABASE LAYER"
        PostgreSQL[("Local PostgreSQL Database<br>📦 DOCKER CONTAINER<br>(Historian & Reports)")]:::db
        CloudDB[("Cloud Database<br>☁️ HOSTED IN CLOUD<br>(Orders & ERP)")]:::db
    end

    %% SCADA Layer (OT)
    subgraph "03. SCADA & IIoT"
        Ignition["Ignition SCADA & Gateway<br>(Docker)"]:::scada
        Perspective["Ignition Perspective<br>(Operator HMI / Dashboard)"]:::scada
    end

    %% Business Application Layer (IT)
    subgraph "04. BUSINESS APPLICATIONS"
        Mendix["Mendix Workflow App<br>(Mendix Cloud)"]:::app
    end

    %% Digital Twin Layer
    subgraph "05. 3D VISUALIZATION"
        Unity["Digital Twin: Unity RealVirtual (WebGL)<br>Served via Web Server (e.g., Nginx)"]:::twin
    end

    %% Data Flow & Connections
    Codesys1 ---|"MQTT (TCP:1883)<br>Pub Telemetry / Sub Commands"| Mosquitto
    CodesysN ---|"MQTT (TCP:1883)<br>Pub Telemetry / Sub Commands"| Mosquitto

    %% Edge AI to Azure Flow
    ESP32 --->|"MQTT / TLS (TCP:8883)<br>Pub Predictions (SAS Token)"| IoTHub
    IoTHub --->|"Internal Route"| EventHubs
    EventHubs ===|"AMQP / TLS (TCP:5671)<br>Read Stream (Azure SDK)"| PythonBridge
    PythonBridge --->|"MQTT (TCP:1883)<br>Pub Anomaly Telemetry"| Mosquitto

    Ignition ---|"MQTT (TCP:1883)<br>Sub Telemetry / Pub Commands"| Mosquitto
    
    %% Orders Flow
    Mendix ---|"Read/Write Orders"| CloudDB
    Mendix ---|"Publish Approved Orders<br>MQTT (TCP/TLS)"| Mosquitto
    Ignition -.-|"Sync Completed Orders to Cloud<br>(When Internet is UP)"| CloudDB
    
    %% Database connections
    Ignition ---|"Write History (Tag Historian)<br>Write Flat Reports (SQL Bridge)"| PostgreSQL
    Mendix ---|"Read Flat Reports Only<br>(Requires VPN / Tunnel)"| PostgreSQL

    Ignition ---|"Serve Operator Screens"| Perspective
    
    Mosquitto ---|"MQTT (WSS:9001)<br>Secure Live Data (WebSockets)"| Unity
    Perspective -.-|"Embeds IFrame"| Unity
    Mendix -.-|"Embeds IFrame"| Unity
```

---

## Key Technical Highlights & Architecture Advantages

### 1. Hybrid Edge-Cloud TinyML Processing
Instead of streaming high-frequency, raw accelerometer vibration data to the cloud (which degrades bandwidth and increases latency/costs), we process the data at the **Edge**. 
- The physical **ESP32 microcontroller** executes a compiled **TinyML classification model** locally.
- Only the processed inference results (predictions, anomaly status, and confidence levels) are packaged as lightweight JSON and sent to Azure.

### 2. High-Grade Security & Isolation of the OT Layer (Zero Trust)
Industrial safety depends on strict network segmentation. Exposing a SCADA gateway (`ignition-gateway`) directly to the internet is a severe vulnerability. 
- **The OT Layer is fully isolated:** The Ignition SCADA and local PLCs operate entirely within the internal, secure Docker bridge network. Ignition has no direct internet access and only communicates with the local Mosquitto broker.
- **Python Security Shield:** A dedicated, custom **Python microservice** acts as the secure OT-IT gateway. It alone handles the external AMQP/TLS handshake and SAS Token credentials with Microsoft Azure. It consumes data from the cloud and forwards it internally, preventing any external entity from connecting directly to the factory network or the SCADA database.

### 3. Containerized Data Transformation Pipeline
The Python bridge is completely containerized within `docker-compose.yml`. This microservice-oriented design offers:
- **Isolation & Portability:** Running the Python consumer script inside a Docker container ensures dependencies (like `azure-eventhub` and `paho-mqtt`) are completely self-contained.
- **Resiliency & Auto-Healing:** Configured with `restart: unless-stopped` in Docker Compose, the bridge automatically recovers and reconnects if the network or Azure connection drops.

### 4. Enterprise-Grade Cloud Ingestion (Microsoft Azure)
The project showcases integration with industry-standard cloud frameworks:
- **Azure IoT Hub:** Serves as the secure ingestion gateway for physical embedded hardware.
- **Built-in Event Routing:** Azure Routes direct payloads to an Event Hubs endpoint, proving knowledge of scalable cloud architecture.

---

### Why Mendix is Crucial (The IT/OT Split):
1. **Sales & Workflow Management:** Ignition is great for machine control, but bad for multi-step human workflows (like credit checks, manager approvals). Mendix provides the perfect "Customer/Manager Portal" to process these orders before pushing them to the factory floor.
2. **Maintenance Ticketing (CMMS):** If a machine errors out, Ignition can publish an alarm to MQTT. Mendix picks it up and generates a maintenance ticket, assigns a technician, and tracks the repair workflow.
3. **IT vs OT:** Mendix represents the IT (Information Technology) business layer. Ignition represents the OT (Operational Technology) factory layer. Having both demonstrates an enterprise-grade understanding of Industry 4.0.
