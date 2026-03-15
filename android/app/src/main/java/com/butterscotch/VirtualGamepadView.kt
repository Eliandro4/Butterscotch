package com.butterscotch

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.view.MotionEvent
import android.view.View
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.hypot
import kotlin.math.sin

class VirtualGamepadView(context: Context) : View(context) {

    private val layoutConfig = VirtualGamepadConfig.loadLayout(context)

    private val buttonPaint = Paint().apply {
        color = Color.argb(128, 200, 200, 200)
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
        color = Color.argb(100, 150, 150, 150)
        style = Paint.Style.FILL
        isAntiAlias = true
    }

    private val joystickNubPaint = Paint().apply {
        color = Color.argb(180, 255, 255, 255)
        style = Paint.Style.FILL
        isAntiAlias = true
    }

    // Tracking active pointers
    private val activePointers = mutableMapOf<Int, PointerState>()
    private var joystickPointerId: Int? = null
    
    // Joystick state
    private var jsNubX = layoutConfig.joystick.x
    private var jsNubY = layoutConfig.joystick.y
    private var isJsActive = false

    private val activeKeys = mutableSetOf<Int>()

    data class PointerState(var x: Float, var y: Float, var buttonPressed: VirtualButton?)

    init {
        // Necessary if drawing
        setWillNotDraw(false)
    }

    fun reloadConfig() {
        val newLayout = VirtualGamepadConfig.loadLayout(context)
        layoutConfig.buttons.clear()
        layoutConfig.buttons.addAll(newLayout.buttons)
        layoutConfig.joystick = newLayout.joystick
        
        jsNubX = layoutConfig.joystick.x
        jsNubY = layoutConfig.joystick.y
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        // Draw Joystick
        if (layoutConfig.joystick.enabled) {
            val js = layoutConfig.joystick
            canvas.drawCircle(js.x, js.y, js.radius, joystickBasePaint)
            // Stroke
            canvas.drawCircle(js.x, js.y, js.radius, buttonStrokePaint)
            // Nub
            val nx = if (isJsActive) jsNubX else js.x
            val ny = if (isJsActive) jsNubY else js.y
            canvas.drawCircle(nx, ny, js.innerRadius, joystickNubPaint)
        }

        // Draw Buttons
        for (btn in layoutConfig.buttons) {
            val isActive = activePointers.values.any { it.buttonPressed == btn }
            buttonPaint.color = if (isActive) Color.argb(180, 255, 255, 255) else Color.argb(128, 200, 200, 200)

            canvas.drawCircle(btn.x, btn.y, btn.radius, buttonPaint)
            canvas.drawCircle(btn.x, btn.y, btn.radius, buttonStrokePaint)

            val textY = btn.y - ((textPaint.descent() + textPaint.ascent()) / 2)
            canvas.drawText(btn.label, btn.x, textY, textPaint)
        }
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        val action = event.actionMasked
        val pointerIndex = event.actionIndex
        val pointerId = event.getPointerId(pointerIndex)
        val x = event.getX(pointerIndex)
        val y = event.getY(pointerIndex)

        when (action) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                handlePointerDown(pointerId, x, y)
                invalidate()
                return true
            }
            MotionEvent.ACTION_MOVE -> {
                for (i in 0 until event.pointerCount) {
                    val pId = event.getPointerId(i)
                    val px = event.getX(i)
                    val py = event.getY(i)
                    handlePointerMove(pId, px, py)
                }
                invalidate()
                return true
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                handlePointerUp(pointerId)
                invalidate()
                return true
            }
            MotionEvent.ACTION_CANCEL -> {
                handlePointerUp(pointerId)
                invalidate()
                return true
            }
        }
        return super.onTouchEvent(event)
    }

    private fun handlePointerDown(pointerId: Int, x: Float, y: Float) {
        // Check joystick first
        if (layoutConfig.joystick.enabled && joystickPointerId == null) {
            val js = layoutConfig.joystick
            val dist = hypot((x - js.x).toDouble(), (y - js.y).toDouble()).toFloat()
            if (dist <= js.radius * 1.5f) { // slightly larger hit box
                joystickPointerId = pointerId
                isJsActive = true
                updateJoystickInput(x, y)
                activePointers[pointerId] = PointerState(x, y, null)
                return
            }
        }

        // Check buttons
        for (btn in layoutConfig.buttons) {
            val dist = hypot((x - btn.x).toDouble(), (y - btn.y).toDouble()).toFloat()
            if (dist <= btn.radius) {
                // Pressed button
                val state = PointerState(x, y, btn)
                activePointers[pointerId] = state
                sendKey(btn.mappedKey, true)
                return
            }
        }
        
        // If unhandled, record it anyway to track movement
        activePointers[pointerId] = PointerState(x, y, null)
    }

    private fun handlePointerMove(pointerId: Int, x: Float, y: Float) {
        val state = activePointers[pointerId] ?: return
        state.x = x
        state.y = y

        if (joystickPointerId == pointerId) {
            updateJoystickInput(x, y)
            return
        }

        // Moving over buttons? For simplicity, we only trigger button if pressed down on it initially.
        // Optional: Implement sliding across buttons.
    }

    private fun handlePointerUp(pointerId: Int) {
        val state = activePointers.remove(pointerId)
        
        if (joystickPointerId == pointerId) {
            joystickPointerId = null
            isJsActive = false
            jsNubX = layoutConfig.joystick.x
            jsNubY = layoutConfig.joystick.y
            
            // Release all arrow keys
            releaseJsKey(37) // Left
            releaseJsKey(38) // Up
            releaseJsKey(39) // Right
            releaseJsKey(40) // Down
            return
        }

        if (state?.buttonPressed != null) {
            sendKey(state.buttonPressed!!.mappedKey, false)
        }
    }

    private fun updateJoystickInput(px: Float, py: Float) {
        val js = layoutConfig.joystick
        val dx = px - js.x
        val dy = py - js.y
        val dist = hypot(dx.toDouble(), dy.toDouble()).toFloat()
        
        if (dist <= js.radius) {
            jsNubX = px
            jsNubY = py
        } else {
            val angle = atan2(dy.toDouble(), dx.toDouble())
            jsNubX = js.x + (cos(angle) * js.radius).toFloat()
            jsNubY = js.y + (sin(angle) * js.radius).toFloat()
        }

        // Convert offset to 8-way directional keys
        val deadZone = js.radius * 0.3f
        
        val isLeft = dx < -deadZone
        val isRight = dx > deadZone
        val isUp = dy < -deadZone
        val isDown = dy > deadZone

        updateJsKey(37, isLeft)   // Left
        updateJsKey(39, isRight)  // Right
        updateJsKey(38, isUp)     // Up
        updateJsKey(40, isDown)   // Down
    }

    private fun updateJsKey(keyCode: Int, pressed: Boolean) {
        if (pressed) {
            if (!activeKeys.contains(keyCode)) {
                sendKey(keyCode, true)
                activeKeys.add(keyCode)
            }
        } else {
            if (activeKeys.contains(keyCode)) {
                sendKey(keyCode, false)
                activeKeys.remove(keyCode)
            }
        }
    }
    
    private fun releaseJsKey(keyCode: Int) {
        if (activeKeys.contains(keyCode)) {
            sendKey(keyCode, false)
            activeKeys.remove(keyCode)
        }
    }

    private fun sendKey(keyCode: Int, down: Boolean) {
        try {
            ButterscotchNative.nativeKey(keyCode, down)
        } catch (e: Exception) {
            // Native lib might not be loaded in designer
            e.printStackTrace()
        }
    }
}
