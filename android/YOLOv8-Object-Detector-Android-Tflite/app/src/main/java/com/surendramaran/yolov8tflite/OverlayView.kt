package com.surendramaran.yolov8tflite

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Rect
import android.util.AttributeSet
import android.view.View
import androidx.core.content.ContextCompat
import com.surendramaran.yolov8tflite.tracker.ThreatLevel
import yolov8tflite.R

import com.surendramaran.yolov8tflite.tracker.TrackedVehicle

class OverlayView(context: Context?, attrs: AttributeSet?) : View(context, attrs) {

    private var results = listOf<TrackedVehicle>()
    private var boxPaint = Paint()
    private var textBackgroundPaint = Paint()
    private var textPaint = Paint()

    private var bounds = Rect()

    init {
        initPaints()
    }

    fun clear() {
        results = listOf()
        textPaint.reset()
        textBackgroundPaint.reset()
        boxPaint.reset()
        invalidate()
        initPaints()
    }

    private fun initPaints() {
        textBackgroundPaint.color = Color.BLACK
        textBackgroundPaint.style = Paint.Style.FILL
        textBackgroundPaint.textSize = 50f

        textPaint.color = Color.WHITE
        textPaint.style = Paint.Style.FILL
        textPaint.textSize = 50f

        boxPaint.color = ContextCompat.getColor(context!!, R.color.bounding_box_color)
        boxPaint.strokeWidth = 8F
        boxPaint.style = Paint.Style.STROKE
    }

    override fun draw(canvas: Canvas) {
        super.draw(canvas)

        results.forEach {track ->
            val box = track.box

            val left = box.x1 * width
            val top = box.y1 * height
            val right = box.x2 * width
            val bottom = box.y2 * height

            boxPaint.color = when(track.threatLevel) {

                ThreatLevel.SAFE ->
                    Color.rgb(76,175,80)

                ThreatLevel.APPROACHING ->
                    Color.rgb(255,193,7)

                ThreatLevel.DANGER ->
                    Color.RED
            }

            canvas.drawRoundRect(
                left,
                top,
                right,
                bottom,
                18f,
                18f,
                boxPaint
            )

            val status = when(track.threatLevel) {

                ThreatLevel.SAFE ->
                    "SAFE"

                ThreatLevel.APPROACHING ->
                    "⚠ APPROACHING"

                ThreatLevel.DANGER ->
                    "🚨 DANGER"
            }

            val drawableText =
                "${track.box.clsName.uppercase()}\n" +
                        "ID: ${track.id}\n" +
                        status

            val lines = drawableText.split("\n")

            var maxWidth = 0f

            lines.forEach {
                maxWidth = maxOf(maxWidth, textPaint.measureText(it))
            }

            val lineHeight = textPaint.textSize + 8f

            canvas.drawRoundRect(
                left,
                top,
                left + maxWidth + 20f,
                top + lineHeight * lines.size + 20f,
                12f,
                12f,
                textBackgroundPaint
            )

            lines.forEachIndexed { index, line ->
                canvas.drawText(
                    line,
                    left + 10f,
                    top + lineHeight * (index + 1),
                    textPaint
                )
            }

        }
    }

    fun setResults(trackedVehicles: List<TrackedVehicle>) {
        results = trackedVehicles
        invalidate()
    }

    companion object {
        private const val BOUNDING_RECT_TEXT_PADDING = 8
    }
}