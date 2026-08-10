package com.surendramaran.yolov8tflite.tracker

class ThreatEvaluator {

    fun evaluate(track: TrackedVehicle): ThreatLevel {

        if (track.areaHistory.size < 5)
            return ThreatLevel.SAFE

        val first = track.areaHistory.first()
        val last = track.areaHistory.last()

        val growth = last / first

        return when {

            growth > 1.40f ->
                ThreatLevel.DANGER

            growth > 1.15f ->
                ThreatLevel.APPROACHING

            else ->
                ThreatLevel.SAFE
        }
    }
}