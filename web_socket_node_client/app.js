const WebSocket = require('ws');

const serverIp = "192.168.1.40:5000";

const ws = new WebSocket(`wss://${serverIp}/echo/`, {
    rejectUnauthorized: false  // Allow self-signed certificates
});

ws.on('open', () => {
    console.log("Connected to WSS Server");
    ws.send("Hi SS Server");
});

ws.on('message', (msg) => {
    console.log("Server mjs:", msg.toString());
});
