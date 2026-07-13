"""
telemetry.py — Live endpoint telemetry that drives the aquarium.

Polls real system metrics with psutil on a background daemon thread (so the
render loop never blocks) and exposes the latest immutable snapshot. The scene
maps this onto behaviour: CPU -> fish liveliness, network spikes -> sonar
sweeps, a high composite risk score -> the threat state. When live data is
available the aquarium is data-driven, never random.

This is the seam where real Nythos telemetry (threat detections, open ports,
process/service inventory, endpoint health score) would be wired in: replace
`_sample()` with the Nythos agent feed and the visuals follow automatically.
"""
from __future__ import annotations
import threading
import time
from dataclasses import dataclass

try:
    import psutil
except Exception:                      # pragma: no cover
    psutil = None


@dataclass(frozen=True)
class Snapshot:
    cpu: float = 0.0          # 0..100
    mem: float = 0.0          # 0..100
    mem_used_mb: float = 0.0
    mem_total_mb: float = 0.0
    net_kbps: float = 0.0     # recent throughput
    processes: int = 0
    uptime_hours: float = 0.0
    health: float = 100.0     # 0..100 endpoint health score
    risk: float = 8.0         # 0..100 composite risk
    net_spike: bool = False   # transient: a burst of network activity


class Telemetry:
    def __init__(self, settings):
        self.s = settings
        self.snapshot = Snapshot()
        self._stop = threading.Event()
        self._thread = None
        self._last_net = None
        self._net_ema = 0.0

    def start(self):
        if not self.s.use_telemetry or psutil is None:
            return
        try:
            psutil.cpu_percent(None)    # prime the counter
        except Exception:
            return
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def stop(self):
        self._stop.set()

    def _run(self):
        while not self._stop.wait(1.0):
            try:
                self.snapshot = self._sample()
            except Exception:
                pass

    def _sample(self) -> Snapshot:
        cpu = psutil.cpu_percent(None)
        vm = psutil.virtual_memory()
        mem = vm.percent
        mem_used_mb = (vm.total - vm.available) / (1024 * 1024)
        mem_total_mb = vm.total / (1024 * 1024)
        procs = len(psutil.pids())
        try:
            uptime_hours = max(0.0, (time.time() - psutil.boot_time()) / 3600.0)
        except Exception:
            uptime_hours = 0.0

        net = psutil.net_io_counters()
        kbps = 0.0
        spike = False
        if self._last_net is not None:
            sent = net.bytes_sent - self._last_net.bytes_sent
            recv = net.bytes_recv - self._last_net.bytes_recv
            kbps = (sent + recv) / 1024.0
            prev = self._net_ema
            self._net_ema = self._net_ema * 0.7 + kbps * 0.3
            spike = kbps > max(400.0, prev * 3.0)   # sudden burst
        self._last_net = net

        health = max(0.0, 100.0 - cpu * 0.35 - max(0.0, mem - 60) * 0.4)
        proc_load = (procs - 200) * 0.03 if procs > 200 else 0.0
        risk = min(100.0, 6.0 + cpu * 0.25 + max(0.0, mem - 70) * 0.5 + proc_load)

        return Snapshot(cpu=cpu, mem=mem, mem_used_mb=mem_used_mb, mem_total_mb=mem_total_mb,
                        net_kbps=kbps, processes=procs, uptime_hours=uptime_hours,
                        health=health, risk=risk, net_spike=spike)
