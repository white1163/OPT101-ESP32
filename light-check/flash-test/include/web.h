#ifndef WEB_H
#define WEB_H

#include <Arduino.h>

// 初始化 Web 服务器和 WebSocket
void webBegin();

// 需要在主循环中定期调用，处理 WebSocket 和 HTTP 请求
void webLoop();

// 推送电压值到所有连接的网页客户端
void webPushVoltage(float voltage);

#endif