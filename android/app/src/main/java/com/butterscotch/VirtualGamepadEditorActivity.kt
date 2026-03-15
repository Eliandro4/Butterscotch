package com.butterscotch

import android.app.AlertDialog
import android.graphics.Color
import android.os.Bundle
import android.view.View
import android.view.Window
import android.view.WindowManager
import android.widget.Button
import android.widget.EditText
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity

class VirtualGamepadEditorActivity : AppCompatActivity() {

    private lateinit var layoutConfig: VirtualGamepadLayout
    private lateinit var editorView: VirtualGamepadEditorView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        requestWindowFeature(Window.FEATURE_NO_TITLE)
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
        setContentView(R.layout.activity_editor)

        layoutConfig = VirtualGamepadConfig.loadLayout(this)

        val container = findViewById<FrameLayout>(R.id.editorContainer)
        editorView = VirtualGamepadEditorView(this, layoutConfig)
        container.addView(editorView)

        editorView.onButtonDoubleTapped = { btn ->
            showEditButtonDialog(btn)
        }
        editorView.onJoystickDoubleTapped = { js ->
            showEditJoystickDialog(js)
        }

        findViewById<Button>(R.id.btnAddButton).setOnClickListener {
            layoutConfig.buttons.add(VirtualButton(
                x = container.width / 2f,
                y = container.height / 2f
            ))
            editorView.invalidate()
        }

        findViewById<Button>(R.id.btnAddJoystick).setOnClickListener {
            layoutConfig.joysticks.add(VirtualJoystick(
                x = container.width / 4f,
                y = container.height - 200f
            ))
            editorView.invalidate()
        }

        findViewById<Button>(R.id.btnCancel).setOnClickListener {
            finish()
        }

        findViewById<Button>(R.id.btnSave).setOnClickListener {
            VirtualGamepadConfig.saveLayout(this, layoutConfig)
            finish()
        }
    }

    private fun showEditButtonDialog(btn: VirtualButton) {
        val layout = LinearLayout(this)
        layout.orientation = LinearLayout.VERTICAL
        layout.setPadding(50, 40, 50, 10)

        val labelInput = EditText(this)
        labelInput.hint = "Label (e.g. SPACE, Z, X)"
        labelInput.setText(btn.label)
        layout.addView(labelInput)

        val keyCodeInput = EditText(this)
        keyCodeInput.hint = "Key Code (e.g. 32=Space, 90=Z)"
        keyCodeInput.setText(btn.mappedKey.toString())
        keyCodeInput.inputType = android.text.InputType.TYPE_CLASS_NUMBER
        layout.addView(keyCodeInput)
        
        val radiusInput = EditText(this)
        radiusInput.hint = "Radius"
        radiusInput.setText(btn.radius.toInt().toString())
        radiusInput.inputType = android.text.InputType.TYPE_CLASS_NUMBER
        layout.addView(radiusInput)
        
        // Helper text
        val helpText = TextView(this)
        helpText.text = "Common Keys:\nSpace = 32\nEnter = 13\nArrows = 37, 38, 39, 40\nZ = 90, X = 88, C = 67"
        layout.addView(helpText)

        AlertDialog.Builder(this)
            .setTitle("Edit Button")
            .setView(layout)
            .setPositiveButton("Save") { _, _ ->
                btn.label = labelInput.text.toString()
                btn.mappedKey = keyCodeInput.text.toString().toIntOrNull() ?: btn.mappedKey
                btn.radius = radiusInput.text.toString().toFloatOrNull() ?: btn.radius
                editorView.invalidate()
            }
            .setNeutralButton("Delete") { _, _ ->
                layoutConfig.buttons.remove(btn)
                editorView.invalidate()
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun showEditJoystickDialog(js: VirtualJoystick) {
        val layout = LinearLayout(this)
        layout.orientation = LinearLayout.VERTICAL
        layout.setPadding(50, 40, 50, 10)

        val radiusInput = EditText(this)
        radiusInput.hint = "Radius"
        radiusInput.setText(js.radius.toInt().toString())
        radiusInput.inputType = android.text.InputType.TYPE_CLASS_NUMBER
        layout.addView(radiusInput)
        
        val upInput = EditText(this)
        upInput.hint = "Up Key (default 38)"
        upInput.setText(js.upKey.toString())
        upInput.inputType = android.text.InputType.TYPE_CLASS_NUMBER
        layout.addView(upInput)

        val downInput = EditText(this)
        downInput.hint = "Down Key (default 40)"
        downInput.setText(js.downKey.toString())
        downInput.inputType = android.text.InputType.TYPE_CLASS_NUMBER
        layout.addView(downInput)
        
        val leftInput = EditText(this)
        leftInput.hint = "Left Key (default 37)"
        leftInput.setText(js.leftKey.toString())
        leftInput.inputType = android.text.InputType.TYPE_CLASS_NUMBER
        layout.addView(leftInput)

        val rightInput = EditText(this)
        rightInput.hint = "Right Key (default 39)"
        rightInput.setText(js.rightKey.toString())
        rightInput.inputType = android.text.InputType.TYPE_CLASS_NUMBER
        layout.addView(rightInput)

        AlertDialog.Builder(this)
            .setTitle("Edit Joystick")
            .setView(layout)
            .setPositiveButton("Save") { _, _ ->
                js.radius = radiusInput.text.toString().toFloatOrNull() ?: js.radius
                js.upKey = upInput.text.toString().toIntOrNull() ?: js.upKey
                js.downKey = downInput.text.toString().toIntOrNull() ?: js.downKey
                js.leftKey = leftInput.text.toString().toIntOrNull() ?: js.leftKey
                js.rightKey = rightInput.text.toString().toIntOrNull() ?: js.rightKey
                editorView.invalidate()
            }
            .setNeutralButton("Delete") { _, _ ->
                layoutConfig.joysticks.remove(js)
                editorView.invalidate()
            }
            .setNegativeButton("Cancel", null)
            .show()
    }
}
