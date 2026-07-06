package com.surendramaran.yolov8tflite.ble

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.util.Log
import android.os.Handler
import android.os.Looper
import androidx.annotation.RequiresPermission
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothProfile
import android.bluetooth.BluetoothGattCharacteristic
import com.surendramaran.yolov8tflite.tracker.ThreatLevel

class BleManager(
    private val context: Context
){

    private val bluetoothManager =
        context.getSystemService(BluetoothManager::class.java)

    private val bluetoothAdapter: BluetoothAdapter? =
        bluetoothManager.adapter

    private val scanner
        get() = bluetoothAdapter?.bluetoothLeScanner

    private var bluetoothGatt: BluetoothGatt? = null

    private var threatCharacteristic: BluetoothGattCharacteristic? = null

    private var state = BleState.DISCONNECTED

    private val handler = Handler(Looper.getMainLooper())

    private val scanDuration = 5000L
    private val retryDelay = 2000L

    private val targetDeviceName = BleConstants.DEVICE_NAME

    private var lastThreat: ThreatLevel? = null

    fun isBluetoothEnabled(): Boolean {
        return bluetoothAdapter?.isEnabled == true
    }

    @SuppressLint("MissingPermission")
    fun startScan() {

        Log.d("BLE", "startScan() called")

        if (!isBluetoothEnabled()) {
            Log.d("BLE", "Bluetooth OFF")
            return
        }

        state = BleState.SCANNING

        Log.d("BLE", "Starting BLE scan")

        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()

        scanner?.startScan(null, settings, scanCallback)

        handler.postDelayed({

            if (state == BleState.SCANNING) {

                Log.d("BLE", "Scan timeout")

                stopScan()

            }

        }, scanDuration)
    }

    @SuppressLint("MissingPermission")
    private fun stopScan() {

        scanner?.stopScan(scanCallback)

        state = BleState.DISCONNECTED

        Log.d("BLE", "Scan stopped")

        handler.postDelayed({

            Log.d("BLE", "Restarting scan")

            startScan()

        }, retryDelay)

    }

    @SuppressLint("MissingPermission")
    private fun connect(device: BluetoothDevice) {

        Log.d("BLE", "Connecting to ${device.address}")

        bluetoothGatt = device.connectGatt(
            context,
            false,
            gattCallback
        )
    }

    @SuppressLint("MissingPermission")
    fun sendThreat(level: ThreatLevel) {


        if (state != BleState.READY)
            return

        if (level == lastThreat) return
        lastThreat = level

        val value = when (level) {

            ThreatLevel.SAFE -> 0

            ThreatLevel.APPROACHING -> 1

            ThreatLevel.DANGER -> 2
        }

        val characteristic = threatCharacteristic ?: return

        characteristic.value = byteArrayOf(value.toByte())

        bluetoothGatt?.writeCharacteristic(characteristic)

        Log.d("BLE", "Sent threat: $value")
    }

    private val scanCallback = object : ScanCallback() {

        @RequiresPermission(allOf = [Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT])
        override fun onScanResult(
            callbackType: Int,
            result: ScanResult
        ) {

            Log.d("BLE", "onScanResult()")

            val device = result.device

            val advertisedName = result.scanRecord?.deviceName
            val deviceName = device.name

            Log.d(
                "BLE",
                """
                Device:
                Address: ${device.address}
                Device.name: $deviceName
                Advertised: $advertisedName
                RSSI: ${result.rssi}
                """.trimIndent()
            )

            if (advertisedName == targetDeviceName ||
                deviceName == targetDeviceName) {

                Log.d("BLE", "Target ESP found!")

                scanner?.stopScan(this)

                handler.removeCallbacksAndMessages(null)

                state = BleState.CONNECTING

                Log.d("BLE", "Connecting...")

                connect(device)
            }
        }

        override fun onBatchScanResults(results: MutableList<ScanResult>) {
            Log.d("BLE", "Batch scan: ${results.size} devices")
        }

        override fun onScanFailed(errorCode: Int) {
            Log.e("BLE", "Scan failed: $errorCode")
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {

        override fun onConnectionStateChange(
            gatt: BluetoothGatt,
            status: Int,
            newState: Int
        ) {

            when (newState) {

                BluetoothProfile.STATE_CONNECTED -> {

                    Log.d("BLE", "Connected!")

                    state = BleState.CONNECTED

                    gatt.discoverServices()
                }

                BluetoothProfile.STATE_DISCONNECTED -> {

                    Log.d("BLE", "Disconnected")

                    state = BleState.DISCONNECTED

                    startScan()
                }
            }
        }

        override fun onServicesDiscovered(
            gatt: BluetoothGatt,
            status: Int
        ) {

            Log.d("BLE", "Services discovered")

            val service =
                gatt.getService(BleConstants.SERVICE_UUID)

            if (service == null) {

                Log.e("BLE", "Service not found")

                return
            }

            threatCharacteristic =
                service.getCharacteristic(
                    BleConstants.CHARACTERISTIC_UUID
                )

            if (threatCharacteristic == null) {

                Log.e("BLE", "Characteristic not found")

                return
            }

            state = BleState.READY

            Log.d("BLE", "BLE READY")
        }
    }
}