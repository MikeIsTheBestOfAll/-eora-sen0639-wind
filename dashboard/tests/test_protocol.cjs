"use strict";

const assert = require("node:assert/strict");
const protocol = require("../protocol.js");

const packet = protocol.parseWindLine(
  "WV1,KITE01,128,6.420,6.100,6.830,0.210,10,2475,4360,0,-62.0,9.5"
);

assert.equal(packet.nodeId, "KITE01");
assert.equal(packet.sequence, 128);
assert.equal(packet.averageMps, 6.42);
assert.equal(packet.validSamples, 10);
assert.equal(packet.batteryMv, 4360);
assert.equal(packet.rssiDbm, -62);
assert.equal(protocol.parseWindLine("RADIO_START_RX=0"), null);
assert.deepEqual(protocol.decodeStatus(0), {
  label: "正常",
  details: ["数据有效"],
  severity: "ok",
});
assert.equal(protocol.decodeStatus(5).severity, "error");
assert.match(protocol.decodeStatus(12).label, /量程上限/);
assert.throws(
  () => protocol.parseWindLine("WV1,KITE01,1,2,3"),
  /字段数量错误/
);

const csv = protocol.packetToCsvRow({
  ...packet,
  receivedAt: new Date("2026-11-10T08:00:00.000Z"),
});
assert.match(csv, /^2026-11-10T08:00:00.000Z,WV1,KITE01,128,/);

console.log("protocol tests passed");
