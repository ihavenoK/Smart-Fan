package com.chinamobile.iot.pulsar.example;

import com.alibaba.fastjson2.JSONObject;
import com.chinamobile.iot.pulsar.config.IoTConfig;
import com.chinamobile.iot.pulsar.auth.IoTConsumer;
import com.chinamobile.iot.pulsar.auth.IoTMessage;
import com.chinamobile.iot.pulsar.auth.AESBase64Utils;
import com.chinamobile.iot.pulsar.auth.TokenUtil;

import io.netty.util.internal.StringUtil;
import org.apache.http.client.methods.CloseableHttpResponse;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.entity.StringEntity;
import org.apache.http.impl.client.CloseableHttpClient;
import org.apache.http.impl.client.HttpClients;
import org.apache.http.util.EntityUtils;
import org.apache.pulsar.client.api.MessageId;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.HashMap;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

//@Slf4j
public class IoTPulsarConsume {
    private static final Logger logger = LoggerFactory.getLogger(IoTPulsarConsume.class);
    //TODO need to set iotAccessId 消费组ID
    private static  String iotAccessId="2brAZHNZWosBQEj11247";
    //TODO need to set iotSecretKey 消费组KEY
    private static  String iotSecretKey="9be379143d9247c1bc59b602c9c4bd62";

    //TODO 订阅名称
    private static  String iotSubscriptionName="2brAZHNZWosBQEj11247-sub";

    // 防循环缓存
    private static final Map<String, CacheEntry> syncCache = new ConcurrentHashMap<>();
    private static final long CACHE_TTL_MS = 5000; // 5秒

    static class CacheEntry {
        Object value;
        long timestamp;
        CacheEntry(Object value) {
            this.value = value;
            this.timestamp = System.currentTimeMillis();
        }
    }

    public static void main(String[] args) throws Exception {
        if (StringUtil.isNullOrEmpty(iotAccessId)) {
            logger.error("iotAccessId is null,please input iotAccessId");
            System.exit(1);
        }
        if (StringUtil.isNullOrEmpty(iotSecretKey)) {
            logger.error("iotSecretKey is null,please input iotSecretKey");
            System.exit(1);
        }
        if (StringUtil.isNullOrEmpty(iotSubscriptionName)) {
            logger.error("iotSubscriptionName is null,please input iotSubscriptionName");
            System.exit(1);
        }
        //TODO 建议收到消息后将消息转到中间件后立即ACK。避免消息量过大导致消息过期
        IoTConsumer iotConsumer = IoTConsumer.IOTConsumerBuilder.anIOTConsumer().brokerServerUrl(IoTConfig.brokerSSLServerUrl)
                .iotAccessId(iotAccessId)
                .iotSecretKey(iotSecretKey)
                .subscriptionName(iotSubscriptionName)
                .iotMessageListener(message -> {
                    MessageId msgId = message.getMessageId();
                    long publishTime = message.getPublishTime();
                    String payload = new String(message.getData());
                    IoTMessage iotMessage= JSONObject.parseObject(payload, IoTMessage.class);
                    String originalMsg= AESBase64Utils.decrypt(iotMessage.getData(),iotSecretKey.substring(8,24));
                    logger.info("IOT consume message======>>>>>>> messageId={}, publishTime={},  payload={}",
                            msgId, publishTime, payload);
                    logger.info("IOT originalMsg:{}",originalMsg);
                    
                    // 处理设备属性同步
                    handleDevicePropertySync(originalMsg);
                }).build();
        iotConsumer.run();
    }

    private static void handleDevicePropertySync(String originalMsg) {
        try {
            // 解析原始消息（originalMsg 已经是解密后的JSON字符串）
            JSONObject originalJson = JSONObject.parseObject(originalMsg);
            String msgType = originalJson.getString("msgType");
            if (!"thingProperty".equals(msgType)) {
                return; // 只处理属性上报
            }

            JSONObject subData = originalJson.getJSONObject("subData");
            String deviceName = subData.getString("deviceName");
            String productId = subData.getString("productId");
            JSONObject params = subData.getJSONObject("params"); // 属性值，格式如 {"LED":{"value":1},"temp":{"value":25},"humi":{"value":60}}

            // 提取需要同步的属性值（简化处理：直接取 params 中的每个属性的 value）
            Map<String, Object> propertiesToSet = new HashMap<>();
            for (String key : params.keySet()) {
                Object val = params.get(key);
                if (val instanceof JSONObject) {
                    // 如果值是 { "value": xxx } 形式，取里面的 value
                    val = ((JSONObject) val).get("value");
                }
                propertiesToSet.put(key, val);
            }

            // 判断来源设备，确定目标设备
            String targetDeviceName;
            String targetProductId;
            if (deviceName.equals("app")) {
                targetDeviceName = "esp";
                targetProductId = "e2F2Jz2iUU"; // 如果两个设备同产品，则与 source 相同
            } else if (deviceName.equals("esp")) {
                targetDeviceName = "app";
                targetProductId = "e2F2Jz2iUU";
            } else {
                return; // 不是这两个设备，忽略
            }

            // 防循环：检查最近是否处理过相同的属性变化
            boolean needSync = false;
            for (Map.Entry<String, Object> entry : propertiesToSet.entrySet()) {
                String prop = entry.getKey();
                Object val = entry.getValue();
                String cacheKey = deviceName + "->" + targetDeviceName + ":" + prop;
                CacheEntry cached = syncCache.get(cacheKey);
                long now = System.currentTimeMillis();
                if (cached != null && cached.value.equals(val) && (now - cached.timestamp) < CACHE_TTL_MS) {
                    // 短时间内相同值，不触发同步
                    continue;
                }
                // 更新缓存
                syncCache.put(cacheKey, new CacheEntry(val));
                needSync = true;
            }
            
            if (!needSync) {
                logger.info("所有属性均在缓存期内，忽略本次同步");
                return;
            }

            // 调用 API 设置目标设备属性
            boolean success = callSetDeviceProperty(targetProductId, targetDeviceName, propertiesToSet);
            if (success) {
                logger.info("成功将属性 {} 从 {} 同步到 {}", propertiesToSet, deviceName, targetDeviceName);
            } else {
                logger.error("同步属性失败");
            }
        } catch (Exception e) {
            logger.error("处理设备属性同步异常", e);
        }
    }

    /**
     * 获取设备属性详情（可选功能，用于查询设备当前属性）
     * @param productId 产品ID
     * @param deviceName 设备名称
     * @param properties 要查询的属性数组
     * @return 属性值的Map，如果失败返回null
     */
    private static Map<String, Object> queryDevicePropertyDetail(String productId, String deviceName, String[] properties) {
        try {
            String url = "https://iot-api.heclouds.com/thingmodel/query-device-property-detail";
            JSONObject body = new JSONObject();
            body.put("product_id", productId);
            body.put("device_name", deviceName);
            body.put("params", properties);

            // 生成鉴权头
            String accessKey = "iZk3oZE3a5/pvX2vzemTazUPh4F9kxXZv7Gb9SU7NZ0=";
            String version = "2022-05-01";
            String resourceName = "products/" + productId;
            long et = System.currentTimeMillis() / 1000 + 3600;
            String method = "sha1";
            String authorization = TokenUtil.assembleToken(version, resourceName, String.valueOf(et), method, accessKey);

            CloseableHttpClient httpClient = HttpClients.createDefault();
            HttpPost httpPost = new HttpPost(url);
            httpPost.setHeader("Content-Type", "application/json");
            httpPost.setHeader("authorization", authorization);
            httpPost.setEntity(new StringEntity(body.toJSONString(), "UTF-8"));

            CloseableHttpResponse response = httpClient.execute(httpPost);
            try {
                String responseStr = EntityUtils.toString(response.getEntity());
                JSONObject respJson = JSONObject.parseObject(responseStr);
                int code = respJson.getIntValue("code");
                if (code == 0) {
                    JSONObject data = respJson.getJSONObject("data");
                    Map<String, Object> result = new HashMap<>();
                    for (String key : data.keySet()) {
                        result.put(key, data.get(key));
                    }
                    return result;
                } else {
                    logger.error("查询设备属性失败: {}", respJson.getString("msg"));
                    return null;
                }
            } finally {
                response.close();
                httpClient.close();
            }
        } catch (Exception e) {
            logger.error("调用查询设备属性API异常", e);
            return null;
        }
    }

    private static boolean callSetDeviceProperty(String productId, String deviceName, Map<String, Object> properties) {
        try {
            // 1. 准备参数
            String url = "https://iot-api.heclouds.com/thingmodel/set-device-property";
            JSONObject body = new JSONObject();
            body.put("product_id", productId);
            body.put("device_name", deviceName);
            body.put("params", properties); // 根据API文档，参数名应为"params"

            // 2. 生成鉴权头
            // 需要从平台获取 accessKey（产品级或用户级），这里假设使用产品级 accessKey
            String accessKey = "iZk3oZE3a5/pvX2vzemTazUPh4F9kxXZv7Gb9SU7NZ0="; // 从产品详情页获取
            String version = "2022-05-01";
            String resourceName = "products/" + productId; // 使用产品级资源
            long et = System.currentTimeMillis() / 1000 + 3600; // 过期时间1小时
            String method = "sha1";
            String authorization = TokenUtil.assembleToken(version, resourceName, String.valueOf(et), method, accessKey);

            // 3. 发送HTTP POST请求
            CloseableHttpClient httpClient = HttpClients.createDefault();
            HttpPost httpPost = new HttpPost(url);
            httpPost.setHeader("Content-Type", "application/json");
            httpPost.setHeader("authorization", authorization);
            httpPost.setEntity(new StringEntity(body.toJSONString(), "UTF-8"));

            CloseableHttpResponse response = httpClient.execute(httpPost);
            try {
                int statusCode = response.getStatusLine().getStatusCode();
                String responseStr = EntityUtils.toString(response.getEntity());
                logger.info("API响应状态码: {}, 响应内容: {}", statusCode, responseStr);
                
                if (statusCode == 200) {
                    JSONObject respJson = JSONObject.parseObject(responseStr);
                    int code = respJson.getIntValue("code");
                    String msg = respJson.getString("msg");
                    if (code == 0) {
                        logger.info("API调用成功: {}", msg);
                        return true;
                    } else {
                        logger.error("API调用失败，错误码: {}, 错误信息: {}", code, msg);
                        return false;
                    }
                } else {
                    logger.error("HTTP请求失败，状态码: {}, 响应内容: {}", statusCode, responseStr);
                    return false;
                }
            } finally {
                response.close();
                httpClient.close();
            }
        } catch (Exception e) {
            logger.error("调用设置设备属性API异常", e);
            return false;
        }
    }
}
