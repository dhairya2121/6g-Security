# Security Threat Modeling in 6G

A simulation-based study of security threats in next-generation wireless networks using **NS-3** and **5G-LENA NR**.

The project models a normal wireless network and evaluates the impact of different security attacks, including:

- **Jamming**
- **Spoofing**
- **Denial of Service (DoS)**

The simulations compare normal and attacked scenarios using network performance and security-related metrics.

---

## 📌 Project Overview

### Objective

The objective of this project is to simulate and analyze common security threats in a 6G-oriented network environment and study their effect on network performance.

The project follows the workflow:

```text
Normal Network
      ↓
Attack Simulation
      ↓
Performance Measurement
      ↓
Comparison
      ↓
Security Impact Analysis
```

### Attacks Studied

| Scenario | Description |
|---|---|
| Baseline | Normal network operation without an attack |
| Jamming | Interference-based attack affecting wireless communication |
| Spoofing | Simulation of unauthorized/malicious network behavior |
| DoS | Denial-of-Service attack affecting network availability |

---

## 📊 Metrics

The following metrics are used to compare normal and attacked scenarios:

- **Throughput (Mbps)**
- **Packet Delivery Ratio (PDR)**
- **Packet Loss**
- **End-to-End Delay (ms)**
- **Jitter (ms)**
- **SINR / Signal Quality**, where applicable
- **Network Availability / Attack Impact**

The results are stored in CSV files for further analysis and visualization.

---

# 🛠️ Technology Stack

- **NS-3.48**
- **5G-LENA NR v5.1**
- **C++**
- **Ubuntu 24.04 LTS**
- **CMake**
- **Git**
- **WSL2** (recommended for Windows users)

---

# 📁 Repository Structure

```text
6g-security/
│
├── simulations/
│   ├── 6g-baseline.cc
│   ├── 6g-jamming.cc
│   ├── 6g-spoofing.cc
│   └── 6g-dos.cc
│
├── results/
│   ├── baseline.csv
│   ├── jamming.csv
│   ├── spoofing.csv
│   └── dos.csv
│
├── analysis/
│   └── ...
│
├── docs/
│   └── ...
│
├── screenshots/
│   └── ...
│
├── .gitignore
└── README.md
```

> Some simulation and result files may be added as the project progresses.

---

# 💻 System Requirements

## Recommended Environment

### Linux

- Ubuntu **24.04 LTS**
- Git
- C++ compiler
- CMake
- Python (optional, for analysis/visualization)

### Windows

Windows users can use **WSL2 with Ubuntu 24.04 LTS**.

The project was developed and tested using:

```text
Windows
   ↓
WSL2
   ↓
Ubuntu 24.04 LTS
   ↓
NS-3.48
   ↓
5G-LENA NR v5.1
```

---

# 🚀 Setup

## 1. Clone this repository

```bash
git clone https://github.com/YOUR_USERNAME/6g-security.git
cd 6g-security
```

Replace `YOUR_USERNAME` with the GitHub username that owns this repository.

---

## 2. Install prerequisites

On Ubuntu:

```bash
sudo apt update
sudo apt install -y git g++ cmake ninja-build python3 python3-pip
```

You may also need additional packages required by NS-3 and 5G-LENA depending on your installation.

---

# 3. Install NS-3.48

Create a working directory:

```bash
mkdir -p ~/6g-security
cd ~/6g-security
```

Obtain **NS-3.48** and place it at:

```text
~/6g-security/ns-3
```

Verify the version:

```bash
cd ~/6g-security/ns-3
git describe --tags
```

It should report:

```text
ns-3.48
```

---

# 4. Install 5G-LENA NR v5.1

Install **5G-LENA NR v5.1** according to the official 5G-LENA installation instructions.

The expected development environment is:

```text
~/6g-security/
├── ns-3/
└── nr/
```

Verify that the NR module is available before continuing.

---

# 5. Build NS-3

Go to the NS-3 directory:

```bash
cd ~/6g-security/ns-3
```

Build the project:

```bash
./ns3 build
```

If the build completes successfully, NS-3 is ready.

You can test the installation with:

```bash
./ns3 run hello-simulator
```

---

# 6. Add the project simulations

The simulation source files are located in this repository under:

```text
simulations/
```

Copy them into the NS-3 `scratch` directory:

```bash
cp ~/6g-security/6g-security/simulations/*.cc \
   ~/6g-security/ns-3/scratch/
```

Your NS-3 directory should now contain:

```text
ns-3/
└── scratch/
    ├── 6g-baseline.cc
    ├── 6g-jamming.cc
    ├── 6g-spoofing.cc
    └── 6g-dos.cc
```

---

# ▶️ Running the Simulations

Go to NS-3:

```bash
cd ~/6g-security/ns-3
```

Build:

```bash
./ns3 build
```

## Baseline

Run the normal network scenario:

```bash
./ns3 run 6g-baseline
```

## Jamming

Run the jamming scenario:

```bash
./ns3 run 6g-jamming
```

## Spoofing

Run:

```bash
./ns3 run 6g-spoofing
```

## DoS

Run:

```bash
./ns3 run 6g-dos
```

> The spoofing and DoS commands will become available once their respective simulation files are included in the repository.

---

# 📄 Results

Simulation results are exported as CSV files.

Example:

```text
results/
├── baseline.csv
├── jamming.csv
├── spoofing.csv
└── dos.csv
```

A typical result contains:

```text
scenario,
txPackets,
rxPackets,
lostPackets,
throughputMbps,
delayMs,
jitterMs
```

These files can be imported into Python, Excel, MATLAB, or other data-analysis tools for visualization.

---

# 📈 Analysis

The main objective of the analysis is to compare:

```text
Baseline
   │
   ├── Jamming
   ├── Spoofing
   └── DoS
```

and determine how each attack affects:

- Throughput
- Packet delivery
- Packet loss
- Delay
- Jitter
- Signal quality
- Overall network availability

Graphs and detailed analysis will be maintained in the `analysis/` and `docs/` directories.

---

# 🔬 Reproducibility

For reproducible results, use the same major software versions:

```text
NS-3        : 3.48
5G-LENA NR  : 5.1
Ubuntu      : 24.04 LTS
```

Simulation parameters such as packet count, attack duration, traffic configuration, and network configuration should be kept consistent when comparing different attack scenarios.

---

# ⚠️ Important Notes

This repository contains **research/educational simulations** of network security threats.

The attack implementations are intended for controlled simulation environments and security research. They do not represent attacks against real-world networks.

The repository does **not** contain the complete NS-3 or 5G-LENA source trees. These frameworks must be installed separately.

---

# 📚 References

- NS-3 — Discrete-event network simulator
- 5G-LENA — 5G NR simulation framework for NS-3
- 3GPP specifications and technical documentation related to 5G/6G security

Official installation and documentation links will be maintained here.

---

# 👨‍💻 Project

**Project:** Security Threat Modeling in 6G

**Author:** Vedant Shende, Suraj Shewale, Dhairya Singhal

**Technologies:** C++, NS-3, 5G-LENA, Linux/WSL2

---

## ⭐ Future Work

Planned extensions include:

- Spoofing attack analysis
- DoS attack analysis
- Comparative performance graphs
- Attack severity comparison
- Additional 6G security scenarios
- Automated result analysis
- Visualization of security impact

---

## 📜 License

This project is intended for educational and research purposes.