package com.example.esp8266_app;

import android.util.Base64;
import java.io.UnsupportedEncodingException;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;

public class TokenUtil {

    private static final String VERSION = "2018-10-31";

    /**
     * 生成OneNET平台的访问Token
     * @param productId 产品ID
     * @param deviceName 设备名称
     * @param deviceKey 设备密钥（Base64字符串）
     * @param expireSeconds 过期时间（秒），例如3600
     * @param method 签名方法：md5/sha1/sha256
     * @return 完整的token字符串（已URL编码）
     */
    public static String generateToken(String productId, String deviceName, String deviceKey,
                                       long expireSeconds, String method) {
        try {
            long et = System.currentTimeMillis() / 1000 + expireSeconds; // 过期时间戳
            String res = "products/" + productId + "/devices/" + deviceName; // 设备级资源

            // 构造待签名字符串
            String stringToSign = et + "\n" + method + "\n" + res + "\n" + VERSION;

            // 对设备密钥进行Base64解码
            byte[] keyBytes = Base64.decode(deviceKey, Base64.DEFAULT);

            // 计算HMAC签名
            String hmacAlgorithm;
            switch (method.toLowerCase()) {
                case "sha1": hmacAlgorithm = "HmacSHA1"; break;
                case "sha256": hmacAlgorithm = "HmacSHA256"; break;
                case "md5": hmacAlgorithm = "HmacMD5"; break;
                default: throw new IllegalArgumentException("Unsupported method: " + method);
            }
            Mac mac = Mac.getInstance(hmacAlgorithm);
            SecretKeySpec keySpec = new SecretKeySpec(keyBytes, hmacAlgorithm);
            mac.init(keySpec);
            byte[] signBytes = mac.doFinal(stringToSign.getBytes("UTF-8"));

            // Base64编码签名
            String sign = Base64.encodeToString(signBytes, Base64.NO_WRAP);

            // 构建token键值对并进行URL编码
            StringBuilder token = new StringBuilder();
            appendEncoded(token, "version", VERSION);
            token.append("&");
            appendEncoded(token, "res", res);
            token.append("&");
            appendEncoded(token, "et", String.valueOf(et));
            token.append("&");
            appendEncoded(token, "method", method);
            token.append("&");
            appendEncoded(token, "sign", sign);

            return token.toString();

        } catch (NoSuchAlgorithmException | InvalidKeyException | UnsupportedEncodingException e) {
            e.printStackTrace();
            return null;
        }
    }

    /**
     * 按照PDF要求进行URL编码（仅编码特殊字符：+ 空格 / ? % # & =）
     * 其余字符保持不变，避免过度编码。
     */
    private static void appendEncoded(StringBuilder sb, String key, String value) {
        sb.append(key).append("=").append(encodeValue(value));
    }

    private static String encodeValue(String s) {
        if (s == null) return "";
        StringBuilder sb = new StringBuilder();
        for (char c : s.toCharArray()) {
            switch (c) {
                case '+': sb.append("%2B"); break;
                case ' ': sb.append("%20"); break;
                case '/': sb.append("%2F"); break;
                case '?': sb.append("%3F"); break;
                case '%': sb.append("%25"); break;
                case '#': sb.append("%23"); break;
                case '&': sb.append("%26"); break;
                case '=': sb.append("%3D"); break;
                default: sb.append(c); break;
            }
        }
        return sb.toString();
    }
}