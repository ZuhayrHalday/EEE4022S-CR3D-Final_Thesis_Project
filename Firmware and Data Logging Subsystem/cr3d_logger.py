# ====== cr3d_logger.py ======
import threading, queue, time, json, csv, pathlib, sys, math
from collections import deque
import tkinter as tk
from tkinter import ttk, messagebox

import serial, serial.tools.list_ports
import pandas as pd
import matplotlib
matplotlib.use("TkAgg")
from matplotlib.figure import Figure
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from tzlocal import get_localzone
import requests
import geocoder

APP_TITLE = "DESKTOP MUON LOGGER"

THEME = {
    "bg_dark":        "#0b1117",  # main window background
    "bg_panel_top":   "#000811",  # TOP BAR color
    "bg_panel_side":  "#111925",  # SIDEBAR color
    "fg_main":        "#e6f0ff",
    "fg_dim":         "#a9c1ff",
    "accent":         "#6bb4ff",
    "red":            "#d94b4b",
    "green":          "#3bd16f",
    "btn_start_bg":   "#2a72c7",
    "btn_start_active":"#3885e6",
    "btn_stop_bg":    "#a33a3a",
    "btn_stop_active":"#bf4a4a",
}

CSV_HEADER = [
    "timestamp_local","elapsed_s","type",
    "mv","adc","mv_peak","adc_peak","baseline_adc","dead_us",
    "lat","lon","temp_C","pressure_hPa"
]
LOCAL_TZ = get_localzone()
LOGO_CANDIDATES = ["logo.png","cr3d_logo.png","CR3D_logo.png"]

# ------ Running histogram ------
class RunningHist:
    def __init__(self, bin_width_mv=10.0, max_bins=400):
        self.w = float(bin_width_mv)
        self.max_bins = max_bins
        self.counts = {}
        self.total = 0
    def add(self, mv):
        if mv is None: return
        b = int(max(0, mv // self.w))
        if b >= self.max_bins: b = self.max_bins - 1
        self.counts[b] = self.counts.get(b, 0) + 1
        self.total += 1
    def mode_mpv(self):
        if not self.counts: return None
        b = max(self.counts.items(), key=lambda kv: kv[1])[0]
        return (b + 0.5) * self.w

class CR3DApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title(APP_TITLE)
        self.configure(bg=THEME["bg_dark"])
        try:
            self.after(0, lambda: self.state('zoomed')) 
        except Exception:
            pass
        self.resizable(False, False)

        # Logo / icon
        self.logo_img_small = None
        self._load_logo_images()
        if self.logo_img_small is not None:
            try: self.iconphoto(True, self.logo_img_small)
            except Exception: pass

        # State
        self.ser = None
        self.reader_thread = None
        self.reader_running = False
        self.q = queue.Queue()
        self.logging = False
        self.session_start = None
        self.session_csv = None
        self.t0_pc = None
        self.t0_us = None
        self.el0 = None
        self.lat = None
        self.lon = None
        self.tempC = None
        self.press_hPa = None
        self.hit_flash_until = 0.0

        # Stats (live)
        self.event_times = deque()
        self.session_peak_mv = None
        self.session_total = 0
        self.dead_time_s_total = 0.0
        self.last_event_perf = None
        self.last_peak_mv = None
        self.sample_mv = deque(maxlen=1200)
        self.intervals = deque(maxlen=256)
        self.hist = RunningHist(bin_width_mv=10.0, max_bins=600)
        self.rate_env = deque(maxlen=180)

        # UI
        self._build_styles()
        self._build_topbar()
        self._build_main_area()   
        self._build_stats_content()  
        self._build_controls()    

        # Periodic tasks
        self._refresh_ports()
        self.after(1000, self._port_watchdog)
        self.after(300, self._update_location_weather)
        self.after(50, self._ui_heartbeat)
        self.after(60000, self._env_snapshot_tick)

    # ---------- Logo ----------
    def _load_logo_images(self):
        from pathlib import Path
        png_path = None
        for name in LOGO_CANDIDATES:
            p = Path(name)
            if p.exists():
                png_path = str(p); break
        if not png_path: return
        try:
            from PIL import Image, ImageTk  
            img = Image.open(png_path).convert("RGBA")
            target_h = 100
            w, h = img.size; scale = target_h / h
            img_small = img.resize((max(1,int(w*scale)), target_h), Image.LANCZOS)
            self.logo_img_small = ImageTk.PhotoImage(img_small)
        except Exception:
            try:
                tmp = tk.PhotoImage(file=png_path)
                if tmp.height() > 40:
                    factor = max(1, tmp.height() // 40)
                    tmp = tmp.subsample(factor, factor)
                self.logo_img_small = tmp
            except Exception:
                self.logo_img_small = None

    # ---------- Styles ----------
    def _build_styles(self):
        style = ttk.Style()
        style.theme_use("clam")

        style.configure("TFrame", background=THEME["bg_dark"])

        # TOP BAR styles
        style.configure("TopBar.TFrame", background=THEME["bg_panel_top"])
        style.configure("Top.TLabel",   background=THEME["bg_panel_top"], foreground=THEME["fg_main"], font=("Segoe UI Bold", 12))
        style.configure("TopTitle.TLabel", background=THEME["bg_panel_top"], foreground=THEME["accent"], font=("Bahnschrift Bold SemiCondensed", 35))
        style.configure("TopBtn.TButton", font=("Segoe UI", 10))
        style.map("TopBtn.TButton", background=[("active", "#07101d")], foreground=[("disabled", THEME["fg_dim"])])

        # SIDEBAR styles
        style.configure("Side.TFrame", background=THEME["bg_panel_side"])
        style.configure("Side.TLabel", background=THEME["bg_panel_side"], foreground=THEME["fg_main"], font=("Segoe UI", 12))
        style.configure("SideSm.TLabel", background=THEME["bg_panel_side"], foreground=THEME["fg_main"], font=("Segoe UI", 10))
        style.configure("SideTitle.TLabel", background=THEME["bg_panel_side"], foreground=THEME["fg_dim"], font=("Segoe UI Semibold", 12))

        # Buttons
        style.configure("Start.TButton", font=("Segoe UI Semibold", 11), background=THEME["btn_start_bg"], foreground="white", borderwidth=0, focusthickness=0)
        style.map("Start.TButton", background=[("active", THEME["btn_start_active"]), ("disabled", "#35506e")], foreground=[("disabled", "#c7d3ea")])
        style.configure("Stop.TButton", font=("Segoe UI Semibold", 11), background=THEME["btn_stop_bg"], foreground="white", borderwidth=0, focusthickness=0)
        style.map("Stop.TButton", background=[("active", THEME["btn_stop_active"]), ("disabled", "#6a4747")], foreground=[("disabled", "#e3c9c9")])

        style.configure(
        "SideHeader.TLabel",
        background=THEME["bg_panel_side"],
        foreground=THEME["accent"],         
        font=("Segoe UI Semibold", 16)
)

    # ---------- Top Bar ----------
    def _build_topbar(self):
        self.topbar = ttk.Frame(self, style="TopBar.TFrame")
        self.topbar.pack(fill="x", side="top")

        left = ttk.Frame(self.topbar, style="TopBar.TFrame")
        left.pack(side="left", padx=10, pady=6)
        self.status_lbl = ttk.Label(left, text="Disconnected", style="Top.TLabel")
        self.status_lbl.pack(side="left", padx=(0,8))
        self.port_cmb = ttk.Combobox(left, state="readonly", width=14)
        self.port_cmb.pack(side="left", padx=5)
        self.refresh_btn = ttk.Button(left, text="Refresh Ports", style="TopBtn.TButton", command=self._refresh_ports)
        self.refresh_btn.pack(side="left", padx=6)

        center = ttk.Frame(self.topbar, style="TopBar.TFrame")
        center.pack(side="left", expand=True)
        wrap = ttk.Frame(center, style="TopBar.TFrame")
        wrap.pack(pady=2)
        if self.logo_img_small is not None:
            tk.Label(wrap, image=self.logo_img_small, bg=THEME["bg_panel_top"]).pack(side="left", padx=(0, 12))
        ttk.Label(wrap, text=APP_TITLE, style="TopTitle.TLabel").pack(side="left")

        right = ttk.Frame(self.topbar, style="TopBar.TFrame")
        right.pack(side="right", padx=10, pady=4)
        self.loc_lbl = ttk.Label(right, text="Lat: ---,  Lon: ---", style="Top.TLabel")
        self.loc_lbl.pack(side="left", padx=8)
        self.wx_lbl = ttk.Label(right, text="Temp: -- °C   P: --- hPa", style="Top.TLabel")
        self.wx_lbl.pack(side="left", padx=8)
        self.led = tk.Canvas(right, width=60, height=60, highlightthickness=0, bg=THEME["bg_panel_top"])
        self.led.pack(side="left", padx=10)
        self.led_id = self.led.create_oval(8,8,52,52, fill=THEME["red"], outline="")
        self.led_text = self.led.create_text(30,30, text="IDLE", fill="white", font=("Segoe UI Semibold", 11))

    # ---------- Scroll helpers ----------
    def _bind_wheel(self, widget):
        # Activate global wheel bindings when the mouse is over the canvas
        widget.bind("<Enter>", lambda e: self._wheel_bind_all())
        widget.bind("<Leave>", lambda e: self._wheel_unbind_all())

    def _wheel_bind_all(self):
        # Windows / macOS
        self.bind_all("<MouseWheel>", self._on_mousewheel, add="+")
        # Linux (X11)
        self.bind_all("<Button-4>", self._on_mousewheel_linux_up, add="+")
        self.bind_all("<Button-5>", self._on_mousewheel_linux_down, add="+")

    def _wheel_unbind_all(self):
        self.unbind_all("<MouseWheel>")
        self.unbind_all("<Button-4>")
        self.unbind_all("<Button-5>")

    def _on_mousewheel(self, event):
        
        self.stats_canvas.yview_scroll(-int(event.delta/120)*3, "units")

    def _on_mousewheel_linux_up(self, event):
        self.stats_canvas.yview_scroll(-3, "units")

    def _on_mousewheel_linux_down(self, event):
        self.stats_canvas.yview_scroll(+3, "units")


    # ---------- Main area (plot + sidebar scaffolding) ----------
    def _build_main_area(self):
        # Root container
        container = ttk.Frame(self, style="TFrame")
        container.pack(fill="both", expand=True)

        # ---- Plot area ----
        self.plot_container = ttk.Frame(container, style="TFrame")
        self.plot_container.pack(side="left", fill="both", expand=True)

        # ---- Sidebar shell  ----
        self.sidebar = ttk.Frame(container, style="Side.TFrame", width=320)
        self.sidebar.pack(side="right", fill="y")
        self.sidebar.pack_propagate(False)

        # ---- Scrollable STATS panel  ----
        stats_holder = ttk.Frame(self.sidebar, style="Side.TFrame")
        stats_holder.pack(side="top", fill="both", expand=True)

        
        self.stats_canvas = tk.Canvas(
            stats_holder,
            bg=THEME["bg_panel_side"],
            highlightthickness=0,
            borderwidth=0,
        )
        self.stats_canvas.configure(yscrollincrement=20) 

        self.stats_vsb = tk.Scrollbar(stats_holder, orient="vertical", command=self.stats_canvas.yview)
        self.stats_canvas.configure(yscrollcommand=self.stats_vsb.set)

        self.stats_canvas.pack(side="left", fill="both", expand=True)
        self.stats_vsb.pack(side="right", fill="y")

        # Inner frame
        self.stats_frame = ttk.Frame(self.stats_canvas, style="Side.TFrame")
        self.stats_window = self.stats_canvas.create_window((0, 0), window=self.stats_frame, anchor="nw")

        
        def _on_frame_configure(event=None):
            self.stats_canvas.configure(scrollregion=self.stats_canvas.bbox("all"))
            self.stats_canvas.itemconfigure(self.stats_window, width=self.stats_canvas.winfo_width())

        self.stats_frame.bind("<Configure>", _on_frame_configure)
        self.stats_canvas.bind("<Configure>", _on_frame_configure)

        # Responsive mouse-wheel scrolling
        self._bind_wheel(self.stats_canvas)

        # ---- Fixed ACTIONS panel (footer) ----
        self.actions_frame = ttk.Frame(self.sidebar, style="Side.TFrame")
        self.actions_frame.pack(side="bottom", fill="x", padx=14, pady=14)

        # ---- Plot placeholders ----
        self.canvas = None
        self.ax = None
        self.line = None
        self.xs = []
        self.ys = []


    # ---------- Build stats content inside scrollable panel ----------
    def _build_stats_content(self):
        # Top header
        ttk.Label(self.stats_frame, text="Session statistics", style="SideHeader.TLabel")\
        .pack(anchor="w", padx=14, pady=(14,6))

        # Thin divider under the header
        ttk.Separator(self.stats_frame, orient="horizontal").pack(fill="x", padx=14, pady=(0,10))

        # --- Section: Counting ---
        ttk.Label(self.stats_frame, text="Counting", style="SideTitle.TLabel").pack(anchor="w", padx=14, pady=(16,6))
        self._row(self.stats_frame, "Total count", "total_val")
        self._row(self.stats_frame, "Rate (CPM)", "cpm_val")
        self._row(self.stats_frame, "Poisson 1σ", "cpm_sigma_val")
        self._row(self.stats_frame, "Live rate (CPM)", "live_cpm_val")

        # --- Section: Timing ---
        ttk.Label(self.stats_frame, text="Timing", style="SideTitle.TLabel").pack(anchor="w", padx=14, pady=(12,6))
        self._row(self.stats_frame, "Time since last peak", "since_last_val")
        self._row(self.stats_frame, "⟨Δt⟩ (s)", "mean_dt_val")
        self._row(self.stats_frame, "CV(Δt)", "cv_dt_val")

        # --- Section: Amplitude / Noise ---
        ttk.Label(self.stats_frame, text="Amplitude / Noise", style="SideTitle.TLabel").pack(anchor="w", padx=14, pady=(12,6))
        self._row(self.stats_frame, "Last peak (mV)", "last_peak_val")
        self._row(self.stats_frame, "Session peak (mV)", "peak_val")
        self._row(self.stats_frame, "Noise RMS (mV)", "noise_rms_val")
        self._row(self.stats_frame, "MPV (mV)", "mpv_val")

        # --- Section: Uptime ---
        ttk.Label(self.stats_frame, text="Uptime", style="SideTitle.TLabel").pack(anchor="w", padx=14, pady=(12,6))
        self._row(self.stats_frame, "Run time", "runtime_val")
        self._row(self.stats_frame, "Dead time", "deadtime_val")
        self._row(self.stats_frame, "Dead-time %", "deadfrac_val")

        note = ttk.Label(self.stats_frame,
            text="Stats persist during a session.\nReset when you restart logging\nor relaunch the app.",
            style="SideSm.TLabel")
        note.pack(anchor="w", padx=14, pady=(18,8))

    def _row(self, parent, label, attrname):
        row = ttk.Frame(parent, style="Side.TFrame")
        row.pack(fill="x", padx=14, pady=(4,6))
        ttk.Label(row, text=label, style="SideSm.TLabel").pack(anchor="w")
        lbl = ttk.Label(row, text="--", style="Side.TLabel")
        lbl.config(font=("Segoe UI Semibold", 16))
        lbl.pack(anchor="w", pady=(1,0))
        setattr(self, attrname, lbl)

    # ---------- Controls ----------
    def _build_controls(self):
        self.start_btn = ttk.Button(self.actions_frame, text="Start logging", style="Start.TButton", command=self._start_logging)
        self.start_btn.pack(fill="x", pady=(0, 6))

        self.stop_btn = ttk.Button(self.actions_frame, text="Stop logging", style="Stop.TButton", command=self._stop_logging, state="disabled")
        self.stop_btn.pack(fill="x")

    # ---------- Ports / Connection ----------
    def _refresh_ports(self):
        ports = [p.device for p in serial.tools.list_ports.comports()]
        self.port_cmb["values"] = ports
        if ports:
            cur = self.port_cmb.get()
            self.port_cmb.set(cur if cur in ports else ports[0])
        else:
            self.port_cmb.set("")
    def _is_selected_port_present(self):
        sel = self.port_cmb.get().strip()
        if not sel: return False
        return sel in [p.device for p in serial.tools.list_ports.comports()]
    def _port_watchdog(self):
        connected = (self.ser is not None and self.ser.is_open) or self._is_selected_port_present()
        self.status_lbl.config(text=("Connected" if connected else "Disconnected"),
                               foreground=("#69d18a" if connected else THEME["fg_main"]))
        self._refresh_ports()
        self.after(1000, self._port_watchdog)

    # ---------- Geo + Weather ----------
    def _update_location_weather(self):
        try:
            g = geocoder.ip('me')
            if g.ok and g.latlng:
                self.lat, self.lon = float(g.latlng[0]), float(g.latlng[1])
                self.loc_lbl.config(text=f"Lat: {self.lat:.2f},  Lon: {self.lon:.2f}")
        except Exception:
            pass
        if self.lat is not None and self.lon is not None:
            try:
                url = (f"https://api.open-meteo.com/v1/forecast?"
                       f"latitude={self.lat:.5f}&longitude={self.lon:.5f}"
                       f"&current=temperature_2m,pressure_msl")
                r = requests.get(url, timeout=6)
                if r.ok:
                    cur = r.json().get("current", {})
                    t = cur.get("temperature_2m"); p = cur.get("pressure_msl")
                    if t is not None: self.tempC = float(t)
                    if p is not None: self.press_hPa = float(p)
            except Exception:
                pass
        t_txt = f"{self.tempC:.1f} °C" if self.tempC is not None else "-- °C"
        p_txt = f"{self.press_hPa:.0f} hPa" if self.press_hPa is not None else "--- hPa"
        self.wx_lbl.config(text=f"Temp: {t_txt}   P: {p_txt}")
        self.after(300000, self._update_location_weather)

    # ---------- Start / Stop ----------
    def _start_logging(self):
        port = self.port_cmb.get().strip()
        if not port:
            messagebox.showerror("No port", "No serial port selected.")
            return
        try:
            self.ser = serial.Serial(port, baudrate=115200, timeout=1)
        except serial.SerialException as e:
            messagebox.showerror("Serial error", f"Could not open {port}:\n{e}")
            return

        self.logging = True
        self.session_start = pd.Timestamp.now(tz=LOCAL_TZ)
        stamp = self.session_start.strftime("%Y%m%d_%H%M%S")
        self.session_csv = pathlib.Path(f"CR3D_{stamp}.csv")
        with self.session_csv.open("w", newline="") as f:
            csv.writer(f).writerow(CSV_HEADER)

        self.t0_pc = pd.Timestamp.now(tz=LOCAL_TZ)
        self.t0_us = None
        self.el0 = time.perf_counter()

        # reset stats
        self.event_times.clear()
        self.session_peak_mv = None
        self.session_total = 0
        self.dead_time_s_total = 0.0
        self.last_event_perf = None
        self.last_peak_mv = None
        self.sample_mv.clear()
        self.intervals.clear()
        self.hist = RunningHist(bin_width_mv=10.0, max_bins=600)
        self._update_sidebar(force=True)

        time.sleep(0.25)
        for cmd in (b"SET MODE FIXED\n", b"SET BASELINE_MV 880\n", b"SET THRESHOLD_MV 50\n", b"SET PULSER ON\n"):
            try: self.ser.write(cmd)
            except Exception: pass

        self._create_plot()

        self.reader_running = True
        self.reader_thread = threading.Thread(target=self._reader, daemon=True)
        self.reader_thread.start()

        self.start_btn.configure(state="disabled")
        self.stop_btn.configure(state="normal")

    def _stop_logging(self):
        self.reader_running = False
        self.logging = False
        try:
            if self.ser and self.ser.is_open: self.ser.close()
        except Exception:
            pass
        self.ser = None

        self.start_btn.configure(state="normal")
        self.stop_btn.configure(state="disabled")

        self._destroy_plot()
        self.xs.clear(); self.ys.clear()
        self._update_sidebar(force=True)

    # ---------- Reader ----------
    def _reader(self):
        while self.reader_running and self.ser and self.ser.is_open:
            try:
                line = self.ser.readline().decode(errors="ignore").strip()
                if not line: continue
                try:
                    obj = json.loads(line)
                except Exception:
                    continue
                if (self.t0_us is None) and ("ts_us" in obj):
                    self.t0_us = int(obj["ts_us"])
                self.q.put(obj)
            except serial.SerialException:
                break
            except Exception:
                continue

    # ---------- Plot helpers ----------
    def _create_plot(self):
        if hasattr(self, "canvas") and self.canvas is not None: return
        for ch in self.plot_container.winfo_children(): ch.destroy()
        fig = Figure(dpi=100, facecolor=THEME["bg_panel_side"])
        ax = fig.add_subplot(111)
        ax.set_facecolor(THEME["bg_dark"])
        for spine in ax.spines.values(): spine.set_color(THEME["fg_dim"])
        ax.tick_params(colors=THEME["fg_main"])
        ax.set_xlabel("Elapsed time (s)", color=THEME["fg_main"])
        ax.set_ylabel("Voltage (mV)", color=THEME["fg_main"])
        ax.set_title("A2 live readout", color=THEME["accent"])
        ax.ticklabel_format(axis='x', style='plain'); ax.ticklabel_format(axis='y', style='plain')
        line, = ax.plot([], [], linewidth=1.4)
        canvas = FigureCanvasTkAgg(fig, master=self.plot_container)
        canvas.draw(); canvas.get_tk_widget().pack(fill="both", expand=True)
        self.canvas, self.ax, self.line = canvas, ax, line

    def _destroy_plot(self):
        if hasattr(self, "canvas") and self.canvas is not None:
            self.canvas.get_tk_widget().destroy()
            self.canvas = self.ax = self.line = None

    def _append_plot(self, elapsed_s, mv):
        if not hasattr(self, "xs"): self.xs = []
        if not hasattr(self, "ys"): self.ys = []
        self.xs.append(elapsed_s); self.ys.append(mv)
        if len(self.xs) > 5000:
            self.xs, self.ys = self.xs[-5000:], self.ys[-5000:]

    def _redraw_plot(self):
        if not (hasattr(self, "canvas") and self.canvas and self.ax and self.line): return
        self.line.set_xdata(self.xs); self.line.set_ydata(self.ys)
        self.ax.relim(); self.ax.autoscale_view(); self.canvas.draw()

    # ---------- CSV ----------
    def _log_row(self, row):
        if not self.logging or self.session_csv is None: return
        with self.session_csv.open("a", newline="") as f:
            csv.writer(f).writerow(row)

    # ---------- Hit indicator ----------
    def _set_led_idle(self):
        self.led.itemconfigure(self.led_id, fill=THEME["red"])
        self.led.itemconfigure(self.led_text, text="IDLE")
    def _flash_hit(self):
        self.led.itemconfigure(self.led_id, fill="#3bd16f")
        self.led.itemconfigure(self.led_text, text="HIT")
        self.hit_flash_until = time.perf_counter() + 0.35

    # ---------- Stats helpers ----------
    def _poisson_sigma_cpm(self, n_in_window):
        return math.sqrt(max(0, n_in_window))
    def _noise_rms_mv(self):
        if len(self.sample_mv) < 10: return None
        s = list(self.sample_mv)
        mu = sum(s)/len(s)
        var = sum((x-mu)*(x-mu) for x in s)/len(s)
        return math.sqrt(max(0.0, var))
    def _live_time_s(self):
        if not self.logging or self.el0 is None: return 0.0
        run = time.perf_counter() - self.el0
        live = max(0.0, run - self.dead_time_s_total)
        return live
    def _env_snapshot_tick(self):
        if self.logging:
            cpm_now = len([t for t in self.event_times if (time.perf_counter() - t) <= 60.0])
            self.rate_env.append( (time.time(), cpm_now, self.press_hPa, self.tempC) )
        self.after(60000, self._env_snapshot_tick)

    # ---------- Sidebar update ----------
    def _update_sidebar(self, force=False):
        if not self.logging:
            for key in ("cpm_val","cpm_sigma_val","live_cpm_val","total_val",
                        "since_last_val","mean_dt_val","cv_dt_val",
                        "last_peak_val","peak_val","noise_rms_val","mpv_val",
                        "runtime_val","deadtime_val","deadfrac_val"):
                getattr(self, key).config(text="--")
            return

        now = time.perf_counter()
        while self.event_times and (now - self.event_times[0] > 60.0):
            self.event_times.popleft()
        n = len(self.event_times)
        cpm = n
        sigma = self._poisson_sigma_cpm(n)

        live_s = self._live_time_s()
        live_cpm = (self.session_total / (live_s/60.0)) if live_s > 0 else 0.0

        if len(self.intervals) >= 2:
            mean_dt = sum(self.intervals)/len(self.intervals)
            mu = mean_dt
            var = sum((x-mu)*(x-mu) for x in self.intervals)/(len(self.intervals)-1)
            sd = math.sqrt(max(0.0, var))
            cv = (sd/mu) if mu > 1e-9 else None
        elif len(self.intervals) == 1:
            mean_dt = self.intervals[0]; cv = None
        else:
            mean_dt = None; cv = None

        noise = self._noise_rms_mv()
        mpv = self.hist.mode_mpv()

        since_last = (now - self.last_event_perf) if self.last_event_perf is not None else None

        run_s = time.perf_counter() - self.el0 if self.el0 else 0.0
        dead_s = self.dead_time_s_total
        dead_frac = (100.0 * dead_s/run_s) if run_s > 1e-6 else 0.0

        self.cpm_val.config(text=f"{cpm:d}")
        self.cpm_sigma_val.config(text=(f"±{sigma:.1f}" if sigma is not None else "--"))
        self.live_cpm_val.config(text=(f"{live_cpm:.1f}" if live_cpm > 0 else "--"))
        self.total_val.config(text=f"{self.session_total:d}")
        self.since_last_val.config(text=(f"{since_last:.1f} s" if since_last is not None else "--"))
        self.mean_dt_val.config(text=(f"{mean_dt:.2f}" if mean_dt is not None else "--"))
        self.cv_dt_val.config(text=(f"{cv:.2f}" if cv is not None else "--"))
        self.last_peak_val.config(text=(f"{self.last_peak_mv:.1f}" if self.last_peak_mv is not None else "--"))
        self.peak_val.config(text=("--" if self.session_peak_mv is None else f"{self.session_peak_mv:.1f}"))
        self.noise_rms_val.config(text=(f"{noise:.1f}" if noise is not None else "--"))
        self.mpv_val.config(text=(f"{mpv:.0f}" if mpv is not None else "--"))
        self.runtime_val.config(text=(self._fmt_dhms(run_s)))
        self.deadtime_val.config(text=(self._fmt_dhms(dead_s)))
        self.deadfrac_val.config(text=(f"{dead_frac:.2f} %" if run_s > 0 else "--"))

    def _fmt_dhms(self, s):
        s = int(s)
        d, r = divmod(s, 86400)
        h, r = divmod(r, 3600)
        m, r = divmod(r, 60)
        if d > 0:   return f"{d}d {h}h {m}m {r}s"
        if h > 0:   return f"{h}h {m}m {r}s"
        return f"{m}m {r}s"

    # ---------- UI heartbeat ----------
    def _ui_heartbeat(self):
        try:
            while True:
                obj = self.q.get_nowait()
                self._handle_obj(obj)
        except queue.Empty:
            pass
        if time.perf_counter() > self.hit_flash_until:
            self._set_led_idle()
        if self.logging and hasattr(self, "canvas") and self.canvas is not None:
            self._redraw_plot()
        self._update_sidebar()
        self.after(50, self._ui_heartbeat)

    # ---------- Message handler ----------
    def _handle_obj(self, obj):
        if "ts_us" in obj and self.t0_us is not None and self.t0_pc is not None:
            dt_s = (int(obj["ts_us"]) - int(self.t0_us)) / 1e6
            ts_local = (self.t0_pc + pd.to_timedelta(dt_s, unit="s")).tz_convert(LOCAL_TZ)
            t_iso = ts_local.isoformat()
            elapsed = time.perf_counter() - self.el0 if self.el0 else dt_s
        else:
            ts_local = pd.Timestamp.now(tz=LOCAL_TZ)
            t_iso = ts_local.isoformat()
            elapsed = time.perf_counter() - self.el0 if self.el0 else 0.0

        typ = obj.get("type","")

        if typ == "sample":
            try:
                mv = float(obj.get("mv", 0.0))
            except:
                mv = 0.0
            if not math.isnan(mv) and not math.isinf(mv):
                self.sample_mv.append(mv)
            if self.logging and hasattr(self, "canvas") and self.canvas is not None:
                self._append_plot(elapsed, mv)
            self._log_row([
                t_iso, f"{elapsed:.3f}", "sample",
                obj.get("mv",""), obj.get("adc",""),
                "", "", "", "",
                (f"{self.lat:.6f}" if self.lat is not None else ""),
                (f"{self.lon:.6f}" if self.lon is not None else ""),
                (f"{self.tempC:.2f}" if self.tempC is not None else ""),
                (f"{self.press_hPa:.1f}" if self.press_hPa is not None else "")
            ])

        elif typ == "event":
            self._flash_hit()
            nowp = time.perf_counter()
            if self.last_event_perf is not None:
                self.intervals.append(nowp - self.last_event_perf)
            self.last_event_perf = nowp

            self.event_times.append(nowp)
            self.session_total += 1

            try:
                d_us = float(obj.get("dead_us", 0))
            except:
                d_us = 0.0
            self.dead_time_s_total += max(0.0, d_us)/1e6

            mvp = obj.get("mv_peak", None)
            try:
                mvp_f = float(mvp) if mvp is not None else None
            except:
                mvp_f = None
            if mvp_f is not None:
                self.last_peak_mv = mvp_f
                self.hist.add(mvp_f)
                if self.session_peak_mv is None or mvp_f > self.session_peak_mv:
                    self.session_peak_mv = mvp_f
                if self.logging and self.canvas is not None:
                    self._append_plot(elapsed, mvp_f)

            self._log_row([
                t_iso, f"{elapsed:.6f}", "event",
                "", "", obj.get("mv_peak",""), obj.get("adc_peak",""),
                obj.get("baseline_adc",""), obj.get("dead_us",""),
                (f"{self.lat:.6f}" if self.lat is not None else ""),
                (f"{self.lon:.6f}" if self.lon is not None else ""),
                (f"{self.tempC:.2f}" if self.tempC is not None else ""),
                (f"{self.press_hPa:.1f}" if self.press_hPa is not None else "")
            ])

        elif typ == "hello":
            pass

if __name__ == "__main__":
    try:
        app = CR3DApp()
        app.mainloop()
    except KeyboardInterrupt:
        sys.exit(0)
