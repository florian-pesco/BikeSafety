package com.surendramaran.yolov8tflite.ble

import java.util.UUID

object BleConstants {

    const val DEVICE_NAME = "BikeSafetyESP"

    val SERVICE_UUID =
        UUID.fromString("12345678-1234-1234-1234-1234567890ab")

    val CHARACTERISTIC_UUID =
        UUID.fromString("87654321-4321-4321-4321-ba0987654321")
}