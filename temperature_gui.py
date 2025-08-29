#!/usr/bin/env python3
"""Simple Tkinter GUI to read temperature variables via pyX2Cscope.

Connects to a PIC32CM MCU running ``main_temp.c`` and displays two
variables exposed through the X2Cscope interface:
``TemperatureValueX2C`` and ``tempSampleRate``.  A red bar visualises
the temperature.

Falls back to a demo mode with synthetic data when ``pyX2Cscope`` is not
available so the GUI can run without hardware.
"""

from __future__ import annotations

import math
import time
from dataclasses import dataclass
from typing import Optional

import tkinter as tk
from tkinter import filedialog, messagebox, ttk

try:  # optional dependency
    from pyx2cscope.x2cscope import X2CScope  # type: ignore
except Exception:  # pragma: no cover - pyX2Cscope missing
    X2CScope = None  # type: ignore

import serial.tools.list_ports


# ---------------------------------------------------------------------------
# Demo backend
# ---------------------------------------------------------------------------

@dataclass
class _DemoSource:
    """Provide synthetic data when pyX2Cscope isn't available."""

    temp: float = 25.0
    rate: int = 0

    def __post_init__(self) -> None:
        self.t_last = time.perf_counter()

    def read(self) -> tuple[float, int]:
        now = time.perf_counter()
        dt = now - self.t_last
        self.t_last = now
        self.temp = 25.0 + 5.0 * math.sin(now)
        if dt > 1.5:
            self.rate = (self.rate + 1) % 4
        return self.temp, self.rate


class _ScopeWrapper:
    """Tiny wrapper around pyX2Cscope with demo fallback."""

    def __init__(self) -> None:
        self.demo = X2CScope is None
        self.scope: Optional[X2CScope] = None
        self.var_temp = None
        self.var_rate = None
        self.demo_src = _DemoSource()

    def connect(self, port: str, elf: str) -> None:
        if X2CScope is None:
            self.demo = True
            return
        self.scope = X2CScope(port=port)
        self.scope.import_variables(elf)
        self.var_temp = self.scope.get_variable("TemperatureValueX2C")
        self.var_rate = self.scope.get_variable("tempSampleRate")
        self.demo = False

    def disconnect(self) -> None:
        if self.scope is not None:
            self.scope.disconnect()
        self.scope = None
        self.demo = X2CScope is None

    def read(self) -> tuple[float, int]:
        if self.demo or self.scope is None:
            return self.demo_src.read()
        return (
            float(self.var_temp.get_value()),  # type: ignore[call-arg]
            int(self.var_rate.get_value()),  # type: ignore[call-arg]
        )


# ---------------------------------------------------------------------------
# GUI
# ---------------------------------------------------------------------------

RATE_LABELS = {
    0: "500 ms",
    1: "1 s",
    2: "2 s",
    3: "4 s",
}


class TemperatureGUI:
    DT_MS = 200

    def __init__(self) -> None:
        self.root = tk.Tk()
        self.root.title("PIC32CM Temperature Monitor")

        self._scope = _ScopeWrapper()
        self.connected = False
        self._after_id: Optional[str] = None

        self._build_ui()

    # ------------------------------------------------------------------ UI ---
    def _build_ui(self) -> None:
        main = ttk.Frame(self.root, padding=8)
        main.pack(fill="both", expand=True)

        conn = ttk.LabelFrame(main, text="Connection", padding=8)
        conn.pack(fill="x")

        ttk.Label(conn, text="ELF file:").grid(row=0, column=0, sticky="e")
        self.elf_var = tk.StringVar()
        ttk.Entry(conn, textvariable=self.elf_var, width=40).grid(row=0, column=1, sticky="we", padx=4)
        ttk.Button(conn, text="Browse", command=self._browse).grid(row=0, column=2)

        ttk.Label(conn, text="Port:").grid(row=1, column=0, sticky="e")
        self.port_var = tk.StringVar(value="-")
        self.port_menu = ttk.OptionMenu(conn, self.port_var, "-", *self._ports())
        self.port_menu.grid(row=1, column=1, sticky="we", padx=4)
        ttk.Button(conn, text="Refresh", command=self._refresh_ports).grid(row=1, column=2)

        self.conn_btn = ttk.Button(conn, text="Connect", command=self._toggle_conn)
        self.conn_btn.grid(row=2, column=0, columnspan=3, pady=(6, 0))

        disp = ttk.LabelFrame(main, text="Temperature", padding=8)
        disp.pack(fill="both", expand=True, pady=8)

        self.temp_str = tk.StringVar(value="Temperature: —")
        ttk.Label(disp, textvariable=self.temp_str, font=("Arial", 16)).pack()

        self.rate_str = tk.StringVar(value="Sample rate: —")
        ttk.Label(disp, textvariable=self.rate_str).pack()

        self.canvas = tk.Canvas(disp, width=60, height=200, bg="white")
        self.canvas.pack(pady=8)
        self._draw_thermometer(0.0)

    def _draw_thermometer(self, temp: float) -> None:
        self.canvas.delete("all")
        self.canvas.create_rectangle(20, 10, 40, 190, outline="black", width=2)
        h = max(0.0, min(temp, 125.0)) / 125.0 * 180.0
        self.canvas.create_rectangle(20, 190 - h, 40, 190, fill="red", outline="")

    # ------------------------------------------------------------- utilities --
    @staticmethod
    def _ports() -> list[str]:
        return [p.device for p in serial.tools.list_ports.comports()] or ["-"]

    def _refresh_ports(self) -> None:
        menu = self.port_menu["menu"]
        menu.delete(0, "end")
        for p in self._ports():
            menu.add_command(label=p, command=lambda v=p: self.port_var.set(v))
        self.port_var.set("-")

    def _browse(self) -> None:
        fn = filedialog.askopenfilename(title="Select ELF", filetypes=[("ELF", "*.elf"), ("All", "*.*")])
        if fn:
            self.elf_var.set(fn)

    # -------------------------------------------------------------- connect ---
    def _toggle_conn(self) -> None:
        if self.connected:
            self._disconnect()
        else:
            self._connect()

    def _connect(self) -> None:
        port = self.port_var.get()
        elf = self.elf_var.get()
        if not port or port == "-" or not elf:
            messagebox.showwarning("Missing", "Choose COM port and ELF file")
            return
        try:
            self._scope.connect(port, elf)
        except Exception as e:  # pragma: no cover - hardware errors
            messagebox.showerror("Connect", str(e))
            return
        self.connected = True
        self.conn_btn.config(text="Disconnect")
        self._schedule_update()

    def _disconnect(self) -> None:
        if self._after_id is not None:
            self.root.after_cancel(self._after_id)
            self._after_id = None
        self._scope.disconnect()
        self.connected = False
        self.conn_btn.config(text="Connect")

    # --------------------------------------------------------------- update ---
    def _schedule_update(self) -> None:
        self._after_id = self.root.after(self.DT_MS, self._update)

    def _update(self) -> None:
        temp_c, rate = self._scope.read()
        temp_f = temp_c * 9.0 / 5.0 + 32.0
        self.temp_str.set(f"Temperature: {temp_f:.0f} °F")
        self.rate_str.set(f"Sample rate: {RATE_LABELS.get(rate, rate)}")
        self._draw_thermometer(temp_c)
        if self.connected:
            self._schedule_update()


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> None:
    gui = TemperatureGUI()
    gui.root.mainloop()


if __name__ == "__main__":
    main()
