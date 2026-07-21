#include "web.h"
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

// ========== 网页内容（内嵌 HTML + JS + Chart.js）==========
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>OPT101 实时波形</title>
    <style>
        body {
            font-family: 'Segoe UI', sans-serif;
            margin: 20px;
            background: #1e1e2f;
            color: #eee;
        }
        .container {
            max-width: 1200px;
            margin: auto;
            background: #2a2a3a;
            padding: 20px;
            border-radius: 12px;
        }
        h2 { text-align: center; margin-top: 0; }
        .status {
            text-align: center;
            padding: 8px;
            margin-bottom: 20px;
            border-radius: 20px;
            background: #3a3a4a;
            font-weight: bold;
        }
        .connected { color: #4caf50; }
        .disconnected { color: #f44336; }
        canvas { max-height: 500px; width: 100%; background:#222; }
        .info {
            display: flex;
            justify-content: space-between;
            margin-top: 15px;
            font-size: 1.2em;
        }
        .value { font-weight: bold; color: #ffaa66; }
    </style>
</head>
<body>
<div class="container">
    <h2>📈 OPT101 光强实时波形</h2>
    <div class="status" id="statusLabel">⚪ 连接中...</div>
    <canvas id="waveCanvas" width="800" height="400"></canvas>
    <div class="info">
        <span>📡 实时数据点</span>
        <span id="currentValue" class="value">--</span>
        <span>🕒 更新频率: ~20 Hz</span>
    </div>
</div>

<script>
const MAX_POINTS = 200;
const Y_MAX = 4000;//~原3100
let dataBuffer = [];
let ws, ctx, canvasW, canvasH;

// 初始化画布
function initCanvas(){
    const canvas = document.getElementById("waveCanvas");
    ctx = canvas.getContext("2d");
    canvasW = canvas.width;
    canvasH = canvas.height;
}

// 绘制波形
function drawWave(){
    ctx.clearRect(0,0,canvasW,canvasH);
    // 绘制网格
    ctx.strokeStyle = "#444";
    ctx.lineWidth = 1;
    for(let i=0;i<canvasW;i+=40){
        ctx.beginPath();ctx.moveTo(i,0);ctx.lineTo(i,canvasH);ctx.stroke();
    }
    for(let i=0;i<canvasH;i+=40){
        ctx.beginPath();ctx.moveTo(0,i);ctx.lineTo(canvasW,i);ctx.stroke();
    }

    // 绘制曲线
    if(dataBuffer.length < 2) return;
    const stepX = canvasW / MAX_POINTS;
    const scaleY = canvasH / Y_MAX;

    ctx.beginPath();
    ctx.strokeStyle = "#4caf50";
    ctx.lineWidth = 2;
    for(let i=0; i<dataBuffer.length; i++){
        const x = i * stepX;
        const y = canvasH - dataBuffer[i] * scaleY;
        i === 0 ? ctx.moveTo(x,y) : ctx.lineTo(x,y);
    }
    ctx.stroke();

    // 填充下方底色
    ctx.lineTo((dataBuffer.length-1)*stepX, canvasH);
    ctx.lineTo(0, canvasH);
    ctx.closePath();
    ctx.fillStyle = "rgba(76,175,80,0.1)";
    ctx.fill();
}

// 新增数据
function pushData(val){
    dataBuffer.push(val);
    if(dataBuffer.length > MAX_POINTS) dataBuffer.shift();
    drawWave();
    document.getElementById("currentValue").innerText = val.toFixed(3) + " V";
}

// WebSocket连接
function connectWs(){
    ws = new WebSocket(`ws://${window.location.hostname}:81`);
    ws.onopen = ()=>{
        document.getElementById("statusLabel").innerText = "🟢 已连接";
        document.getElementById("statusLabel").className = "status connected";
    };
    ws.onmessage = (e)=>{
        const num = parseFloat(e.data);
        if(!isNaN(num)) pushData(num);
    };
    ws.onclose = ()=>{
        document.getElementById("statusLabel").innerText = "🔴 断开，5秒重连";
        document.getElementById("statusLabel").className = "status disconnected";
        setTimeout(connectWs, 5000);
    };
}

window.onload = ()=>{
    initCanvas();
    connectWs();
}
</script>
</body>
</html>
)rawliteral";

// ========== 全局对象 ==========
WebServer server(80);
WebSocketsServer webSocket(81);

// ========== 网页请求处理 ==========
void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not Found");
}

// ========== WebSocket 事件（可选）==========
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_CONNECTED) {
    Serial.printf("[WebSocket] Client #%u connected\n", num);
  } else if (type == WStype_DISCONNECTED) {
    Serial.printf("[WebSocket] Client #%u disconnected\n", num);
  }
  // 可以在这里处理客户端发来的消息（本例不需要）
}

// ========== 对外接口：初始化 ==========
void webBegin() {
  // 启动 HTTP 服务器
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("[Web] HTTP server started on port 80");

  // 启动 WebSocket 服务器
  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
  Serial.println("[Web] WebSocket server started on port 81");
}

// ========== 对外接口：主循环处理 ==========
void webLoop() {
  server.handleClient();
  webSocket.loop();
}

// ========== 对外接口：推送电压值给所有客户端 ==========
void webPushVoltage(float voltage) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%.3f", voltage);
  webSocket.broadcastTXT(buf);
}