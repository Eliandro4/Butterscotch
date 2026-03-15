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

    data class PointerState(var x: Float, var y: Float, var buttonPressed: VirtualButton?)
    private val activePointers = mutableMapOf<Int, PointerState>()

    class JoystickState(val js: VirtualJoystick) {
        var pointerId: Int? = null
        var nubX: Float = js.x
        var nubY: Float = js.y
        val activeKeys = mutableSetOf<Int>()

        fun reset() {
            pointerId = null
            nubX = js.x
            nubY = js.y
        }
    }

    private val joystickStates = mutableMapOf<VirtualJoystick, JoystickState>()

    init {
        setWillNotDraw(false)
        initJoystickStates()
    }

    private fun initJoystickStates() {
        for (jsState in joystickStates.values) {
            for (key in jsState.activeKeys) {
                sendKey(key, false)
            }
        }
        joystickStates.clear()
        for (js in layoutConfig.joysticks) {
            joystickStates[js] = JoystickState(js)
        }
    }

    fun reloadConfig() {
        val newLayout = VirtualGamepadConfig.loadLayout(context)
        layoutConfig.buttons.clear()
        layoutConfig.buttons.addAll(newLayout.buttons)
        layoutConfig.joysticks.clear()
        layoutConfig.joysticks.addAll(newLayout.joysticks)
        initJoystickStates()
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        // Draw Joysticks
        for (js in layoutConfig.joysticks) {
            val state = joystickStates[js]
            if (state != null) {
                canvas.drawCircle(js.x, js.y, js.radius, joystickBasePaint)
                canvas.drawCircle(js.x, js.y, js.radius, buttonStrokePaint)
                canvas.drawCircle(state.nubX, state.nubY, js.innerRadius, joystickNubPaint)
            }
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
        // Check joysticks first
        for (js in layoutConfig.joysticks) {
            val state = joystickStates[js] ?: continue
            if (state.pointerId == null) {
                val dist = hypot((x - js.x).toDouble(), (y - js.y).toDouble()).toFloat()
                if (dist <= js.radius * 1.5f) { // slightly larger hit box
                    state.pointerId = pointerId
                    updateJoystickInput(state, x, y)
                    activePointers[pointerId] = PointerState(x, y, null)
                    return
                }
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

        for (jsState in joystickStates.values) {
            if (jsState.pointerId == pointerId) {
                updateJoystickInput(jsState, x, y)
                return
            }
        }
    }

    private fun handlePointerUp(pointerId: Int) {
        val state = activePointers.remove(pointerId)
        
        for (jsState in joystickStates.values) {
            if (jsState.pointerId == pointerId) {
                for (key in jsState.activeKeys) {
                    sendKey(key, false)
                }
                jsState.activeKeys.clear()
                jsState.reset()
                return
            }
        }

        if (state?.buttonPressed != null) {
            sendKey(state.buttonPressed!!.mappedKey, false)
        }
    }

    private fun updateJoystickInput(state: JoystickState, px: Float, py: Float) {
        val js = state.js
        val dx = px - js.x
        val dy = py - js.y
        val dist = hypot(dx.toDouble(), dy.toDouble()).toFloat()
        
        if (dist <= js.radius) {
            state.nubX = px
            state.nubY = py
        } else {
            val angle = atan2(dy.toDouble(), dx.toDouble())
            state.nubX = js.x + (cos(angle) * js.radius).toFloat()
            state.nubY = js.y + (sin(angle) * js.radius).toFloat()
        }

        // Convert offset to 4-way keys (diagonals will trigger 2 keys)
        val deadZone = js.radius * 0.3f
        
        val isLeft = dx < -deadZone
        val isRight = dx > deadZone
        val isUp = dy < -deadZone
        val isDown = dy > deadZone

        updateJsKey(state, js.leftKey, isLeft)
        updateJsKey(state, js.rightKey, isRight)
        updateJsKey(state, js.upKey, isUp)
        updateJsKey(state, js.downKey, isDown)
    }

    private fun updateJsKey(state: JoystickState, keyCode: Int, pressed: Boolean) {
        if (pressed) {
            if (!state.activeKeys.contains(keyCode)) {
                sendKey(keyCode, true)
                state.activeKeys.add(keyCode)
            }
        } else {
            if (state.activeKeys.contains(keyCode)) {
                sendKey(keyCode, false)
                state.activeKeys.remove(keyCode)
            }
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
