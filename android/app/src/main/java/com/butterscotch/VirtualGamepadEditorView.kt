package com.butterscotch

import android.app.AlertDialog
import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.view.MotionEvent
import android.view.View
import android.widget.EditText
import android.widget.LinearLayout
import kotlin.math.hypot

class VirtualGamepadEditorView(context: Context, val layoutConfig: VirtualGamepadLayout) : View(context) {

    private val buttonPaint = Paint().apply {
        color = Color.argb(128, 0, 255, 0)
        style = Paint.Style.FILL
        isAntiAlias = true
    }
    
    private val buttonStrokePaint = Paint().apply {
        color = Color.argb(255, 255, 255, 255)
        style = Paint.Style.STROKE
        strokeWidth = 4f
        isAntiAlias = true
    }

    private val textPaint = Paint().apply {
        color = Color.WHITE
        textSize = 40f
        textAlign = Paint.Align.CENTER
        isAntiAlias = true
    }

    private val joystickBasePaint = Paint().apply {
        color = Color.argb(100, 0, 255, 0)
        style = Paint.Style.FILL
        isAntiAlias = true
    }

    // Dragging state
    private var draggedButton: VirtualButton? = null
    private var draggedJoystick: VirtualJoystick? = null
    private var lastTouchX = 0f
    private var lastTouchY = 0f
    private var isDragging = false

    var onButtonDoubleTapped: ((VirtualButton) -> Unit)? = null
    var onJoystickDoubleTapped: ((VirtualJoystick) -> Unit)? = null

    init {
        setWillNotDraw(false)
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        // Draw Joysticks
        for (js in layoutConfig.joysticks) {
            canvas.drawCircle(js.x, js.y, js.radius, joystickBasePaint)
            canvas.drawCircle(js.x, js.y, js.radius, buttonStrokePaint)
            canvas.drawText("JOYSTICK", js.x, js.y, textPaint)
        }

        // Draw Buttons
        for (btn in layoutConfig.buttons) {
            canvas.drawCircle(btn.x, btn.y, btn.radius, buttonPaint)
            canvas.drawCircle(btn.x, btn.y, btn.radius, buttonStrokePaint)

            val textY = btn.y - ((textPaint.descent() + textPaint.ascent()) / 2)
            canvas.drawText(btn.label, btn.x, textY, textPaint)
        }
    }

    private var lastDownTime = 0L

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val x = event.x
        val y = event.y

        when (event.action) {
            MotionEvent.ACTION_DOWN -> {
                val now = System.currentTimeMillis()
                val isDoubleClick = (now - lastDownTime) < 300
                lastDownTime = now

                lastTouchX = x
                lastTouchY = y
                isDragging = false

                // Check buttons (reverse order to pick top-most if overlapping)
                draggedButton = layoutConfig.buttons.lastOrNull { btn ->
                    hypot((x - btn.x).toDouble(), (y - btn.y).toDouble()) <= btn.radius
                }

                if (draggedButton != null) {
                    if (isDoubleClick) {
                        onButtonDoubleTapped?.invoke(draggedButton!!)
                        draggedButton = null
                    }
                    return true
                }

                // Check joysticks
                draggedJoystick = layoutConfig.joysticks.lastOrNull { js ->
                    hypot((x - js.x).toDouble(), (y - js.y).toDouble()) <= js.radius
                }

                if (draggedJoystick != null) {
                    if (isDoubleClick) {
                        onJoystickDoubleTapped?.invoke(draggedJoystick!!)
                        draggedJoystick = null
                    }
                    return true
                }
            }
            MotionEvent.ACTION_MOVE -> {
                val dx = x - lastTouchX
                val dy = y - lastTouchY
                
                if (hypot(dx.toDouble(), dy.toDouble()) > 10) {
                    isDragging = true
                }

                if (draggedButton != null) {
                    draggedButton!!.x += dx
                    draggedButton!!.y += dy
                    lastTouchX = x
                    lastTouchY = y
                    invalidate()
                    return true
                } else if (draggedJoystick != null) {
                    draggedJoystick!!.x += dx
                    draggedJoystick!!.y += dy
                    lastTouchX = x
                    lastTouchY = y
                    invalidate()
                    return true
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                draggedButton = null
                draggedJoystick = null
                return true
            }
        }
        return true
    }
}
