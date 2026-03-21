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
import androidx.documentfile.provider.DocumentFile
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
        private const val REQUEST_CODE_OPEN_TREE = 1002
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
            val intent = Intent(Intent.ACTION_OPEN_DOCUMENT_TREE).apply {
                flags = Intent.FLAG_GRANT_READ_URI_PERMISSION or
                    Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION
            }
            startActivityForResult(intent, REQUEST_CODE_OPEN_TREE)
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
        if (requestCode == REQUEST_CODE_OPEN_TREE && resultCode == Activity.RESULT_OK) {
            data?.data?.let { treeUri ->
                try {
                    contentResolver.takePersistableUriPermission(
                        treeUri,
                        Intent.FLAG_GRANT_READ_URI_PERMISSION
                    )
                } catch (_: SecurityException) {
                    // Some providers do not support persistable permissions; copy still works this session.
                }

                tvSelectedFile.text = "Copying game files to cache..."
                btnLaunch.isEnabled = false

                Thread {
                    val filePath = copyGameFolderToCache(treeUri)
                    runOnUiThread {
                        if (filePath != null) {
                            selectedFilePath = filePath
                            tvSelectedFile.text = "Ready: ${File(filePath).name}"
                            btnLaunch.isEnabled = true
                        } else {
                            selectedFilePath = null
                            tvSelectedFile.text = "Could not find data.win / game.unx in folder"
                            Toast.makeText(
                                this,
                                "Pick the folder that contains the data file and .ogg music files",
                                Toast.LENGTH_LONG
                            ).show()
                        }
                    }
                }.start()
            }
        }
    }

    /**
     * Copies the whole selected directory into app cache so external (non-embedded) sounds
     * resolve next to the data archive, same as on desktop.
     */
    private fun copyGameFolderToCache(treeUri: Uri): String? {
        val root = DocumentFile.fromTreeUri(this, treeUri) ?: return null
        val destDir = File(cacheDir, "game_data_${System.currentTimeMillis()}")
        destDir.mkdirs()

        return try {
            copyDocumentTree(root, destDir, "")
            findMainDataFile(destDir)?.absolutePath
        } catch (e: Exception) {
            e.printStackTrace()
            null
        }
    }

    private fun copyDocumentTree(node: DocumentFile, destRoot: File, relativePrefix: String) {
        when {
            node.isDirectory -> {
                node.listFiles().forEach { child ->
                    val name = child.name ?: return@forEach
                    val nextRel = if (relativePrefix.isEmpty()) name else "$relativePrefix/$name"
                    copyDocumentTree(child, destRoot, nextRel)
                }
            }
            node.isFile -> {
                val out = File(destRoot, relativePrefix)
                out.parentFile?.mkdirs()
                contentResolver.openInputStream(node.uri)?.use { input ->
                    FileOutputStream(out).use { output -> input.copyTo(output) }
                }
            }
        }
    }

    private fun findMainDataFile(dir: File): File? {
        val candidates = listOf("data.win", "game.unx", "game.win", "game.droid")
        for (name in candidates) {
            val f = File(dir, name)
            if (f.isFile) return f
        }
        return dir.walkTopDown()
            .maxDepth(6)
            .firstOrNull { file ->
                file.isFile && candidates.any { file.name.equals(it, ignoreCase = true) }
            }
    }
}
