package com.example.esp8266_app;

import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.app.AlertDialog;
import android.widget.Button;
import android.widget.EditText;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;
import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken;
import org.json.JSONException;
import org.json.JSONObject;

public class MainActivity extends AppCompatActivity {

    private static final String TAG = "MainActivity";

    // 设备信息（根据您提供）
    private static final String PRODUCT_ID = "e2F2Jz2iUU";
    private static final String DEVICE_NAME = "app";
    private static final String DEVICE_KEY = "WDZ4TkZHdlNjdTU1c3FhMkJRSlJpSGo4TlFuUjdFMlI=";

    // MQTT Broker (根据PDF使用通用接入点)
    private static final String BROKER_TCP = "tcp://mqtts.heclouds.com:1883"; // 非加密

    private TextView tvConnectionStatus, tvWindSpeed, tvTemperature, tvHumidity;
    private Button btnConnect, btnDisconnect, btnInfoPanel, btnWindMinus, btnWindPlus;
    private Button btnCountdownMinus, btnCountdownPlus;
    private RadioGroup radioMode;
    private RadioButton radioManual, radioAuto, radioTimer;
    private EditText etCountdown;
    private TextView tvResult; // 用于存储信息文本，在浮窗中显示

    private MqttHelper mqttHelper;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        initViews();
        setupListeners();
    }

    private void initViews() {
        tvConnectionStatus = findViewById(R.id.tvConnectionStatus);
        btnConnect = findViewById(R.id.btnConnect);
        btnDisconnect = findViewById(R.id.btnDisconnect);
        btnInfoPanel = findViewById(R.id.btnInfoPanel);
        radioMode = findViewById(R.id.radioMode);
        radioManual = findViewById(R.id.radioManual);
        radioAuto = findViewById(R.id.radioAuto);
        radioTimer = findViewById(R.id.radioTimer);
        etCountdown = findViewById(R.id.etCountdown);
        btnCountdownMinus = findViewById(R.id.btnCountdownMinus);
        btnCountdownPlus = findViewById(R.id.btnCountdownPlus);
        btnWindMinus = findViewById(R.id.btnWindMinus);
        btnWindPlus = findViewById(R.id.btnWindPlus);
        tvWindSpeed = findViewById(R.id.tvWindSpeed);
        tvTemperature = findViewById(R.id.tvTemperature);
        tvHumidity = findViewById(R.id.tvHumidity);
        
        // 初始化信息文本
        tvResult = new TextView(this);
        tvResult.setText("结果反馈");
    }

    private void setupListeners() {
        btnConnect.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                connectMqtt();
            }
        });

        btnDisconnect.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                disconnectMqtt();
            }
        });

        btnInfoPanel.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                showInfoPanel();
            }
        });

        radioMode.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(RadioGroup group, int checkedId) {
                if (mqttHelper != null && mqttHelper.isConnected()) {
                    reportProperties();
                }
            }
        });

        btnCountdownMinus.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                adjustCountdown(-1);
            }
        });

        btnCountdownPlus.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                adjustCountdown(1);
            }
        });

        btnWindMinus.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                adjustWindSpeed(-1);
            }
        });

        btnWindPlus.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                adjustWindSpeed(1);
            }
        });
    }

    private void connectMqtt() {
        // 生成Token（使用sha256，过期时间1小时）
        String token = TokenUtil.generateToken(PRODUCT_ID, DEVICE_NAME, DEVICE_KEY, 3600, "sha256");
        if (token == null) {
            Toast.makeText(this, "Token生成失败", Toast.LENGTH_SHORT).show();
            return;
        }
        Log.i(TAG, "Generated token: " + token);

        mqttHelper = new MqttHelper(BROKER_TCP, DEVICE_NAME, PRODUCT_ID, token,
                new MqttHelper.MqttConnectionListener() {
                    @Override
                    public void onConnectSuccess() {
                        runOnUiThread(() -> {
                            tvConnectionStatus.setText("已连接");
                            tvConnectionStatus.setTextColor(getColor(android.R.color.holo_green_dark));
                            btnConnect.setEnabled(false);
                            btnDisconnect.setEnabled(true);
                            radioMode.setEnabled(true);
                            etCountdown.setEnabled(true);
                            btnCountdownMinus.setEnabled(true);
                            btnCountdownPlus.setEnabled(true);
                            btnWindMinus.setEnabled(true);
                            btnWindPlus.setEnabled(true);
                            tvResult.append("\n连接成功");
                        });
                    }

                    @Override
                    public void onConnectFailed(Throwable reason) {
                        runOnUiThread(() -> {
                            tvConnectionStatus.setText("连接失败");
                            tvConnectionStatus.setTextColor(getColor(android.R.color.holo_red_dark));
                            tvResult.append("\n连接失败: " + reason.getMessage());
                        });
                    }

                    @Override
                    public void onDisconnect() {
                        runOnUiThread(() -> {
                            tvConnectionStatus.setText("未连接");
                            tvConnectionStatus.setTextColor(getColor(android.R.color.darker_gray));
                            btnConnect.setEnabled(true);
                            btnDisconnect.setEnabled(false);
                            radioMode.setEnabled(false);
                            etCountdown.setEnabled(false);
                            btnCountdownMinus.setEnabled(false);
                            btnCountdownPlus.setEnabled(false);
                            btnWindMinus.setEnabled(false);
                            btnWindPlus.setEnabled(false);
                            tvResult.append("\n已断开");
                        });
                    }

                    @Override
                    public void onMessageArrived(String topic, String message) {
                        runOnUiThread(() -> {
                            tvResult.append("\n收到消息 [" + topic + "]: " + message);
                            
                            // 如果是属性设置消息，解析并显示详细信息
                            if (topic.equals("$sys/" + PRODUCT_ID + "/" + DEVICE_NAME + "/thing/property/set")) {
                                parseAndDisplayPropertySet(message);
                            }
                        });
                    }

                    @Override
                    public void onDeliveryComplete(IMqttDeliveryToken token) {
                        runOnUiThread(() -> {
                            tvResult.append("\n消息发送完成");
                        });
                    }
                });

        mqttHelper.connect();
        tvConnectionStatus.setText("正在连接...");
    }

    private void disconnectMqtt() {
        if (mqttHelper != null) {
            mqttHelper.disconnect();
        }
    }

    /**
     * 调整风速（0-100范围）
     */
    private void adjustWindSpeed(int delta) {
        try {
            String currentSpeedStr = tvWindSpeed.getText().toString();
            int currentSpeed = Integer.parseInt(currentSpeedStr);
            int newSpeed = currentSpeed + delta;
            
            // 限制在0-100范围内
            if (newSpeed < 0) newSpeed = 0;
            if (newSpeed > 100) newSpeed = 100;
            
            tvWindSpeed.setText(String.valueOf(newSpeed));
            
            if (mqttHelper != null && mqttHelper.isConnected()) {
                reportProperties();
            }
        } catch (NumberFormatException e) {
            tvWindSpeed.setText("0");
        }
    }

    /**
     * 调整倒计时（0-9999范围）
     */
    private void adjustCountdown(int delta) {
        try {
            String currentStr = etCountdown.getText().toString();
            int current = Integer.parseInt(currentStr);
            int newCountdown = current + delta;

            if (newCountdown < 0) newCountdown = 0;
            if (newCountdown > 9999) newCountdown = 9999;

            etCountdown.setText(String.valueOf(newCountdown));

            if (mqttHelper != null && mqttHelper.isConnected()) {
                reportProperties();
            }
        } catch (NumberFormatException e) {
            etCountdown.setText("0");
        }
    }

    /**
     * 显示信息浮窗
     */
    private void showInfoPanel() {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle("信息窗口");
        
        // 创建自定义布局的视图
        View dialogView = getLayoutInflater().inflate(R.layout.dialog_info_panel, null);
        
        // 获取浮窗中的TextView
        TextView tvDialogResult = dialogView.findViewById(R.id.tvResult);
        tvDialogResult.setText(tvResult.getText());
        
        // 设置关闭按钮
        Button btnClose = dialogView.findViewById(R.id.btnClose);
        btnClose.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                // 关闭对话框时更新主文本内容
                tvResult.setText(tvDialogResult.getText());
                ((AlertDialog) v.getTag()).dismiss();
            }
        });
        
        builder.setView(dialogView);
        AlertDialog dialog = builder.create();
        
        // 为关闭按钮设置对话框引用
        btnClose.setTag(dialog);
        
        dialog.show();
    }

    /**
     * 上报属性（countdown/mode/speed 嵌套格式）
     */
    private void reportProperties() {
        if (mqttHelper == null || !mqttHelper.isConnected()) {
            Toast.makeText(this, "请先连接MQTT", Toast.LENGTH_SHORT).show();
            return;
        }

        try {
            JSONObject payload = new JSONObject();
            payload.put("id", String.valueOf(System.currentTimeMillis()).substring(0, 13));
            payload.put("version", "1.0");

            JSONObject params = new JSONObject();

            // 倒计时 (int32) — 嵌套 {"countdown": {"value": 11}}
            try {
                int countdown = Integer.parseInt(etCountdown.getText().toString());
                JSONObject cdObj = new JSONObject();
                cdObj.put("value", countdown);
                params.put("countdown", cdObj);
            } catch (NumberFormatException e) {
                Toast.makeText(this, "倒计时解析错误", Toast.LENGTH_SHORT).show();
                return;
            }

            // 模式 (int32) — 嵌套 {"mode": {"value": 1}}
            int mode;
            int checkedId = radioMode.getCheckedRadioButtonId();
            if (checkedId == R.id.radioManual) {
                mode = 0;
            } else if (checkedId == R.id.radioAuto) {
                mode = 1;
            } else if (checkedId == R.id.radioTimer) {
                mode = 2;
            } else {
                mode = 0;
            }
            JSONObject modeObj = new JSONObject();
            modeObj.put("value", mode);
            params.put("mode", modeObj);

            // 风速 (int32) — 嵌套 {"speed": {"value": 22}}
            try {
                int windSpeed = Integer.parseInt(tvWindSpeed.getText().toString());
                JSONObject windObj = new JSONObject();
                windObj.put("value", windSpeed);
                params.put("speed", windObj);
            } catch (NumberFormatException e) {
                Toast.makeText(this, "风速解析错误", Toast.LENGTH_SHORT).show();
                return;
            }

            payload.put("params", params);

            String propertyTopic = "$sys/" + PRODUCT_ID + "/" + DEVICE_NAME + "/thing/property/post";
            mqttHelper.publish(propertyTopic, payload.toString(), 0);

            String[] modeNames = {"手动", "自动", "定时"};
            tvResult.append("\n模式(" + modeNames[mode] + ") 倒计时(" + etCountdown.getText() + ") 风速(" + tvWindSpeed.getText() + ") 上报已发送");

        } catch (JSONException e) {
            e.printStackTrace();
            Toast.makeText(this, "属性上报JSON构造失败", Toast.LENGTH_SHORT).show();
        }
    }

    // 解析属性设置JSON（扁平格式）并显示
    private void parseAndDisplayPropertySet(String json) {
        try {
            JSONObject root = new JSONObject(json);
            String id = root.optString("id", "");
            JSONObject params = root.optJSONObject("params");
            
            if (params != null) {
                StringBuilder sb = new StringBuilder("收到平台设置：");

                // 模式 mode (int32)：0手动 1自动 2定时
                if (params.has("mode")) {
                    int mode = params.getInt("mode");
                    sb.append("\n  mode = ").append(mode);
                    if (mode == 0) {
                        radioMode.check(R.id.radioManual);
                    } else if (mode == 1) {
                        radioMode.check(R.id.radioAuto);
                    } else if (mode == 2) {
                        radioMode.check(R.id.radioTimer);
                    }
                    tvResult.append("\n模式已更新: " + mode);
                }

                // 倒计时 countdown (int32)：0-9999 分钟
                if (params.has("countdown")) {
                    int countdown = params.getInt("countdown");
                    sb.append("\n  countdown = ").append(countdown);
                    etCountdown.setText(String.valueOf(countdown));
                    tvResult.append("\n倒计时已更新: " + countdown);
                }

                // 风速 speed (int32)：0-100
                if (params.has("speed")) {
                    int speed = params.getInt("speed");
                    sb.append("\n  speed = ").append(speed);
                    tvWindSpeed.setText(String.valueOf(speed));
                    tvResult.append("\n风速已更新: " + speed);
                }

                // 温度 temp (int32)：0-50
                if (params.has("temp")) {
                    int temp = params.getInt("temp");
                    sb.append("\n  temp = ").append(temp);
                    tvTemperature.setText(temp + "°C");
                    tvResult.append("\n温度已更新: " + temp + "°C");
                }

                // 湿度 humi (int32)：20-90
                if (params.has("humi")) {
                    int humi = params.getInt("humi");
                    sb.append("\n  humi = ").append(humi);
                    tvHumidity.setText(humi + "%");
                    tvResult.append("\n湿度已更新: " + humi + "%");
                }

                tvResult.append("\n" + sb.toString());
                sendPropertySetResponse(id);
            } else {
                tvResult.append("\n属性设置消息中无params字段");
                sendPropertySetResponse(id);
            }
        } catch (JSONException e) {
            Log.e(TAG, "解析属性设置消息失败", e);
            tvResult.append("\n解析失败: " + e.getMessage());
        }
    }

    /**
     * 发送属性设置响应到平台
     * @param requestId 从请求消息中提取的id
     */
    private void sendPropertySetResponse(String requestId) {
        try {
            // 构造响应JSON
            JSONObject response = new JSONObject();
            // 如果请求id为空或无效，生成一个13位数字id（备用）
            String respId = (requestId != null && !requestId.isEmpty()) ? requestId : 
                    String.valueOf(System.currentTimeMillis()).substring(0, 13);
            response.put("id", respId);
            response.put("code", 200);
            response.put("msg", "success");
            
            String responseTopic = "$sys/" + PRODUCT_ID + "/" + DEVICE_NAME + "/thing/property/set_reply";
            mqttHelper.publish(responseTopic, response.toString(), 0);
            
            tvResult.append("\n已发送响应: " + response.toString());
        } catch (JSONException e) {
            Log.e(TAG, "构造响应JSON失败", e);
            tvResult.append("\n响应构造失败: " + e.getMessage());
        }
    }

    @Override
    protected void onDestroy() {
        if (mqttHelper != null && mqttHelper.isConnected()) {
            mqttHelper.disconnect();
        }
        super.onDestroy();
    }
}