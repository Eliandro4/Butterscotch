package com.butterscotch

import android.app.Activity
import android.opengl.GLSurfaceView
import android.os.Bundle
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
        setContentView(glSurfaceView)
    }

    override fun onResume() {
        super.onResume()
        glSurfaceView.onResume()
    }

    override fun onPause() {
        super.onPause()
        glSurfaceView.onPause()
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
}
