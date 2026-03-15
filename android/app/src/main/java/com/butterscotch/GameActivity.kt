package com.butterscotch

import android.app.Activity
import android.opengl.GLSurfaceView
import android.os.Bundle
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class GameActivity : Activity() {

    private lateinit var glSurfaceView: ButterscotchGLSurfaceView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        
        // Setup fullscreen
        window.setFlags(
            WindowManager.LayoutParams.FLAG_FULLSCREEN,
            WindowManager.LayoutParams.FLAG_FULLSCREEN
        )
        window.decorView.systemUiVisibility = (View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_FULLSCREEN
                or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY)

        val gameDataPath = intent.getStringExtra("GAME_DATA_PATH") ?: ""
        val debugMode = intent.getBooleanExtra("DEBUG_MODE", false)
        val headlessMode = intent.getBooleanExtra("HEADLESS_MODE", false)

        glSurfaceView = ButterscotchGLSurfaceView(this, gameDataPath, debugMode, headlessMode)
        
        val container = android.widget.FrameLayout(this)
        container.addView(glSurfaceView)
        
        val gamepadView = VirtualGamepadView(this)
        container.addView(gamepadView)
        
        setContentView(container)
    }

    override fun onResume() {
        super.onResume()
        glSurfaceView.onResume()
    }

    override fun onPause() {
        super.onPause()
        glSurfaceView.onPause()
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean {
        val gmlKeyCode = mapAndroidKeyCodeToGml(keyCode)
        if (gmlKeyCode != 0) {
            ButterscotchNative.nativeKey(gmlKeyCode, true)
            return true
        }
        return super.onKeyDown(keyCode, event)
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent): Boolean {
        val gmlKeyCode = mapAndroidKeyCodeToGml(keyCode)
        if (gmlKeyCode != 0) {
            ButterscotchNative.nativeKey(gmlKeyCode, false)
            return true
        }
        return super.onKeyUp(keyCode, event)
    }

    private fun mapAndroidKeyCodeToGml(keyCode: Int): Int {
        return when (keyCode) {
            KeyEvent.KEYCODE_DPAD_LEFT -> 37  // VK_LEFT
            KeyEvent.KEYCODE_DPAD_UP -> 38    // VK_UP
            KeyEvent.KEYCODE_DPAD_RIGHT -> 39 // VK_RIGHT
            KeyEvent.KEYCODE_DPAD_DOWN -> 40  // VK_DOWN
            KeyEvent.KEYCODE_SPACE -> 32      // VK_SPACE
            KeyEvent.KEYCODE_ENTER -> 13      // VK_ENTER
            KeyEvent.KEYCODE_ESCAPE -> 27     // VK_ESCAPE
            KeyEvent.KEYCODE_DEL -> 8         // VK_BACKSPACE
            KeyEvent.KEYCODE_TAB -> 9         // VK_TAB
            KeyEvent.KEYCODE_SHIFT_LEFT, KeyEvent.KEYCODE_SHIFT_RIGHT -> 16 // VK_SHIFT
            KeyEvent.KEYCODE_CTRL_LEFT, KeyEvent.KEYCODE_CTRL_RIGHT -> 17   // VK_CONTROL
            KeyEvent.KEYCODE_ALT_LEFT, KeyEvent.KEYCODE_ALT_RIGHT -> 18     // VK_ALT
            KeyEvent.KEYCODE_PAGE_UP -> 33    // VK_PAGEUP
            KeyEvent.KEYCODE_PAGE_DOWN -> 34  // VK_PAGEDOWN
            KeyEvent.KEYCODE_MOVE_END -> 35   // VK_END
            KeyEvent.KEYCODE_MOVE_HOME -> 36  // VK_HOME
            KeyEvent.KEYCODE_INSERT -> 45     // VK_INSERT
            KeyEvent.KEYCODE_FORWARD_DEL -> 46 // VK_DELETE
            in KeyEvent.KEYCODE_0..KeyEvent.KEYCODE_9 -> keyCode - KeyEvent.KEYCODE_0 + 48
            in KeyEvent.KEYCODE_A..KeyEvent.KEYCODE_Z -> keyCode - KeyEvent.KEYCODE_A + 65
            else -> 0
        }
    }

    // Load native library
    companion object {
        init {
            System.loadLibrary("butterscotch")
        }
    }
}

class ButterscotchGLSurfaceView(
    context: android.content.Context,
    private val dataPath: String,
    private val debugMode: Boolean,
    private val headlessMode: Boolean
) : GLSurfaceView(context) {

    private val renderer: ButterscotchRenderer

    init {
        // Use OpenGL ES 3.0
        setEGLContextClientVersion(3)

        renderer = ButterscotchRenderer(dataPath, debugMode, headlessMode)
        setRenderer(renderer)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        // Find pointer index
        val action = event.actionMasked
        val pointerIndex = event.actionIndex
        val pointerId = event.getPointerId(pointerIndex)

        val x = event.getX(pointerIndex)
        val y = event.getY(pointerIndex)

        when (action) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                ButterscotchNative.nativeTouch(pointerId, x, y, 0) // 0 for DOWN
            }
            MotionEvent.ACTION_MOVE -> {
                for (i in 0 until event.pointerCount) {
                    ButterscotchNative.nativeTouch(
                        event.getPointerId(i),
                        event.getX(i),
                        event.getY(i),
                        1 // 1 for MOVE
                    )
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                ButterscotchNative.nativeTouch(pointerId, x, y, 2) // 2 for UP
            }
        }
        return true
    }
}

class ButterscotchRenderer(
    private val dataPath: String,
    private val debugMode: Boolean,
    private val headlessMode: Boolean
) : GLSurfaceView.Renderer {

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
        ButterscotchNative.nativeInit(dataPath, debugMode, headlessMode)
    }

    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
        ButterscotchNative.nativeResize(width, height)
    }

    override fun onDrawFrame(gl: GL10?) {
        if (!ButterscotchNative.nativeStep()) {
            // Game decided to exit
            System.exit(0)
        }
    }
}

object ButterscotchNative {
    external fun nativeInit(dataPath: String, debugMode: Boolean, headlessMode: Boolean)
    external fun nativeResize(width: Int, height: Int)
    external fun nativeStep(): Boolean
    external fun nativeTouch(pointerId: Int, x: Float, y: Float, action: Int)
    external fun nativeKey(keyCode: Int, down: Boolean)
}
