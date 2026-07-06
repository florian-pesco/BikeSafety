package com.surendramaran.yolov8tflite.tracker

import com.surendramaran.yolov8tflite.BoundingBox

data class TrackedVehicle(

    val id: Int,

    var box: BoundingBox,

    var age: Int = 0,

    var missedFrames: Int = 0,

    val areaHistory: MutableList<Float> = mutableListOf(),

    var threatLevel: ThreatLevel = ThreatLevel.SAFE

)