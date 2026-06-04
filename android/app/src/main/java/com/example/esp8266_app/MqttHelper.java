package com.example.esp8266_app;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken;
import org.eclipse.paho.client.mqttv3.MqttCallback;
import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttException;
import org.eclipse.paho.client.mqttv3.MqttMessage;
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MqttHelper {
    private static final String TAG = "MqttHelper";

    private String serverURI;      // tcp://mqtts.heclouds.com:1883
    private String clientId;       // 设备名称 "app"
    private String userName;       // 产品ID
    private String password;       // 动态生成的Token

    private MqttClient mqttClient;
    private MqttConnectionListener listener;
    private ExecutorService executor = Executors.newSingleThreadExecutor();
    private Handler mainHandler = new Handler(Looper.getMainLooper());

    private boolean isConnecting = false; // 防止重复连接
    private int reconnectAttempts = 0;
    private static final int MAX_RECONNECT_ATTEMPTS = 3;
    private static final long BASE_RECONNECT_DELAY = 2000; // 2秒

    public interface MqttConnectionListener {
        void onConnectSuccess();
        void onConnectFailed(Throwable reason);
        void onDisconnect();
        void onMessageArrived(String topic, String message);
        void onDeliveryComplete(IMqttDeliveryToken token);
    }

    public MqttHelper(String serverURI, String clientId, String userName, String password,
                      MqttConnectionListener listener) {
        this.serverURI = serverURI;
        this.clientId = clientId;
        this.userName = userName;
        this.password = password;
        this.listener = listener;
    }

    public void connect() {
        if (mqttClient != null && mqttClient.isConnected()) {
            Log.i(TAG, "Already connected");
            return;
        }
        if (isConnecting) {
            Log.i(TAG, "Connection already in progress");
            return;
        }
        isConnecting = true;
        executor.execute(new Runnable() {
            @Override
            public void run() {
                try {
                    if (mqttClient == null) {
                        mqttClient = new MqttClient(serverURI, clientId, new MemoryPersistence());
                        mqttClient.setCallback(new MqttCallback() {
                            @Override
                            public void connectionLost(Throwable cause) {
                                Log.e(TAG, "Connection lost", cause);
                                // 在主线程通知
                                mainHandler.post(new Runnable() {
                                    @Override
                                    public void run() {
                                        if (listener != null) {
                                            listener.onDisconnect();
                                        }
                                    }
                                });
                                // 尝试重连（手动控制）
                                attemptReconnect();
                            }

                            @Override
                            public void messageArrived(String topic, MqttMessage message) throws Exception {
                                final String msg = new String(message.getPayload());
                                Log.i(TAG, "Message arrived on topic " + topic + ": " + msg);
                                mainHandler.post(new Runnable() {
                                    @Override
                                    public void run() {
                                        if (listener != null) {
                                            listener.onMessageArrived(topic, msg);
                                        }
                                    }
                                });
                            }

                            @Override
                            public void deliveryComplete(IMqttDeliveryToken token) {
                                Log.i(TAG, "Delivery complete");
                                mainHandler.post(new Runnable() {
                                    @Override
                                    public void run() {
                                        if (listener != null) {
                                            listener.onDeliveryComplete(token);
                                        }
                                    }
                                });
                            }
                        });
                    }

                    MqttConnectOptions options = new MqttConnectOptions();
                    options.setCleanSession(true);   // clean session必须为1
                    options.setUserName(userName);
                    options.setPassword(password.toCharArray());
                    options.setConnectionTimeout(30);
                    options.setKeepAliveInterval(60);
                    options.setAutomaticReconnect(false); // 我们自己控制重连

                    Log.i(TAG, "Connecting to " + serverURI + " with clientId " + clientId);
                    mqttClient.connect(options);
                    Log.i(TAG, "Connected successfully");

                    // 连接成功后订阅属性上报回复Topic
                    String replyTopic = "$sys/" + userName + "/" + clientId + "/thing/property/post/reply";
                    mqttClient.subscribe(replyTopic, 0); // QoS 0

                    // 新增：订阅属性设置Topic
                    String setTopic = "$sys/" + userName + "/" + clientId + "/thing/property/set";
                    mqttClient.subscribe(setTopic, 0);
                    Log.i(TAG, "Subscribed to property set topic: " + setTopic);

                    reconnectAttempts = 0; // 重置重试计数
                    isConnecting = false;
                    mainHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            if (listener != null) listener.onConnectSuccess();
                        }
                    });

                } catch (MqttException e) {
                    Log.e(TAG, "Connect failed", e);
                    isConnecting = false;
                    mainHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            if (listener != null) listener.onConnectFailed(e);
                        }
                    });
                    // 触发重连
                    attemptReconnect();
                }
            }
        });
    }

    private void attemptReconnect() {
        if (reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
            Log.e(TAG, "Max reconnect attempts reached, stop retrying.");
            return;
        }
        reconnectAttempts++;
        long delay = BASE_RECONNECT_DELAY * (long) Math.pow(2, reconnectAttempts - 1); // 指数退避
        Log.i(TAG, "Scheduling reconnect attempt " + reconnectAttempts + " in " + delay + "ms");
        mainHandler.postDelayed(new Runnable() {
            @Override
            public void run() {
                connect();
            }
        }, delay);
    }

    public void disconnect() {
        if (mqttClient == null || !mqttClient.isConnected()) {
            Log.i(TAG, "Already disconnected");
            return;
        }
        executor.execute(new Runnable() {
            @Override
            public void run() {
                try {
                    mqttClient.disconnect();
                    Log.i(TAG, "Disconnected");
                    mainHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            if (listener != null) listener.onDisconnect();
                        }
                    });
                } catch (MqttException e) {
                    Log.e(TAG, "Disconnect failed", e);
                }
            }
        });
    }

    public void publish(String topic, String payload, int qos) {
        if (mqttClient == null || !mqttClient.isConnected()) {
            Log.e(TAG, "Not connected, cannot publish");
            return;
        }
        executor.execute(new Runnable() {
            @Override
            public void run() {
                try {
                    MqttMessage message = new MqttMessage(payload.getBytes("UTF-8"));
                    message.setQos(qos);
                    message.setRetained(false); // retain必须为0
                    mqttClient.publish(topic, message);
                    Log.i(TAG, "Published to " + topic + ": " + payload);
                } catch (Exception e) {
                    Log.e(TAG, "Publish failed", e);
                }
            }
        });
    }

    public boolean isConnected() {
        return mqttClient != null && mqttClient.isConnected();
    }
}