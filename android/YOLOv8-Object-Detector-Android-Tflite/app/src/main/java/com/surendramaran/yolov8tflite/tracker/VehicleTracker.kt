package com.surendramaran.yolov8tflite.tracker

import com.surendramaran.yolov8tflite.BoundingBox
import kotlin.math.max
import kotlin.math.min

class VehicleTracker {

    private val threatEvaluator = ThreatEvaluator()

    private val tracks = mutableListOf<TrackedVehicle>()
    private var nextId = 0

    fun update(detections: List<BoundingBox>): List<TrackedVehicle> {

        // Increase age
        tracks.forEach {
            it.age++
            it.missedFrames++
        }

        for (detection in detections) {

            val match = tracks.maxByOrNull {
                iou(it.box, detection)
            }

            if (match != null && iou(match.box, detection) > 0.3f) {

                match.box = detection
                match.missedFrames = 0

                val area = detection.w * detection.h

                match.areaHistory.add(area)

                match.threatLevel =
                    threatEvaluator.evaluate(match)

                if (match.areaHistory.size > 10) {
                    match.areaHistory.removeAt(0)
                }

            } else {
                val area = detection.w * detection.h
                tracks.add(
                    TrackedVehicle(
                        id = nextId++,
                        box = detection,
                        areaHistory = mutableListOf(area)
                    )
                )
            }
        }

        tracks.removeAll { it.missedFrames > 10 }

        return tracks
    }


    private fun iou(a: BoundingBox, b: BoundingBox): Float {

        val left = max(a.x1, b.x1)
        val top = max(a.y1, b.y1)
        val right = min(a.x2, b.x2)
        val bottom = min(a.y2, b.y2)

        if (right <= left || bottom <= top)
            return 0f

        val intersection = (right - left) * (bottom - top)

        val areaA = (a.x2 - a.x1) * (a.y2 - a.y1)
        val areaB = (b.x2 - b.x1) * (b.y2 - b.y1)

        return intersection / (areaA + areaB - intersection)
    }
}