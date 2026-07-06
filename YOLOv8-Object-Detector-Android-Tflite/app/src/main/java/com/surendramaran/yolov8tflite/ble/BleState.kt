package com.surendramaran.yolov8tflite.ble

enum class BleState {
    DISCONNECTED,
    SCANNING,
    CONNECTING,
    CONNECTED,
    READY
}