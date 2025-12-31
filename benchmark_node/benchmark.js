const WebSocket = require('ws');

const URL = 'wss://192.168.1.40:5000'; // update to your ip
const WINDOW = 256;          // mjs on air
const DURATION = 10_000;     // ms

const ws = new WebSocket(URL, {
  rejectUnauthorized: false
});

let seq = 0;
let inFlight = 0;
let sent = 0;
let received = 0;

const rtts = new Map();
const stats = [];

ws.on('open', () => {
  console.log('OPEN');

  const start = Date.now();

  function pump() {
    while (inFlight < WINDOW) {
      const msg = {
        seq,
        t0: process.hrtime.bigint().toString()
      };

      rtts.set(seq, process.hrtime.bigint());
      ws.send(JSON.stringify(msg));

      seq++;
      sent++;
      inFlight++;
    }

    if (Date.now() - start < DURATION) {
      setImmediate(pump);
    } else {
      setTimeout(report, 500);
    }
  }

  pump();
});

ws.on('message', data => {
  const msg = JSON.parse(data);
  const t0 = rtts.get(msg.seq);
  if (!t0) return;

  const t1 = process.hrtime.bigint();
  const rttMs = Number(t1 - t0) / 1e6;

  stats.push(rttMs);
  rtts.delete(msg.seq);

  received++;
  inFlight--;
});

function report() {
  stats.sort((a, b) => a - b);

  const avg = stats.reduce((a, b) => a + b, 0) / stats.length;
  const min = stats[0];
  const max = stats.at(-1);

  let jitter = 0;
  for (let i = 1; i < stats.length; i++) {
    jitter += Math.abs(stats[i] - stats[i - 1]);
  }
  jitter /= stats.length - 1;

  console.log('--- BENCHMARK ---');
  console.log(`TX: ${sent / (DURATION / 1000)} msg/s`);
  console.log(`RX: ${received / (DURATION / 1000)} msg/s`);
  console.log(`RTT avg: ${avg.toFixed(2)} ms`);
  console.log(`RTT min: ${min.toFixed(2)} ms`);
  console.log(`RTT max: ${max.toFixed(2)} ms`);
  console.log(`Jitter: ${jitter.toFixed(2)} ms`);

  process.exit(0);
}
