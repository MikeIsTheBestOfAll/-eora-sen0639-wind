(function (root, factory) {
  const api = factory();
  if (typeof module === "object" && module.exports) {
    module.exports = api;
  }
  root.WindProtocol = api;
})(typeof globalThis !== "undefined" ? globalThis : this, function () {
  "use strict";

  const CSV_HEADER = [
    "received_at",
    "protocol",
    "node",
    "seq",
    "avg_mps",
    "min_mps",
    "max_mps",
    "sd_mps",
    "valid_samples",
    "raw",
    "batt_mv",
    "status",
    "rssi_dbm",
    "snr_db",
  ];

  function parseNumber(value, name) {
    const number = Number(value);
    if (!Number.isFinite(number)) {
      throw new Error(`${name} 不是有效数字`);
    }
    return number;
  }

  function parseInteger(value, name) {
    const number = parseNumber(value, name);
    if (!Number.isInteger(number) || number < 0) {
      throw new Error(`${name} 不是有效非负整数`);
    }
    return number;
  }

  function parseWindLine(input) {
    const line = String(input ?? "").trim();
    if (!line.startsWith("WV1,")) {
      return null;
    }

    const fields = line.split(",");
    if (fields.length !== 13) {
      throw new Error(`WV1字段数量错误：期望13项，实际${fields.length}项`);
    }

    const nodeId = fields[1].trim();
    if (!nodeId) {
      throw new Error("节点名称为空");
    }

    return {
      protocol: fields[0],
      nodeId,
      sequence: parseInteger(fields[2], "seq"),
      averageMps: parseNumber(fields[3], "avg"),
      minimumMps: parseNumber(fields[4], "min"),
      maximumMps: parseNumber(fields[5], "max"),
      standardDeviationMps: parseNumber(fields[6], "sd"),
      validSamples: parseInteger(fields[7], "n"),
      averageRaw: parseInteger(fields[8], "raw"),
      batteryMv: parseInteger(fields[9], "batt_mv"),
      status: parseInteger(fields[10], "status"),
      rssiDbm: parseNumber(fields[11], "rssi"),
      snrDb: parseNumber(fields[12], "snr"),
      sourceLine: line,
    };
  }

  function decodeStatus(statusValue) {
    const status = Number(statusValue) >>> 0;
    if (status === 0) {
      return { label: "正常", details: ["数据有效"], severity: "ok" };
    }

    const details = [];
    if (status & 1) details.push("未发现传感器");
    if (status & 2) details.push("没有有效样本");
    if (status & 4) details.push("存在读取或校验错误");
    if (status & 8) details.push("达到15 m/s量程上限");
    const knownMask = 1 | 2 | 4 | 8;
    if (status & ~knownMask) details.push(`未知状态位 ${status & ~knownMask}`);

    return {
      label: details.join("；"),
      details,
      severity: status & (1 | 2) ? "error" : "warning",
    };
  }

  function csvEscape(value) {
    const text = String(value ?? "");
    if (/[",\r\n]/.test(text)) {
      return `"${text.replace(/"/g, '""')}"`;
    }
    return text;
  }

  function packetToCsvRow(record) {
    return [
      record.receivedAt instanceof Date
        ? record.receivedAt.toISOString()
        : String(record.receivedAt ?? ""),
      record.protocol,
      record.nodeId,
      record.sequence,
      record.averageMps,
      record.minimumMps,
      record.maximumMps,
      record.standardDeviationMps,
      record.validSamples,
      record.averageRaw,
      record.batteryMv,
      record.status,
      record.rssiDbm,
      record.snrDb,
    ].map(csvEscape).join(",");
  }

  return {
    CSV_HEADER,
    parseWindLine,
    decodeStatus,
    packetToCsvRow,
  };
});
