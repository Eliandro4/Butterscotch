package com.butterscotch

import android.content.Context
import android.content.SharedPreferences
import com.google.gson.Gson
import com.google.gson.reflect.TypeToken

data class VirtualButton(
    var id: String = java.util.UUID.randomUUID().toString(),
    var x: Float = 100f,
    var y: Float = 100f,
    var radius: Float = 50f,
    var mappedKey: Int = 32, // VK_SPACE by default
    var label: String = "SPACE"
)

data class VirtualJoystick(
    var id: String = java.util.UUID.randomUUID().toString(),
    var x: Float = 150f,
    var y: Float = 150f,
    var radius: Float = 100f, // Outer radius
    var innerRadius: Float = 40f,
    var upKey: Int = 38,
    var downKey: Int = 40,
    var leftKey: Int = 37,
    var rightKey: Int = 39
)

data class VirtualGamepadLayout(
    var buttons: MutableList<VirtualButton> = mutableListOf(),
    var joysticks: MutableList<VirtualJoystick> = mutableListOf()
)

object VirtualGamepadConfig {
    private const val PREFS_NAME = "VirtualGamepadPrefs"
    private const val KEY_LAYOUT = "gamepad_layout"

    private fun getPrefs(context: Context): SharedPreferences {
        return context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
    }

    fun loadLayout(context: Context): VirtualGamepadLayout {
        val prefs = getPrefs(context)
        val json = prefs.getString(KEY_LAYOUT, null)
        if (json != null) {
            try {
                val type = object : TypeToken<VirtualGamepadLayout>() {}.type
                return Gson().fromJson(json, type)
            } catch (e: Exception) {
                e.printStackTrace()
            }
        }
        return VirtualGamepadLayout() // default empty layout
    }

    fun saveLayout(context: Context, layout: VirtualGamepadLayout) {
        val prefs = getPrefs(context)
        val json = Gson().toJson(layout)
        prefs.edit().putString(KEY_LAYOUT, json).apply()
    }
}
