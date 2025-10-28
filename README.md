# Cosmic Ray Detector and Data Dispatcher (CR3D)
### Undergraduate Thesis Project – EEE4022S 2025  
**Author:** Zuhayr Halday
**Supervisor:** Prof Simon Winberg  
**Institution:** Department of Electrical and Electronic Engineering, University of Cape Town  
**Year:** 2025  

---

## 📖 Project Overview
The **Cosmic Ray Detector and Data Dispatcher (CR3D)** is a compact, educationally oriented **muon detection framework** designed as part of an undergraduate thesis project.  
The system integrates **physics-based simulation**, **analog front-end (AFE) design**, and **modular data acquisition software** to demonstrate a full cosmic-ray detection pipeline — from **muon energy deposition** to **digitized data logging**.

Due to constraints in component availability and time, the detector was developed using a **simulation-centered methodology** that validated all major subsystems analytically and experimentally through proxy testing.

---

## 🧩 System Architecture
CR3D comprises **three interdependent subsystems** reflected in the repo structure:

1. **Scintillator Simulation Subsystem**  
   - Implemented using **Geant4** to model muon interactions with an EJ-200 scintillator.  
   - Produces photon count and timing histograms for varied muon energies, path lengths, and quenching parameters.  
   - Output: `photon_counts.csv` and `time_histograms.csv`

2. **Analog Front-End (AFE) Subsystem**  
   - Designed and simulated in **LTSpice** with analytical modeling in MATLAB.  
   - Includes a **transimpedance amplifier (TIA)**, **peak-hold detector**, and **RC low-pass filter**.  
   - Hardware prototype verified using **controlled current injection** tests.

3. **Firmware and Data Logging Subsystem**  
   - Arduino Nano firmware (C++) for analog sampling, event detection, and serial data transmission.  
   - Python GUI (`cr3d_logger.py`) built with **Tkinter** and **Matplotlib** for real-time plotting, environmental annotation, and structured CSV logging.
