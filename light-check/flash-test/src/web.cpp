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
    <script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
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
        canvas { max-height: 500px; width: 100%; }
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
    let ws;
    let chart;

    function initChart() {
        const ctx = document.getElementById('waveCanvas').getContext('2d');
        chart = new Chart(ctx, {
            type: 'line',
            data: {
                datasets: [{
                    label: '电压 (V)',
                    data: [],
                    borderColor: '#4caf50',
                    backgroundColor: 'rgba(76, 175, 80, 0.1)',
                    borderWidth: 2,
                    pointRadius: 0,
                    tension: 0.2,
                    fill: true
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: true,
                scales: {
                    x: { title: { display: true, text: '数据点序号', color: '#ccc' }, grid: { color: '#444' } },
                    y: { title: { display: true, text: '电压 (V)', color: '#ccc' }, min: 0, max: 3.3, grid: { color: '#444' } }
                },
                plugins: {
                    tooltip: { mode: 'index', intersect: false },
                    legend: { labels: { color: '#eee' } }
                }
            }
        });
    }

    function addDataPoint(value) {
        if (!chart) return;
        let dataset = chart.data.datasets[0];
        let newData = dataset.data.concat(value);
        if (newData.length > MAX_POINTS) newData.shift();
        dataset.data = newData;
        chart.update('none');
        document.getElementById('currentValue').innerHTML = value.toFixed(3) + ' V';
    }

    function connectWebSocket() {
        ws = new WebSocket('ws://' + window.location.hostname + ':81');
        ws.onopen = function() {
            document.getElementById('statusLabel').innerHTML = '🟢 已连接';
            document.getElementById('statusLabel').className = 'status connected';
        };
        ws.onmessage = function(event) {
            let val = parseFloat(event.data);
            if (!isNaN(val)) addDataPoint(val);
        };
        ws.onclose = function() {
            document.getElementById('statusLabel').innerHTML = '🔴 连接断开，5秒后重连';
            document.getElementById('statusLabel').className = 'status disconnected';
            setTimeout(connectWebSocket, 5000);
        };
        ws.onerror = function(err) {
            console.error('WebSocket 错误', err);
        };
    }

    window.onload = function() {
        initChart();
        connectWebSocket();
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