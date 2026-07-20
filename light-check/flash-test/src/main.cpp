#include <WiFi.h>
#include "web.h"

// 热点配置
const char* ssid = "ESP32_OPT101";
const char* password = "";

// UDP 配置
WiFiUDP udp;
const uint16_t udpPort = 8888;
const IPAddress broadcastIP(192, 168, 4, 255);

// OPT101 引脚（确认是有效 ADC 引脚）
#define OPT101_PIN  5

// 滞回比较阈值 (V) – 避免噪声引起的反复触发
const float thresholdHigh = 1.5;   // 高于此值认为“高”
const float thresholdLow  = 1.2;   // 低于此值认为“低”

// 超时设置 (ms)：如果超过此时间未检测到新脉冲，频率清零
const unsigned long timeoutMs = 3000;

// 频率测量变量
unsigned long lastPulseTime = 0;        // 上一次脉冲的时刻 (ms)
float currentFreq = 0.0;                // 当前频率 (Hz)
bool hasValidPulse = false;              // 是否收到过有效脉冲
bool lastStateHigh = false;              // 上一次比较后的状态（高/低）

// 辅助：获取当前信号的高低状态（滞回比较）
bool isSignalHigh(float voltage) {
  static bool state = false;   // 当前状态
  if (voltage > thresholdHigh) {
    state = true;
  } else if (voltage < thresholdLow) {
    state = false;
  }
  // 如果在滞回区间内，保持之前的状态
  return state;
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  
  WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP: ");
  Serial.println(myIP);

  webBegin();
  

  udp.begin(udpPort);
  Serial.printf("UDP broadcast on port %d, thresholds: %.2f/%.2fV\n", 
                udpPort, thresholdLow, thresholdHigh);
}

void loop() {
  // 读取 ADC 并转换电压
  int adcValue = analogRead(OPT101_PIN);
  float voltage = adcValue * (3.3f / 4095.0f);
  
  // 滞回比较得到当前高低状态
  bool nowHigh = isSignalHigh(voltage);
  
  // 检测上升沿（从低到高）
  if (!lastStateHigh && nowHigh) {
    unsigned long nowTime = millis();
    if (hasValidPulse) {
      unsigned long interval = nowTime - lastPulseTime;
      if (interval > 0 && interval < timeoutMs) {
        currentFreq = 1000.0f / interval;
        if (currentFreq > 50) currentFreq = 0;
      } else {
        // 间隔异常（超时或负），频率清零
        currentFreq = 0;
      }
    } else {
      hasValidPulse = true;
    }
    lastPulseTime = nowTime;
  }
  lastStateHigh = nowHigh;
  
  // 超时监测：如果长时间没有新脉冲，频率归零
  if (hasValidPulse && (millis() - lastPulseTime > timeoutMs)) {
    currentFreq = 0;
    hasValidPulse = false;   // 重置标志，等待下一个脉冲
    // 可选：串口输出超时信息
    // Serial.println("Timeout: no pulse");
  }
  
  // 定期广播（每 200ms）
  static unsigned long lastBroadcast = 0;
  if (millis() - lastBroadcast >= 200) {
    lastBroadcast = millis();
    
    char buffer[128];
    snprintf(buffer, sizeof(buffer),
             "Freq=%.2fHz, Voltage=%.2fV, State=%d",
             currentFreq, voltage, nowHigh ? 1 : 0);
    
    udp.beginPacket(broadcastIP, udpPort);
    udp.print(buffer);
    udp.endPacket();
    
    Serial.println(buffer);
  }

    // ----- 推送电压值到网页（每 50ms 推送一次）-----
  static unsigned long lastWebPush = 0;   // 定义静态变量，保留上次推送时间
  if (millis() - lastWebPush >= 30) {     // 33Hz 推送频率
    lastWebPush = millis();
    webPushVoltage(adcValue);              // 调用正确的函数，只传电压值
  }

  // 处理 Web 服务器和 WebSocket 请求（必须频繁调用）
  webLoop();
  
  // 采样间隔 5ms (200Hz)
  delay(5);

}
