# 🌍 IncuNest — Open-Source Neonatal Incubator

**IncuNest** (from *in3ator*) is an open-source neonatal incubator designed for hospitals with limited resources.  
Its mission is to reduce neonatal mortality worldwide by providing an accessible, reliable, and locally replicable medical device.

---

## 🩺 Background and Motivation

Every year, over **1.5 million premature babies** die because they don’t have access to an incubator.  
Commercial incubators can cost more than **€35,000**, making them unaffordable in many low-income regions.

**IncuNest** was created as a **low-cost (≈ €350 in components)**, **energy-efficient** alternative, capable of maintaining a newborn’s temperature and humidity even in environments with unstable electricity.  
It can be built locally with standard tools and commonly available materials.

The project is developed and maintained by the NGO [**Medical Open World**](https://medicalopenworld.org/, recipient of the **2025 Princess of Girona Social Award**.  
To date, more than **200 incubators have been installed across 30 countries**, saving thousands of newborn lives.

---

## 🧩 Repository Structure

```
IncuNest/
├── Firmware/          # Firmware source code (ESP32, Arduino framework)
│   ├── Display_HMI/
│   ├── motherboard/
│   └── old            # Legacy
├── Hardware/          # CAD, schematics, and PCB design files
│   ├── Electronics/
│   └── Mechanical/
├── LICENSE            # Open license (non-commercial use allowed)
├── README.md          # This document
└── .gitignore         # Excludes heavy folders (e.g., Hardware/**/History)
```

### 🔐 WiFi and ThingsBoard Configuration

To enable WiFi or ThingsBoard functionality, create or edit the file  
`Firmware/motherBoard/include/Credentials.h` and add your configuration as follows:

```cpp
#ifndef CREDENTIALS
#define CREDENTIALS

#define THINGSBOARD_SERVER "your_url_to_thingsboard"
#define THINGSBOARD_PORT 1883

#define FACTORY_SERVER 0
#define DEMO_SERVER 1

#define THINGSBOARD_PROVISION_SERVER FACTORY_SERVER

#if (THINGSBOARD_PROVISION_SERVER == DEMO_SERVER)
    #define PROVISION_DEVICE_KEY "yourProvisionKey"
    #define PROVISION_DEVICE_SECRET "yourProvisionKeySecret"
#elif (THINGSBOARD_PROVISION_SERVER == FACTORY_SERVER)
    #define PROVISION_DEVICE_KEY "yourProvisionKey"
    #define PROVISION_DEVICE_SECRET "yourProvisionKeySecret"
#endif

#define ssid "yourwifissid"
#define wifiPassword "yourwifipassword"

#endif // CREDENTIALS

```

---

## ⚙️ Setup and Installation

### 🔧 Firmware
1. Install [**PlatformIO**](https://platformio.org/) (recommended inside VSCode).  
2. There are 2 boards to program (the display and the motherboard).
3. Open the `Firmware/XXX` folder.  
4. Connect your board.  
5. Build and upload the firmware:
   ```bash
   pio run --target upload

## 💡 Hardware

All electronic and mechanical designs are available in the `Hardware/` folder,  
including **schematics**, **PCB layouts**, and **3D printable parts**.

---

## 🧠 Key Features

- Automated thermal and humidity control  
- Redundant, low-power energy system  
- Modular firmware with OTA update support  
- Full sensorization (temperature, humidity, door, etc.)  
- Remote telemetry and monitoring capabilities  
- **Open-source design:** free to build, adapt, or improve anywhere  

---

## 🌱 Social Impact

- **200+ incubators** deployed in rural hospitals  
- **30+ countries** reached across Africa, Latin America, Asia, and Eastern Europe  
- **30,000+ hours** of operation logged  
- Built in collaboration with **10 Salesian vocational centers in Spain**  
- Supported by partner NGOs such as **Ayuda Contenedores** for logistics and shipping  

---

## 🤝 How to Contribute

You can help in many ways:

- 🧰 **Technical development:** firmware, electronics, or mechanical design  
- 🌍 **Field partnerships:** connect with hospitals, NGOs, or universities  
- 💶 **Financial support:** cover materials or shipment costs  
- 💬 **Outreach and volunteering:** talks, workshops, mentoring  

📩 **Contact:** [contact@medicalopenworld.org](mailto:contact@medicalopenworld.org)  
🌐 **Website:** [https://medicalopenworld.org](https://medicalopenworld.org)  
📷 **Instagram:** [@medicalopenworld](https://www.instagram.com/medicalopenworld)

---

## 📜 License

This project is distributed under an **Open-Source License for non-commercial use**.  
Commercial use or integration into profit-oriented products requires prior authorization  
from **Medicina Abierta al Mundo**.  
See the [`LICENSE`](LICENSE) file for details.

---

## ✨ Credits

**Lead Developer:** Pablo Sánchez Bergasa  
Industrial Engineer (UPNA) — Master in Electrical Systems (UNED)  
🏅 *Princess of Girona Social Award 2025*  
Founder & Director — NGO *Medical Open World*

**Contributors:**  
Volunteers, partner companies, and educational centers (*Salesianos*, *IED*, *Ayuda Contenedores*, and more).

---

> *"So that the place where a premature baby is born does not limit their chances of survival.”*
