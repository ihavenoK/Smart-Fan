package com.chinamobile.iot.pulsar.auth;

import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;
import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;
import java.security.InvalidKeyException;
import java.security.NoSuchAlgorithmException;
import java.util.Base64;

public class TokenUtil {

    /**
     * 生成 authorization 头
     * @param version 版本，固定为 "2022-05-01"
     * @param resourceName 资源名称，如 "products/{productId}"
     * @param expirationTime 过期时间（秒级时间戳），例如 System.currentTimeMillis()/1000 + 3600
     * @param signatureMethod 签名方法，如 "sha1"
     * @param accessKey 从平台获取的 accessKey（需Base64解码）
     * @return authorization 字符串
     */
    public static String assembleToken(String version, String resourceName, String expirationTime,
                                        String signatureMethod, String accessKey)
            throws UnsupportedEncodingException, NoSuchAlgorithmException, InvalidKeyException {
        StringBuilder sb = new StringBuilder();
        String res = URLEncoder.encode(resourceName, "UTF-8");
        String sig = URLEncoder.encode(generatorSignature(version, resourceName, expirationTime, accessKey, signatureMethod), "UTF-8");
        sb.append("version=").append(version)
                .append("&res=").append(res)
                .append("&et=").append(expirationTime)
                .append("&method=").append(signatureMethod)
                .append("&sign=").append(sig);
        return sb.toString();
    }

    public static String generatorSignature(String version, String resourceName, String expirationTime,
                                             String accessKey, String signatureMethod)
            throws NoSuchAlgorithmException, InvalidKeyException {
        String encryptText = expirationTime + "\n" + signatureMethod + "\n" + resourceName + "\n" + version;
        byte[] bytes = HmacEncrypt(encryptText, accessKey, signatureMethod);
        return Base64.getEncoder().encodeToString(bytes);
    }

    public static byte[] HmacEncrypt(String data, String key, String signatureMethod)
            throws NoSuchAlgorithmException, InvalidKeyException {
        // key 是 Base64 字符串，需要先解码
        byte[] keyBytes = Base64.getDecoder().decode(key);
        SecretKeySpec signKey = new SecretKeySpec(keyBytes, "Hmac" + signatureMethod.toUpperCase());
        Mac mac = Mac.getInstance("Hmac" + signatureMethod.toUpperCase());
        mac.init(signKey);
        return mac.doFinal(data.getBytes());
    }
}