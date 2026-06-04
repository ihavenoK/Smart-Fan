# IoT Pulsar SDK

Pulsar Java SDK 客户端，用于连接 OneNET IoT 平台，实现设备与云端的数据通信。

## 环境依赖

| 软件    | 版本        |
|---------|-----------|
| Java    | 1.8.0_202 |
| Maven   | 3.6.3     |

## 配置说明

使用前请修改 `src/main/java/.../IoTPulsarConsume.java` 中的以下配置：

### 1. 产品配置信息

```java
targetProductId = "你的产品B的productId"; // 第79行：目标产品B的productId
targetProductId = "你的产品A的productId"; // 第82行：目标产品A的productId
String accessKey = "你的产品accessKey";   // 第133行：从产品详情页获取
```

### 2. 设备名称配置

```java
if (deviceName.equals("app")) {
    targetDeviceName = "ESP8266-01";      // 请根据实际设备名称修改
} else if (deviceName.equals("ESP8266-01")) {
    targetDeviceName = "app";             // 请根据实际设备名称修改
}
```

## API 接口

### 查询设备属性详情

`POST https://iot-api.heclouds.com/thingmodel/query-device-property-detail`

根据产品ID和设备名，下发物模型属性获取命令到设备，设备返回属性值。

- 仅支持 OneJson 设备，需要设备在线
- Content-type: `application/json`

**请求参数：**

| 参数 | 类型 | 必选 | 描述 |
|------|------|------|------|
| product_id | string | 是 | 产品ID，平台生成唯一ID |
| device_name | string | 是 | 设备名称 |
| params | array | 是 | 功能点标识数组，如 `["light","model"]` |

**响应：** 返回 `code`、`msg`、`request_id`、`data`（业务数据）

### 设置设备属性

`POST https://iot-api.heclouds.com/thingmodel/set-device-property`

根据产品ID和设备名，下发物模型属性设置命令到设备。

**请求参数：**

| 参数 | 类型 | 必选 | 描述 |
|------|------|------|------|
| product_id | string | 是 | 产品ID |
| device_name | string | 是 | 设备名称 |
| params | object | 是 | 属性值 JSON，如 `{"switch": true}` |
