# PIC32CM JH01 Curiosity Nano – pyX2Cscope Demo

This repository contains small Python utilities and accompanying firmware that
show how to communicate with a Microchip PIC32CM JH01 Curiosity Nano board via
UART using the [pyX2Cscope](https://x2cscope.github.io/pyx2cscope/) library.

The MCU firmware (`main_temp.c`) samples an I²C temperature sensor and exposes
two variables through the X2Cscope interface:

* **`TemperatureValueX2C`** – current temperature in °C.
* **`tempSampleRate`** – enumeration representing the sensor sampling period.

The Python scripts connect to the board, read these variables and present them
in various GUIs.  When `pyx2cscope` is missing the programs fall back to a demo
mode that generates synthetic data so the interfaces remain usable without
hardware.

## Included files

- `temperature_gui.py` – Tkinter application that displays the temperature and
  sampling rate as text and visualises the temperature with a moving red bar.
- `InductiveSensor.py` – Resolver signal monitor for the LX34070A inductive
  sensor (runs with hardware or in demo mode).
- `motorlogger.py` – Logging GUI able to capture motor-control variables.
- `main_temp.c` – Firmware for the PIC32CM JH01 board providing the temperature
  variables over X2Cscope.

## Requirements

- Python 3.11+
- `pyx2cscope` for hardware communication (`pip install pyx2cscope`)
- `pyserial` (installed with `pyx2cscope`)
- Optional dependencies for some demos: `matplotlib`, `pandas`, `scipy`

## Running the temperature demo

1. Build and flash `main_temp.c` to the PIC32CM JH01 Curiosity Nano.
2. Connect the board via USB and note the serial port name.
3. Start the GUI:

   ```bash
   python temperature_gui.py
   ```

4. Select the compiled ELF file and serial port then click **Connect**.
5. The GUI displays `TemperatureValueX2C` and `tempSampleRate` along with a
   thermometer-style red bar that moves with the temperature.

If `pyx2cscope` is not installed the GUI shows simulated values.

## License

This code is provided for demonstration purposes without warranty.
