package com.surendramaran.yolov8tflite.wifi

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.util.Log
import com.surendramaran.yolov8tflite.tracker.ThreatLevel
import java.net.HttpURLConnection
import java.net.URL
import java.util.concurrent.ExecutorService
import java.util.concurrent.Executors

class WifiEspManager(context: Context) {

    private val executor: ExecutorService =
        Executors.newSingleThreadExecutor()

    private val connectivityManager =
        context.applicationContext.getSystemService(
            ConnectivityManager::class.java
        )

    private val wifiNetworkCallback =
        object : ConnectivityManager.NetworkCallback() {
            override fun onAvailable(network: Network) {
                connectivityManager.bindProcessToNetwork(network)
                statusText = "WiFi: connected, waiting for ESP"
                Log.d(TAG, "Bound app traffic to WiFi network")
            }

            override fun onLost(network: Network) {
                connectivityManager.bindProcessToNetwork(null)
                statusText =
                    "WiFi: connect to ${WifiConstants.ESP_SSID}"
                Log.d(TAG, "WiFi network lost")
            }
        }

    @Volatile
    private var statusText =
        "WiFi: connect to ${WifiConstants.ESP_SSID}"

    @Volatile
    private var lastSentThreat: ThreatLevel? = null

    @Volatile
    private var lastSendSucceeded = false

    @Volatile
    private var lastAttemptMillis = 0L

    init {
        val request = NetworkRequest.Builder()
            .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
            .build()

        connectivityManager.registerNetworkCallback(
            request,
            wifiNetworkCallback
        )

        connectivityManager.allNetworks
            .firstOrNull { network ->
                connectivityManager
                    .getNetworkCapabilities(network)
                    ?.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) == true
            }
            ?.let { network ->
                connectivityManager.bindProcessToNetwork(network)
                statusText = "WiFi: connected, waiting for ESP"
                Log.d(TAG, "Bound app traffic to existing WiFi network")
            }
    }

    fun sendThreat(level: ThreatLevel) {
        val now = System.currentTimeMillis()

        if (
            level == lastSentThreat &&
            lastSendSucceeded
        ) {
            return
        }

        if (
            level == lastSentThreat &&
            !lastSendSucceeded &&
            now - lastAttemptMillis < RETRY_DELAY_MS
        ) {
            return
        }

        lastSentThreat = level
        lastAttemptMillis = now

        executor.execute {
            postThreat(level)
        }
    }

    fun getStatusText(): String = statusText

    fun shutdown() {
        runCatching {
            connectivityManager.unregisterNetworkCallback(
                wifiNetworkCallback
            )
        }

        connectivityManager.bindProcessToNetwork(null)
        executor.shutdownNow()
    }

    private fun postThreat(level: ThreatLevel) {
        val numericLevel = level.toProtocolValue()
        val endpoint =
            "${WifiConstants.ESP_BASE_URL}/api/threat?level=$numericLevel"

        Log.d(TAG, "POST $endpoint")
        statusText = "WiFi: sending ${level.name}"

        var connection: HttpURLConnection? = null

        try {
            connection = URL(endpoint)
                .openConnection() as HttpURLConnection

            connection.requestMethod = "POST"
            connection.connectTimeout = CONNECT_TIMEOUT_MS
            connection.readTimeout = READ_TIMEOUT_MS
            connection.useCaches = false
            connection.doInput = true

            val code = connection.responseCode
            val body = if (code in 200..299) {
                connection.inputStream.bufferedReader().use {
                    it.readText()
                }
            } else {
                connection.errorStream?.bufferedReader()?.use {
                    it.readText()
                }.orEmpty()
            }

            if (code in 200..299) {
                lastSendSucceeded = true
                statusText = "WiFi: ${level.name} sent"
                Log.d(TAG, "ESP response $code: $body")
            } else {
                lastSendSucceeded = false
                statusText = "WiFi error $code"
                Log.e(TAG, "ESP error $code: $body")
            }

        } catch (error: Exception) {
            lastSendSucceeded = false
            statusText = "WiFi: ESP not reachable"
            Log.e(TAG, "Failed to send threat ${level.name}", error)

        } finally {
            connection?.disconnect()
        }
    }

    private fun ThreatLevel.toProtocolValue(): Int =
        when (this) {
            ThreatLevel.SAFE -> 0
            ThreatLevel.APPROACHING -> 1
            ThreatLevel.DANGER -> 2
        }

    companion object {
        private const val TAG = "WiFiESP"
        private const val CONNECT_TIMEOUT_MS = 700
        private const val READ_TIMEOUT_MS = 700
        private const val RETRY_DELAY_MS = 1000L
    }
}
