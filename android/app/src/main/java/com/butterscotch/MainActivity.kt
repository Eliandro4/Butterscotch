package com.butterscotch

import android.app.Activity
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.widget.Button
import android.widget.Switch
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import java.io.File
import java.io.FileOutputStream

class MainActivity : AppCompatActivity() {

    private lateinit var btnSelectFile: Button
    private lateinit var tvSelectedFile: TextView
    private lateinit var switchDebug: Switch
    private lateinit var switchHeadless: Switch
    private lateinit var btnLaunch: Button
    private lateinit var btnEditGamepad: Button

    private var selectedFilePath: String? = null

    companion object {
        private const val REQUEST_CODE_OPEN_DOCUMENT = 1001
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        btnSelectFile = findViewById(R.id.btnSelectFile)
        tvSelectedFile = findViewById(R.id.tvSelectedFile)
        switchDebug = findViewById(R.id.switchDebug)
        switchHeadless = findViewById(R.id.switchHeadless)
        btnLaunch = findViewById(R.id.btnLaunch)
        btnEditGamepad = findViewById(R.id.btnEditGamepad)

        btnSelectFile.setOnClickListener {
            val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
                addCategory(Intent.CATEGORY_OPENABLE)
                type = "*/*"
            }
            startActivityForResult(intent, REQUEST_CODE_OPEN_DOCUMENT)
        }

        btnLaunch.setOnClickListener {
            if (selectedFilePath != null) {
                val intent = Intent(this, GameActivity::class.java).apply {
                    putExtra("GAME_DATA_PATH", selectedFilePath)
                    putExtra("DEBUG_MODE", switchDebug.isChecked)
                    putExtra("HEADLESS_MODE", switchHeadless.isChecked)
                }
                startActivity(intent)
            }
        }

        btnEditGamepad.setOnClickListener {
            val intent = Intent(this, VirtualGamepadEditorActivity::class.java)
            startActivity(intent)
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == REQUEST_CODE_OPEN_DOCUMENT && resultCode == Activity.RESULT_OK) {
            data?.data?.let { uri ->
                tvSelectedFile.text = "Copying file to cache..."
                btnLaunch.isEnabled = false
                
                Thread {
                    val filePath = copyUriToCache(uri)
                    runOnUiThread {
                        if (filePath != null) {
                            selectedFilePath = filePath
                            tvSelectedFile.text = "Selected: ${File(filePath).name}"
                            btnLaunch.isEnabled = true
                        } else {
                            tvSelectedFile.text = "Failed to copy file"
                            Toast.makeText(this, "Could not read the selected file", Toast.LENGTH_SHORT).show()
                        }
                    }
                }.start()
            }
        }
    }

    private fun copyUriToCache(uri: Uri): String? {
        try {
            val fileName = "data.win" // Let's simplify and just name it data.win in cache
            val cacheFile = File(cacheDir, fileName)
            
            contentResolver.openInputStream(uri)?.use { inputStream ->
                FileOutputStream(cacheFile).use { outputStream ->
                    inputStream.copyTo(outputStream)
                }
            }
            return cacheFile.absolutePath
        } catch (e: Exception) {
            e.printStackTrace()
            return null
        }
    }
}
