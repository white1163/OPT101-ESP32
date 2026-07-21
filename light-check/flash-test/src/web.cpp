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
        .controls {
            margin-top: 20px;
            display: flex;
            gap: 12px;
            align-items: center;
            flex-wrap: wrap;
        }
        .controls button {
            padding: 10px 20px;
            border: none;
            border-radius: 6px;
            font-size: 16px;
            cursor: pointer;
            color: #fff;
            transition: 0.2s;
        }
        .controls button:hover { opacity: 0.8; }
        .btn-start { background: #4caf50; }
        .btn-stop { background: #f44336; }
        .btn-save { background: #2196f3; }
        .btn-csv { background: #ff9800; }
        .rec-status { font-weight: bold; margin-left: 10px; }
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

    <!-- 新增控制区域 -->
    <div class="controls">
        <button id="startRecBtn" class="btn-start">▶ 开始录制</button>
        <button id="stopRecBtn" class="btn-stop" style="display:none;">⏹ 停止录制</button>
        <button id="saveImgBtn" class="btn-save">💾 保存截图</button>
        <button id="exportCsvBtn" class="btn-csv">📊 导出CSV</button>
        <span id="recStatus" class="rec-status" style="color:#aaa;">空闲</span>
    </div>
</div>

<script>
const MAX_POINTS = 200;
const Y_MAX = 4000;
const Y_TICKS = [0, 1000, 2000, 3000, 4000];
const Y_LABEL_WIDTH = 40;
let dataBuffer = [];
let ws, ctx, canvasW, canvasH;

// ========== 录制全局变量 ==========
let isRecording = false;
let recDataList = [];
let recStartTime = 0;
let startRecBtn, stopRecBtn, saveImgBtn, exportCsvBtn, recStatusText;

// 初始化画布
function initCanvas(){
    const canvas = document.getElementById("waveCanvas");
    ctx = canvas.getContext("2d");
    canvasW = canvas.width;
    canvasH = canvas.height;
    ctx.font = "12px Arial";
    ctx.fillStyle = "#ffffff";
}

// 绑定按钮事件
function bindButtonEvent(){
    startRecBtn = document.getElementById("startRecBtn");
    stopRecBtn = document.getElementById("stopRecBtn");
    saveImgBtn = document.getElementById("saveImgBtn");
    exportCsvBtn = document.getElementById("exportCsvBtn");
    recStatusText = document.getElementById("recStatus");

    // 开始录制
    startRecBtn.onclick = ()=>{
        if (isRecording) return;
        isRecording = true;
        recDataList = [];
        recStartTime = Date.now();
        startRecBtn.style.display = "none";
        stopRecBtn.style.display = "block";
        recStatusText.innerText = "🔴 正在录制...";
        recStatusText.style.color = "#f44336";
    };

    // 停止录制 → 生成HTML报告（含波形截图+数据）
    stopRecBtn.onclick = ()=>{
        if (!isRecording) return;
        isRecording = false;
        startRecBtn.style.display = "block";
        stopRecBtn.style.display = "none";
        recStatusText.innerText = "⏳ 生成报告...";
        recStatusText.style.color = "#ffaa66";

        // 生成CSV文本
        const csvText = generateCSVText();
        // 波形截图
        const canvas = document.getElementById("waveCanvas");
        const imageData = canvas.toDataURL("image/png");
        // 转义CSV中的HTML特殊字符
        const escapedCSV = csvText.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
        // 构建HTML报告
        const htmlContent = `<!DOCTYPE html>
<html>
<head><meta charset="UTF-8"><title>OPT101 录制报告</title>
<style>
body{font-family:sans-serif;background:#1e1e2f;color:#eee;padding:20px;}
h1{text-align:center;}
img{max-width:100%;border:1px solid #555;}
pre{background:#2a2a3a;padding:10px;border-radius:4px;overflow:auto;max-height:400px;white-space:pre;}
</style>
</head>
<body>
<h1>📈 OPT101 录制报告</h1>
<p>录制时间: ${new Date().toLocaleString()}</p>
<p>数据点数: ${recDataList.length}</p>
<img src="${imageData}" alt="波形截图">
<h2>📊 数据 (CSV格式)</h2>
<pre>${escapedCSV}</pre>
</body>
</html>`;
        // 下载HTML文件
        const blob = new Blob([htmlContent], {type:"text/html;charset=utf-8"});
        const url = URL.createObjectURL(blob);
        const aTag = document.createElement("a");
        aTag.download = "OPT101_录制报告_" + new Date().getTime() + ".html";
        aTag.href = url;
        aTag.click();
        URL.revokeObjectURL(url);

        recStatusText.innerText = "✅ 报告已下载";
        recStatusText.style.color = "#4caf50";
    };

    // 保存截图
    saveImgBtn.onclick = ()=>{
        const canvas = document.getElementById("waveCanvas");
        const link = document.createElement("a");
        link.download = "OPT101_波形截图_" + new Date().getTime() + ".png";
        link.href = canvas.toDataURL("image/png");
        link.click();
    };

    // 导出CSV（独立按钮）
    exportCsvBtn.onclick = ()=>{
        if (recDataList.length === 0) {
            alert("没有录制数据，请先录制！");
            return;
        }
        const csvText = generateCSVText();
        const blob = new Blob([csvText], {type:"text/csv;charset=utf-8"});
        const url = URL.createObjectURL(blob);
        const aTag = document.createElement("a");
        aTag.download = "OPT101_录制数据_" + new Date().getTime() + ".csv";
        aTag.href = url;
        aTag.click();
        URL.revokeObjectURL(url);
    };
}

// 生成CSV文本（供复用）
function generateCSVText(){
    let csvText = "相对时间ms,ADC数值,电压V\n";
    recDataList.forEach(item=>{
        csvText += `${item.timeMs},${item.adcValue},${item.voltage.toFixed(4)}\n`;
    });
    return csvText;
}

// 绘制Y轴网格（原有）
function drawAxisGrid(){
    ctx.strokeStyle = "#666";
    ctx.lineWidth = 1;
    for(let i=Y_LABEL_WIDTH;i<canvasW;i+=40){
        ctx.beginPath();
        ctx.moveTo(i, 0);
        ctx.lineTo(i, canvasH);
        ctx.stroke();
    }
    const scaleY = canvasH / Y_MAX;
    Y_TICKS.forEach(val=>{
        const yPos = canvasH - val * scaleY;
        ctx.beginPath();
        ctx.moveTo(Y_LABEL_WIDTH, yPos);
        ctx.lineTo(canvasW, yPos);
        ctx.stroke();
        ctx.fillText(val.toString(), 5, yPos + 4);
    });
    ctx.save();
    ctx.translate(15, canvasH / 2);
    ctx.rotate(-Math.PI / 2);
    ctx.fillText("ADC数值", 0, 0);
    ctx.restore();
    ctx.strokeStyle="#777";
    ctx.strokeRect(Y_LABEL_WIDTH, 0, canvasW-Y_LABEL_WIDTH, canvasH);
}

// 绘制波形（原有）
function drawWave(){
    ctx.clearRect(0,0,canvasW,canvasH);
    if(dataBuffer.length < 2) { drawAxisGrid(); return; }
    const stepX = (canvasW - Y_LABEL_WIDTH) / MAX_POINTS;
    const scaleY = canvasH / Y_MAX;
    ctx.beginPath();
    ctx.strokeStyle = "#4caf50";
    ctx.lineWidth = 2;
    for(let i=0; i<dataBuffer.length; i++){
        const x = Y_LABEL_WIDTH + i * stepX;
        const y = canvasH - dataBuffer[i] * scaleY;
        i === 0 ? ctx.moveTo(x,y) : ctx.lineTo(x,y);
    }
    ctx.stroke();
    ctx.lineTo(Y_LABEL_WIDTH + (dataBuffer.length-1)*stepX, canvasH);
    ctx.lineTo(Y_LABEL_WIDTH, canvasH);
    ctx.closePath();
    ctx.fillStyle = "rgba(76,175,80,0.1)";
    ctx.fill();
    drawAxisGrid();
}

// 推送新数据
function pushData(val){
    if(isRecording){
        const timeDelta = Date.now() - recStartTime;
        recDataList.push({
            timeMs: timeDelta,
            adcValue: val,
            voltage: val * 3.3 / 4095
        });
    }
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
    drawAxisGrid();
    connectWs();
    bindButtonEvent();
};
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