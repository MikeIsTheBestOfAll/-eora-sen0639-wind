(function () {
  "use strict";

  const Protocol = window.WindProtocol;
  const MAX_HISTORY = 1000;
  const TABLE_ROWS = 12;
  const TREND_WINDOW_MS = 10 * 60 * 1000;

  const state = {
    port: null,
    reader: null,
    reading: false,
    buffer: "",
    records: [],
    latest: null,
    previousSequence: null,
    lostPackets: 0,
    lastPacketAt: null,
    packetIntervals: [],
    demoTimer: null,
    demoSequence: 127,
  };

  const element = Object.fromEntries(
    [
      "loraChip", "nodeChip", "lastUpdateChip", "periodChip", "serialState", "serialHint",
      "connectButton", "disconnectButton", "demoButton", "exportButton", "clearButton",
      "helpButton", "messageBar", "dataQualityBadge", "gaugeValue", "windValue",
      "trendCanvas", "chartEmpty", "statusValue", "batteryValue", "rssiValue", "snrValue",
      "rawValue", "sequenceValue", "lostPacketsValue", "statusDetails", "minimumValue",
      "maximumValue", "standardDeviationValue", "sampleCountValue", "recordCount",
      "historyBody", "helpDialog", "closeHelpButton",
    ].map((id) => [id, document.getElementById(id)])
  );

  function formatTime(date) {
    return new Intl.DateTimeFormat("zh-CN", {
      hour: "2-digit", minute: "2-digit", second: "2-digit", hour12: false,
    }).format(date);
  }

  function formatPeriod(ms) {
    if (!Number.isFinite(ms)) return "--";
    if (ms >= 59000) return `约${Math.round(ms / 60000)}分钟/包`;
    return `约${(ms / 1000).toFixed(ms < 10000 ? 1 : 0)}秒/包`;
  }

  function showMessage(message, kind = "info", timeoutMs = 7000) {
    element.messageBar.textContent = message;
    element.messageBar.className = `message-bar ${kind === "info" ? "" : kind}`.trim();
    element.messageBar.hidden = false;
    window.clearTimeout(showMessage.timer);
    if (timeoutMs > 0) {
      showMessage.timer = window.setTimeout(() => { element.messageBar.hidden = true; }, timeoutMs);
    }
  }

  function setSerialUi(mode) {
    const connected = mode === "serial";
    const demo = mode === "demo";
    element.connectButton.disabled = connected || demo;
    element.disconnectButton.disabled = !connected && !demo;
    element.demoButton.textContent = demo ? "停止模拟" : "模拟演示";
    element.serialState.textContent = connected
      ? "地面接收端已连接"
      : demo
        ? "正在使用模拟数据"
        : "地面接收端未连接";
    element.serialHint.textContent = connected
      ? "串口115200波特，正在等待WV1数据包。"
      : demo
        ? "模拟模式不会读取COM口，可用于检查界面。"
        : "请先关闭 PlatformIO 串口监视器，再连接接收板。";
  }

  function median(values) {
    if (!values.length) return NaN;
    const sorted = [...values].sort((a, b) => a - b);
    const middle = Math.floor(sorted.length / 2);
    return sorted.length % 2 ? sorted[middle] : (sorted[middle - 1] + sorted[middle]) / 2;
  }

  function setBadge(node, statusInfo) {
    node.className = `badge badge-${statusInfo.severity}`;
    node.textContent = statusInfo.label;
  }

  function updateGauge(value, statusInfo) {
    const clamped = Math.max(0, Math.min(15, Number(value) || 0));
    element.gaugeValue.style.strokeDashoffset = String(100 - (clamped / 15) * 100);
    element.gaugeValue.style.stroke = statusInfo.severity === "error"
      ? "#c54141"
      : statusInfo.severity === "warning"
        ? "#d18413"
        : "#08a7a3";
    element.windValue.textContent = Number.isFinite(value) && value >= 0 ? value.toFixed(2) : "--";
  }

  function handlePacket(packet, receivedAt = new Date()) {
    const record = { ...packet, receivedAt };
    if (state.lastPacketAt) {
      const interval = receivedAt.getTime() - state.lastPacketAt.getTime();
      if (interval > 100 && interval < 10 * 60 * 1000) {
        state.packetIntervals.push(interval);
        if (state.packetIntervals.length > 8) state.packetIntervals.shift();
      }
    }
    state.lastPacketAt = receivedAt;

    if (state.previousSequence !== null && record.sequence > state.previousSequence + 1) {
      state.lostPackets += record.sequence - state.previousSequence - 1;
    }
    state.previousSequence = record.sequence;
    state.latest = record;
    state.records.unshift(record);
    if (state.records.length > MAX_HISTORY) state.records.length = MAX_HISTORY;

    renderLatest(record);
    renderTable();
    drawTrend();
    element.exportButton.disabled = false;
  }

  function renderLatest(record) {
    const statusInfo = Protocol.decodeStatus(record.status);
    element.nodeChip.textContent = record.nodeId;
    element.lastUpdateChip.textContent = formatTime(record.receivedAt);
    element.periodChip.textContent = formatPeriod(median(state.packetIntervals));
    element.loraChip.className = "chip chip-ok";
    element.loraChip.innerHTML = state.demoTimer
      ? '<span class="status-dot"></span>模拟数据运行中'
      : '<span class="status-dot"></span>LoRa 已收到数据';
    setBadge(element.statusValue, statusInfo);
    setBadge(element.dataQualityBadge, statusInfo);
    element.statusDetails.textContent = statusInfo.details.join("；");
    element.batteryValue.textContent = `${(record.batteryMv / 1000).toFixed(3)} V`;
    element.rssiValue.textContent = `${record.rssiDbm.toFixed(1)} dBm`;
    element.snrValue.textContent = `${record.snrDb.toFixed(1)} dB`;
    element.rawValue.textContent = String(record.averageRaw);
    element.sequenceValue.textContent = `#${record.sequence}`;
    element.lostPacketsValue.textContent = String(state.lostPackets);
    element.minimumValue.textContent = record.minimumMps >= 0 ? record.minimumMps.toFixed(2) : "--";
    element.maximumValue.textContent = record.maximumMps >= 0 ? record.maximumMps.toFixed(2) : "--";
    element.standardDeviationValue.textContent = record.standardDeviationMps >= 0
      ? record.standardDeviationMps.toFixed(2)
      : "--";
    element.sampleCountValue.textContent = String(record.validSamples);
    updateGauge(record.averageMps, statusInfo);
  }

  function renderTable() {
    element.recordCount.textContent = `${state.records.length} 条记录`;
    if (!state.records.length) {
      element.historyBody.innerHTML = '<tr class="empty-row"><td colspan="11">暂无数据</td></tr>';
      return;
    }

    element.historyBody.innerHTML = state.records.slice(0, TABLE_ROWS).map((record) => {
      const status = Protocol.decodeStatus(record.status);
      return `<tr>
        <td>${formatTime(record.receivedAt)}</td>
        <td>${escapeHtml(record.nodeId)}</td>
        <td>#${record.sequence}</td>
        <td>${record.averageMps.toFixed(2)} m/s</td>
        <td>${record.minimumMps.toFixed(2)}</td>
        <td>${record.maximumMps.toFixed(2)}</td>
        <td>${record.standardDeviationMps.toFixed(2)}</td>
        <td>${record.validSamples}</td>
        <td>${record.rssiDbm.toFixed(1)}</td>
        <td>${record.snrDb.toFixed(1)}</td>
        <td class="status-cell-${status.severity}">${escapeHtml(status.label)}</td>
      </tr>`;
    }).join("");
  }

  function escapeHtml(text) {
    return String(text).replace(/[&<>"']/g, (character) => ({
      "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#039;",
    })[character]);
  }

  function drawTrend() {
    const canvas = element.trendCanvas;
    const context = canvas.getContext("2d");
    const bounds = canvas.getBoundingClientRect();
    const scale = window.devicePixelRatio || 1;
    canvas.width = Math.max(1, Math.round(bounds.width * scale));
    canvas.height = Math.max(1, Math.round(bounds.height * scale));
    context.setTransform(scale, 0, 0, scale, 0, 0);
    context.clearRect(0, 0, bounds.width, bounds.height);

    const cutoff = Date.now() - TREND_WINDOW_MS;
    const points = state.records
      .filter((record) => record.receivedAt.getTime() >= cutoff && record.averageMps >= 0)
      .slice()
      .reverse();
    element.chartEmpty.hidden = points.length > 0;
    if (!points.length) return;

    const margin = { top: 20, right: 18, bottom: 36, left: 48 };
    const width = Math.max(20, bounds.width - margin.left - margin.right);
    const height = Math.max(20, bounds.height - margin.top - margin.bottom);
    const values = points.map((point) => point.averageMps);
    let minY = Math.max(0, Math.min(...values) - 0.4);
    let maxY = Math.min(15, Math.max(...values) + 0.4);
    if (maxY - minY < 1) {
      const center = (minY + maxY) / 2;
      minY = Math.max(0, center - 0.5);
      maxY = Math.min(15, center + 0.5);
    }
    const firstTime = points[0].receivedAt.getTime();
    const lastTime = points[points.length - 1].receivedAt.getTime();
    const timeSpan = Math.max(1000, lastTime - firstTime);
    const x = (time) => margin.left + ((time - firstTime) / timeSpan) * width;
    const y = (value) => margin.top + (1 - (value - minY) / (maxY - minY)) * height;

    context.lineWidth = 1;
    context.strokeStyle = "#dbe5ed";
    context.fillStyle = "#65788b";
    context.font = '12px "Segoe UI", "Microsoft YaHei UI", sans-serif';
    context.textBaseline = "middle";
    for (let i = 0; i <= 5; i += 1) {
      const value = minY + ((maxY - minY) * i) / 5;
      const py = y(value);
      context.beginPath();
      context.moveTo(margin.left, py);
      context.lineTo(margin.left + width, py);
      context.stroke();
      context.textAlign = "right";
      context.fillText(value.toFixed(1), margin.left - 8, py);
    }

    for (let i = 0; i <= 5; i += 1) {
      const time = firstTime + (timeSpan * i) / 5;
      const px = x(time);
      context.beginPath();
      context.moveTo(px, margin.top);
      context.lineTo(px, margin.top + height);
      context.stroke();
      context.textAlign = i === 0 ? "left" : i === 5 ? "right" : "center";
      const label = timeSpan < 120000
        ? formatTime(new Date(time))
        : formatTime(new Date(time)).slice(0, 5);
      context.fillText(label, px, margin.top + height + 21);
    }

    const gradient = context.createLinearGradient(0, margin.top, 0, margin.top + height);
    gradient.addColorStop(0, "rgba(8, 167, 163, 0.24)");
    gradient.addColorStop(1, "rgba(8, 167, 163, 0.01)");
    context.beginPath();
    points.forEach((point, index) => {
      const px = x(point.receivedAt.getTime());
      const py = y(point.averageMps);
      if (index === 0) context.moveTo(px, py);
      else context.lineTo(px, py);
    });
    context.lineTo(x(lastTime), margin.top + height);
    context.lineTo(x(firstTime), margin.top + height);
    context.closePath();
    context.fillStyle = gradient;
    context.fill();

    context.beginPath();
    points.forEach((point, index) => {
      const px = x(point.receivedAt.getTime());
      const py = y(point.averageMps);
      if (index === 0) context.moveTo(px, py);
      else context.lineTo(px, py);
    });
    context.strokeStyle = "#08a7a3";
    context.lineWidth = 3;
    context.lineJoin = "round";
    context.lineCap = "round";
    context.stroke();

    const last = points[points.length - 1];
    context.beginPath();
    context.arc(x(last.receivedAt.getTime()), y(last.averageMps), 4.5, 0, Math.PI * 2);
    context.fillStyle = "#0e72c5";
    context.fill();
  }

  function processSerialLine(line) {
    try {
      const packet = Protocol.parseWindLine(line);
      if (packet) handlePacket(packet);
    } catch (error) {
      showMessage(`收到一行无法解析的数据：${error.message}`, "warning");
    }
  }

  async function readSerialLoop() {
    const decoder = new TextDecoder();
    state.reading = true;
    try {
      while (state.port && state.port.readable && state.reading) {
        state.reader = state.port.readable.getReader();
        try {
          while (state.reading) {
            const { value, done } = await state.reader.read();
            if (done) break;
            state.buffer += decoder.decode(value, { stream: true });
            const lines = state.buffer.split(/\r?\n/);
            state.buffer = lines.pop() || "";
            lines.forEach(processSerialLine);
          }
        } finally {
          state.reader.releaseLock();
          state.reader = null;
        }
      }
    } catch (error) {
      if (state.reading) {
        showMessage(`串口读取中断：${error.message}`, "error", 0);
      }
    } finally {
      if (state.port && state.reading) await disconnectSerial(false);
    }
  }

  async function openPort(port) {
    stopDemo();
    state.port = port;
    try {
      await state.port.open({ baudRate: 115200, bufferSize: 4096 });
      setSerialUi("serial");
      element.loraChip.className = "chip chip-muted";
      element.loraChip.innerHTML = '<span class="status-dot"></span>LoRa 等待数据';
      showMessage("地面接收端已连接。现在可以打开空中发送端电源。", "info");
      readSerialLoop();
    } catch (error) {
      state.port = null;
      setSerialUi("disconnected");
      const occupied = /open|busy|access|network/i.test(error.message);
      showMessage(
        occupied
          ? "无法打开串口。请关闭PlatformIO串口监视器和其他占用该COM口的软件，然后重试。"
          : `连接串口失败：${error.message}`,
        "error",
        0
      );
    }
  }

  async function connectSerial() {
    if (!("serial" in navigator)) {
      showMessage("当前浏览器不支持网页串口。请使用Microsoft Edge或Google Chrome，并通过start_dashboard.bat启动。", "error", 0);
      return;
    }
    try {
      const authorized = await navigator.serial.getPorts();
      const port = authorized.length === 1 ? authorized[0] : await navigator.serial.requestPort();
      await openPort(port);
    } catch (error) {
      if (error.name !== "NotFoundError") showMessage(`选择串口失败：${error.message}`, "error", 0);
    }
  }

  async function disconnectSerial(showNotice = true) {
    state.reading = false;
    if (state.reader) {
      try { await state.reader.cancel(); } catch (_) { /* already closed */ }
    }
    if (state.port) {
      try { await state.port.close(); } catch (_) { /* already closed */ }
    }
    state.reader = null;
    state.port = null;
    state.buffer = "";
    setSerialUi("disconnected");
    if (showNotice) showMessage("已断开地面接收端。", "info");
  }

  function makeDemoPacket() {
    const phase = state.demoSequence / 4;
    const average = 6.3 + Math.sin(phase) * 0.34 + (Math.random() - 0.5) * 0.12;
    const spread = 0.22 + Math.random() * 0.16;
    state.demoSequence += 1;
    return {
      protocol: "WV1",
      nodeId: "KITE01",
      sequence: state.demoSequence,
      averageMps: average,
      minimumMps: Math.max(0, average - spread),
      maximumMps: Math.min(15, average + spread * 1.2),
      standardDeviationMps: 0.15 + Math.random() * 0.1,
      validSamples: 10,
      averageRaw: Math.round(2420 + average * 10),
      batteryMv: 4360 - (state.demoSequence % 18),
      status: 0,
      rssiDbm: -61 - Math.random() * 4,
      snrDb: 8.7 + Math.random() * 1.4,
    };
  }

  async function startDemo() {
    if (state.port) await disconnectSerial(false);
    if (state.demoTimer) return;
    setSerialUi("demo");
    state.demoTimer = window.setInterval(() => handlePacket(makeDemoPacket()), 2000);
    const now = Date.now();
    for (let index = 10; index >= 0; index -= 1) {
      handlePacket(makeDemoPacket(), new Date(now - index * 2000));
    }
    showMessage("已启动模拟数据。连接实物前请先停止模拟。", "info");
  }

  function stopDemo() {
    if (!state.demoTimer) return;
    window.clearInterval(state.demoTimer);
    state.demoTimer = null;
    setSerialUi("disconnected");
  }

  function clearData() {
    state.records = [];
    state.latest = null;
    state.previousSequence = null;
    state.lostPackets = 0;
    state.lastPacketAt = null;
    state.packetIntervals = [];
    element.nodeChip.textContent = "--";
    element.lastUpdateChip.textContent = "--:--:--";
    element.periodChip.textContent = "--";
    element.windValue.textContent = "--";
    element.gaugeValue.style.strokeDashoffset = "100";
    element.minimumValue.textContent = "--";
    element.maximumValue.textContent = "--";
    element.standardDeviationValue.textContent = "--";
    element.sampleCountValue.textContent = "--";
    element.batteryValue.textContent = "-- V";
    element.rssiValue.textContent = "-- dBm";
    element.snrValue.textContent = "-- dB";
    element.rawValue.textContent = "--";
    element.sequenceValue.textContent = "#--";
    element.lostPacketsValue.textContent = "0";
    element.statusDetails.textContent = "尚未收到风速数据。";
    setBadge(element.statusValue, { label: "等待数据", severity: "muted" });
    setBadge(element.dataQualityBadge, { label: "等待数据", severity: "muted" });
    element.exportButton.disabled = true;
    renderTable();
    drawTrend();
  }

  function exportCsv() {
    if (!state.records.length) return;
    const lines = [Protocol.CSV_HEADER.join(",")];
    state.records.slice().reverse().forEach((record) => lines.push(Protocol.packetToCsvRow(record)));
    const blob = new Blob(["\uFEFF" + lines.join("\r\n")], { type: "text/csv;charset=utf-8" });
    const url = URL.createObjectURL(blob);
    const anchor = document.createElement("a");
    const stamp = new Date().toISOString().replace(/[-:]/g, "").replace(/T/, "_").slice(0, 15);
    anchor.href = url;
    anchor.download = `wind_data_${stamp}.csv`;
    anchor.click();
    URL.revokeObjectURL(url);
  }

  function updateFreshness() {
    if (!state.lastPacketAt) return;
    const period = median(state.packetIntervals);
    const timeout = Number.isFinite(period) ? Math.max(6000, period * 3) : 75000;
    const stale = Date.now() - state.lastPacketAt.getTime() > timeout;
    if (stale && !state.demoTimer) {
      element.loraChip.className = "chip chip-warning";
      element.loraChip.innerHTML = '<span class="status-dot"></span>LoRa 数据超时';
    }
  }

  async function tryAutoConnect() {
    if (!("serial" in navigator) || new URLSearchParams(location.search).has("demo")) return;
    try {
      const ports = await navigator.serial.getPorts();
      if (ports.length === 1) await openPort(ports[0]);
    } catch (_) {
      // The user can still connect with the button.
    }
  }

  element.connectButton.addEventListener("click", connectSerial);
  element.disconnectButton.addEventListener("click", async () => {
    if (state.demoTimer) stopDemo();
    else await disconnectSerial();
  });
  element.demoButton.addEventListener("click", () => state.demoTimer ? stopDemo() : void startDemo());
  element.exportButton.addEventListener("click", exportCsv);
  element.clearButton.addEventListener("click", clearData);
  element.helpButton.addEventListener("click", () => element.helpDialog.showModal());
  element.closeHelpButton.addEventListener("click", () => element.helpDialog.close());
  element.helpDialog.addEventListener("click", (event) => {
    if (event.target === element.helpDialog) element.helpDialog.close();
  });
  window.addEventListener("resize", drawTrend);
  window.addEventListener("beforeunload", () => {
    state.reading = false;
    if (state.reader) state.reader.cancel().catch(() => {});
  });
  if ("serial" in navigator) {
    navigator.serial.addEventListener("disconnect", () => disconnectSerial(false));
  }

  setSerialUi("disconnected");
  clearData();
  window.setInterval(updateFreshness, 1000);
  if (new URLSearchParams(location.search).has("demo")) void startDemo();
  else tryAutoConnect();
})();
