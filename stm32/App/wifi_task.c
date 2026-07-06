/**
  * @file    wifi_task.c
  * @brief   ESP8266 Wi-Fi + OneNet MQTT 任务
  *          通知值: 0x00 = USART 数据 / REPORT_ALL = 全量上报 / REPORT_SPEED = 风速上报
  *
  * @note    AT 命令中 JSON 逗号必须用 \, 转义，否则会被 AT 解析器当成分隔符
  */
#include "tasks.h"
#include "esp8266.h"
#include "OLED.h"
#include "debug_serial.h"
#include <stdio.h>
#include <string.h>

/*==============================================================================
 * 凭证配置
 *============================================================================*/
#define WIFI_SSID       "ihavenok"
#define WIFI_PASS       "88888888"
#define MQTT_PRODUCT_ID "e2F2Jz2iUU"
#define MQTT_DEVICE     "esp"
#define MQTT_TOKEN      "version=2018-10-31&res=products%2Fe2F2Jz2iUU%2Fdevices%2Fesp&et=1811086080&method=md5&sign=LtJe5lLvkCUUGGrvHNZs1w%3D%3D"

/*==============================================================================
 * MQTT 主题
 *============================================================================*/
#define MQTT_PUB_TOPIC        "$sys/" MQTT_PRODUCT_ID "/" MQTT_DEVICE "/thing/property/post"
#define MQTT_SUB_TOPIC        "$sys/" MQTT_PRODUCT_ID "/" MQTT_DEVICE "/thing/property/set"
#define MQTT_SUB_REPLY_TOPIC  "$sys/" MQTT_PRODUCT_ID "/" MQTT_DEVICE "/thing/property/set_reply"

/*==============================================================================
 * 静态变量
 *============================================================================*/
static WiFiState_t state = WIFI_INIT_START;

/*==============================================================================
 * 前向声明
 *============================================================================*/
static void WiFi_ProcessMQTT(void);
static void WiFi_ReportStatus(uint8_t reportType);
static void ShowOLEDMsg(uint8_t line, const char *msg);
static void WiFi_DumpRxBuf(const char *title);   /* 失败时打印接收缓冲 */

/*==============================================================================
 * 辅助：在 OLED 指定行显示信息
 *============================================================================*/
static void ShowOLEDMsg(uint8_t line, const char *msg)
{
    if (xSemaphoreTake(xOledMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        OLED_ShowString(0, (int16_t)line * 16, (char *)msg, OLED_8X16);
        OLED_Update();
        xSemaphoreGive(xOledMutex);
    }
}

/*==============================================================================
 * 辅助：失败时打印接收缓冲区（最多前 200 字节可打印字符）
 *============================================================================*/
static void WiFi_DumpRxBuf(const char *title)
{
    const char *rx = ESP8266_GetRxBuf();
    uint16_t len   = ESP8266_GetRxLen();
    DebugSerial_Printf("  [%s] 缓冲区(%u 字节): ", title, len);

    uint16_t i, cnt = 0;
    for (i = 0; i < len && cnt < 200; i++) {
        char c = rx[i];
        if (c == '\r')      { DebugSerial_SendString("\\r"); }
        else if (c == '\n') { DebugSerial_SendString("\\n"); }
        else if (c >= 0x20 && c <= 0x7E) { DebugSerial_SendByte(c); }
        else                { DebugSerial_SendByte('.'); }
        cnt++;
    }
    DebugSerial_SendString("\r\n");
}

/*==============================================================================
 * WiFi 任务入口
 *============================================================================*/
void vTaskWiFi(void *pvParameters)
{
    (void)pvParameters;

    DebugSerial_Printf("[WiFi] 任务启动, 初始化 ESP8266...\r\n");

    ESP8266_SetNotifyTask(xTaskGetCurrentTaskHandle());

    ESP8266_USART_Init(115200);
    ESP8266_RX_Reset();
    vTaskDelay(pdMS_TO_TICKS(300));

    state = WIFI_INIT_START;

    while (1) {
        switch (state) {

        /*==========================================================*/
        case WIFI_INIT_START: {
            DebugSerial_Printf("[WiFi] 步骤 1/7: 模块复位...\r\n");
            ESP8266_SendCmd("AT+RST");
            /* ESP8266 AT+RST 后需要 1~2 秒重启固件，等待 "ready" 信号 */
            if (ESP8266_CheckResponse("ready", 5000)) {
                DebugSerial_Printf("[WiFi]   -> ESP8266 就绪\r\n");
            } else {
                DebugSerial_Printf("[WiFi]   -> 未收到 ready，继续尝试...\r\n");
            }
            ESP8266_RX_Reset();
            state = WIFI_INIT_SET_MODE;
            break;
        }

        /*==========================================================*/
        case WIFI_INIT_SET_MODE: {
            DebugSerial_Printf("[WiFi] 步骤 2/7: 设置 STA 模式...\r\n");
            ESP8266_SendCmd("AT+CWMODE=1");
            if (ESP8266_CheckResponse("OK", 3000)) {
                DebugSerial_Printf("[WiFi]   -> 成功\r\n");
                ESP8266_RX_Reset();
                state = WIFI_INIT_CONNECT_AP;
            } else {
                DebugSerial_Printf("[WiFi]   -> 失败!\r\n");
                WiFi_DumpRxBuf("CWMODE");
                state = WIFI_FAILED;
            }
            break;
        }

        /*==========================================================*/
        case WIFI_INIT_CONNECT_AP: {
            char cmd[128];
            snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"",
                     WIFI_SSID, WIFI_PASS);
            DebugSerial_Printf("[WiFi] 步骤 3/7: 连接 %s ...\r\n", WIFI_SSID);
            ESP8266_SendCmd(cmd);

            uint8_t conn_flag = 0, ip_flag = 0;
            TickType_t xStart = xTaskGetTickCount();
            while ((xTaskGetTickCount() - xStart) < pdMS_TO_TICKS(12000)) {
                const char *rx = ESP8266_GetRxBuf();
                if (strstr(rx, "WIFI CONNECTED") != NULL) conn_flag = 1;
                if (strstr(rx, "WIFI GOT IP")    != NULL) ip_flag   = 1;
                if (conn_flag && ip_flag) break;
                vTaskDelay(pdMS_TO_TICKS(10));
            }

            if (conn_flag && ip_flag) {
                DebugSerial_Printf("[WiFi]   -> 连接成功, 已获取 IP\r\n");
                ESP8266_RX_Reset();
                vTaskDelay(pdMS_TO_TICKS(600));
                state = WIFI_INIT_SINGLE_CONN;
            } else {
                DebugSerial_Printf("[WiFi]   -> 失败! CONN=%d IP=%d\r\n",
                                   conn_flag, ip_flag);
                WiFi_DumpRxBuf("CWJAP");
                state = WIFI_FAILED;
            }
            break;
        }

        /*==========================================================*/
        case WIFI_INIT_SINGLE_CONN: {
            DebugSerial_Printf("[WiFi] 步骤 4/7: 单连接模式...\r\n");
            ESP8266_SendCmd("AT+CIPMUX=0");
            if (ESP8266_CheckResponse("OK", 3000)) {
                DebugSerial_Printf("[WiFi]   -> 成功\r\n");
                ESP8266_RX_Reset();
                state = WIFI_INIT_MQTT_CONFIG;
            } else {
                DebugSerial_Printf("[WiFi]   -> 失败!\r\n");
                WiFi_DumpRxBuf("CIPMUX");
                state = WIFI_FAILED;
            }
            break;
        }

        /*==========================================================*/
        case WIFI_INIT_MQTT_CONFIG: {
            char cmd[384];
            snprintf(cmd, sizeof(cmd),
                     "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"",
                     MQTT_DEVICE, MQTT_PRODUCT_ID, MQTT_TOKEN);
            DebugSerial_Printf("[WiFi] 步骤 5/7: 配置 MQTT 用户...\r\n");
            ESP8266_SendCmd(cmd);
            if (ESP8266_CheckResponse("OK", 5000)) {
                DebugSerial_Printf("[WiFi]   -> 成功\r\n");
                ESP8266_RX_Reset();
                state = WIFI_INIT_MQTT_CONNECT;
            } else {
                DebugSerial_Printf("[WiFi]   -> 失败!\r\n");
                WiFi_DumpRxBuf("MQTT_CFG");
                state = WIFI_FAILED;
            }
            break;
        }

        /*==========================================================*/
        case WIFI_INIT_MQTT_CONNECT: {
            DebugSerial_Printf("[WiFi] 步骤 6/7: 连接 MQTT Broker...\r\n");
            ESP8266_SendCmd("AT+MQTTCONN=0,\"mqtts.heclouds.com\",1883,1");
            if (ESP8266_CheckResponse("OK", 15000)) {
                DebugSerial_Printf("[WiFi]   -> 成功\r\n");
                ESP8266_RX_Reset();
                state = WIFI_INIT_MQTT_SUBSCRIBE;
            } else {
                DebugSerial_Printf("[WiFi]   -> 失败!\r\n");
                WiFi_DumpRxBuf("MQTT_CONN");
                state = WIFI_FAILED;
            }
            break;
        }

        /*==========================================================*/
        case WIFI_INIT_MQTT_SUBSCRIBE: {
            char topic[160];
            DebugSerial_Printf("[WiFi] 步骤 7/7: 订阅 MQTT 话题...\r\n");
            /* A */
            snprintf(topic, sizeof(topic),
                     "AT+MQTTSUB=0,\"$sys/%s/%s/thing/property/post/reply\",0",
                     MQTT_PRODUCT_ID, MQTT_DEVICE);
            ESP8266_SendCmd(topic);
            vTaskDelay(pdMS_TO_TICKS(500));

            /* B */
            snprintf(topic, sizeof(topic),
                     "AT+MQTTSUB=0,\"$sys/%s/%s/thing/property/set\",0",
                     MQTT_PRODUCT_ID, MQTT_DEVICE);
            ESP8266_SendCmd(topic);

            if (ESP8266_CheckResponse("OK", 5000)) {
                DebugSerial_Printf("[WiFi]   -> 成功\r\n");
                DebugSerial_Printf("[WiFi] ===== 初始化完成 =====\r\n");
                if (xSemaphoreTake(xSystemMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    g_SystemState.wifiConnected = 1;
                    xSemaphoreGive(xSystemMutex);
                }
                ShowOLEDMsg(3, "WiFi OK");
                state = WIFI_IDLE;
            } else {
                DebugSerial_Printf("[WiFi]   -> 失败!\r\n");
                WiFi_DumpRxBuf("MQTT_SUB");
                state = WIFI_FAILED;
            }
            break;
        }

        /*==== 空闲主循环 ====*/
        case WIFI_IDLE: {
            static uint32_t lastReportTick = 0;
            uint32_t ulNotifiedValue;

            BaseType_t result = xTaskNotifyWait(0x00,
                                                0xFFFFFFFF,
                                                &ulNotifiedValue,
                                                pdMS_TO_TICKS(200));

            if (result == pdTRUE) {
                if (ulNotifiedValue == 0x00) {
                    WiFi_ProcessMQTT();
                } else if (ulNotifiedValue & REPORT_ALL) {
                    WiFi_ReportStatus(REPORT_ALL);
                } else if (ulNotifiedValue & REPORT_SPEED) {
                    WiFi_ReportStatus(REPORT_SPEED);
                }
            }

            /* 60 秒定期全量上报 */
            if ((xTaskGetTickCount() - lastReportTick) > pdMS_TO_TICKS(60000)) {
                WiFi_ReportStatus(REPORT_ALL);
                lastReportTick = xTaskGetTickCount();
            }

            /* 心跳检测 */
            if (strstr(ESP8266_GetRxBuf(), "ERROR") != NULL) {
                DebugSerial_Printf("[WiFi] 检测到 ERROR\r\n");
                WiFi_DumpRxBuf("ERR");
                if (xSemaphoreTake(xSystemMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    g_SystemState.wifiConnected = 0;
                    xSemaphoreGive(xSystemMutex);
                }
                state = WIFI_FAILED;
            }
            break;
        }

        /*==== 初始化失败：打印信息后驻留，不重试 ====*/
        case WIFI_FAILED:
            DebugSerial_Printf("[WiFi] === 初始化失败, 停止 ===\r\n");
            if (xSemaphoreTake(xSystemMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                g_SystemState.wifiConnected = 0;
                xSemaphoreGive(xSystemMutex);
            }
            ShowOLEDMsg(3, "WiFi FAIL");
            /* 驻留：永远停在这里 */
            vTaskSuspend(NULL);

        } /* switch */
    } /* while(1) */
}

/*==============================================================================
 * MQTT 下发数据解析
 *   接收格式: +MQTTSUBRECV:0,"topic",len,{"id":"N","version":"1.0",...}
 *   字段均为 int32: mode / speed / countdown
 *============================================================================*/
static void WiFi_ProcessMQTT(void)
{
    const char *rx = ESP8266_GetRxBuf();
    const char *pStart = strstr(rx, "+MQTTSUBRECV:0,");
    if (pStart == NULL) return;

    DebugSerial_Printf("[WiFi] 收到 MQTT 下发数据\r\n");

    /* 跳过前缀 + topic + 长度 → 定位 JSON */
    const char *pJson = strchr(pStart + 16, ',');   /* 跳过前缀 → topic */
    if (pJson) pJson = strchr(pJson + 1, ',');       /* 跳过 topic → len */
    if (pJson) pJson++;                               /* JSON 起始 */
    if (pJson == NULL) return;

    /* 提取消息 id（每下发一次 +1，回复需对应） */
    uint32_t msgId = 0;
    const char *pId = strstr(pJson, "\"id\"");
    if (pId) {
        pId = strchr(pId, ':');
        if (pId) sscanf(pId + 1, "%lu", &msgId);
    }

    /*---- 解析 mode ----*/
    const char *pMode = strstr(pJson, "\"mode\"");
    if (pMode) {
        const char *pVal = strchr(pMode, ':');
        if (pVal) {
            int val = 0;
            if (sscanf(pVal + 1, "%d", &val) == 1) {
                if (xSemaphoreTake(xSystemMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    if (val >= MODE_MANUAL && val <= MODE_TIMER)
                        g_SystemState.mode = (FanMode_t)val;
                    xSemaphoreGive(xSystemMutex);
                }
            }
        }
    }

    /*---- 解析 speed（0~100） ----*/
    const char *pSpeed = strstr(pJson, "\"speed\"");
    if (pSpeed) {
        const char *pVal = strchr(pSpeed, ':');
        if (pVal) {
            int val = 0;
            if (sscanf(pVal + 1, "%d", &val) == 1) {
                if (val < 0)   val = 0;
                if (val > 100) val = 100;
                if (xSemaphoreTake(xSystemMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    g_SystemState.targetSpeed = (uint8_t)val;
                    xSemaphoreGive(xSystemMutex);
                }
            }
        }
    }

    /*---- 解析 countdown（定时分钟数） ----*/
    const char *pCd = strstr(pJson, "\"countdown\"");
    if (pCd) {
        const char *pVal = strchr(pCd, ':');
        if (pVal) {
            uint32_t val = 0;
            if (sscanf(pVal + 1, "%lu", &val) == 1) {
                if (xSemaphoreTake(xSystemMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    g_SystemState.timerMinutes = val;
                    if (val > 0) g_SystemState.mode = MODE_TIMER;
                    xSemaphoreGive(xSystemMutex);
                }
            }
        }
    }

    /* 回复平台：id 必须与接收到的 msgId 一致 */
    if (msgId > 0) {
        char reply[256];
        snprintf(reply, sizeof(reply),
                 "AT+MQTTPUB=0,\"%s\","
                 "\"{\\\"id\\\":\\\"%lu\\\""
                 "\\,\\\"code\\\":200"
                 "\\,\\\"msg\\\":\\\"success\\\"}\",0,0",
                 MQTT_SUB_REPLY_TOPIC, msgId);
        ESP8266_SendCmd(reply);
        vTaskDelay(pdMS_TO_TICKS(300));
    }

    ESP8266_RX_Reset();
}

/*==============================================================================
 * 状态上报函数
 *   AT 命令中 JSON 逗号必须转义为 \,  （C 源码: \\,）
 *   字段名: temp / humi / speed / mode / countdown
 *============================================================================*/
static void WiFi_ReportStatus(uint8_t reportType)
{
    uint8_t temp, humi, speed;
    uint8_t mode;
    uint32_t cd;

    if (xSemaphoreTake(xSystemMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        temp  = g_SystemState.temperature;
        humi  = g_SystemState.humidity;
        speed = g_SystemState.currentSpeed;
        mode  = (uint8_t)g_SystemState.mode;
        cd    = g_SystemState.timerMinutes;
        xSemaphoreGive(xSystemMutex);
    } else {
        return;
    }

    char payload[512];

    if (reportType & REPORT_ALL) {
        snprintf(payload, sizeof(payload),
            "AT+MQTTPUB=0,\"%s\","
            "\"{\\\"id\\\":\\\"%lu\\\""
            "\\,\\\"version\\\":\\\"1.0\\\""
            "\\,\\\"params\\\":{"
            "\\\"speed\\\":{\\\"value\\\":%d}"
            "\\,\\\"temp\\\":{\\\"value\\\":%d}"
            "\\,\\\"humi\\\":{\\\"value\\\":%d}"
            "\\,\\\"mode\\\":{\\\"value\\\":%d}"
            "\\,\\\"countdown\\\":{\\\"value\\\":%lu}"
            "}}\",0,0",
            MQTT_PUB_TOPIC, xTaskGetTickCount(),
            speed, temp, humi, mode, cd);
    } else {   /* REPORT_SPEED */
        snprintf(payload, sizeof(payload),
            "AT+MQTTPUB=0,\"%s\","
            "\"{\\\"id\\\":\\\"%lu\\\""
            "\\,\\\"version\\\":\\\"1.0\\\""
            "\\,\\\"params\\\":{"
            "\\\"speed\\\":{\\\"value\\\":%d}"
            "}}\",0,0",
            MQTT_PUB_TOPIC, xTaskGetTickCount(), speed);
    }

    ESP8266_SendCmd(payload);
    vTaskDelay(pdMS_TO_TICKS(500));
}
