package com.butterscotch

import android.app.AlertDialog
import android.graphics.Color
import android.os.Bundle
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
        setContentView(R.layout.activity_editor)

        layoutConfig = VirtualGamepadConfig.loadLayout(this)

        val container = findViewById<FrameLayout>(R.id.editorContainer)
        editorView = VirtualGamepadEditorView(this, layoutConfig)
        container.addView(editorView)

        editorView.onButtonDoubleTapped = { btn ->
            showEditButtonDialog(btn)
        }

        findViewById<Button>(R.id.btnAddButton).setOnClickListener {
            layoutConfig.buttons.add(VirtualButton(
                x = container.width / 2f,
                y = container.height / 2f
            ))
            editorView.invalidate()
        }

        findViewById<Button>(R.id.btnToggleJoystick).setOnClickListener {
            layoutConfig.joystick.enabled = !layoutConfig.joystick.enabled
            if (layoutConfig.joystick.enabled) {
                // reset position if it was out of bounds
                layoutConfig.joystick.x = 200f
                layoutConfig.joystick.y = container.height - 200f
            }
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
}
