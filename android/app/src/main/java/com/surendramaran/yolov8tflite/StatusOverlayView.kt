package com.surendramaran.yolov8tflite

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.util.AttributeSet
import android.view.View

class StatusOverlayView(
    context: Context,
    attrs: AttributeSet?
) : View(context, attrs) {

    private val statusPaint = Paint().apply {
        color = Color.WHITE
        textSize = 32f
        isAntiAlias = true
    }

    private val backgroundPaint = Paint().apply {
        color = Color.argb(140, 0, 0, 0)
    }

    var bleStatus = "🔴 Handlebar not found"

    var fps = 0f

    var inference = 0L

    var objects = 0

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        canvas.drawRect(
            0f,
            0f,
            width.toFloat(),
            70f,
            backgroundPaint
        )

        canvas.drawText(
            bleStatus,
            20f,
            28f,
            statusPaint
        )

        canvas.drawText(
            "FPS: %.1f | %d ms | Objects: %d"
                .format(fps, inference, objects),
            20f,
            62f,
            statusPaint
        )
    }
}