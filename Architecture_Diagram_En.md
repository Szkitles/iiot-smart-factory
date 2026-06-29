# Smart Factory 4.0 - Architecture Diagram

Below is the comprehensive architecture diagram. Mendix has been restored as the central Business Application (IT layer), while Ignition acts as the factory-floor SCADA (OT layer). The mobile HMI for Ignition has been removed.

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

### Why Mendix is Crucial (The IT/OT Split):
1. **Sales & Workflow Management:** Ignition is great for machine control, but bad for multi-step human workflows (like credit checks, manager approvals). Mendix provides the perfect "Customer/Manager Portal" to process these orders before pushing them to the factory floor.
2. **Maintenance Ticketing (CMMS):** If a machine errors out, Ignition can publish an alarm to MQTT. Mendix picks it up and generates a maintenance ticket, assigns a technician, and tracks the repair workflow.
3. **IT vs OT:** Mendix represents the IT (Information Technology) business layer. Ignition represents the OT (Operational Technology) factory layer. Having both demonstrates an enterprise-grade understanding of Industry 4.0.
