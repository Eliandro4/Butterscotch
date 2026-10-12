package net.perfectdreams.butterscotch.android.screens

import android.net.Uri
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.expandVertically
import androidx.compose.animation.shrinkVertically
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ExpandMore
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.rotate
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.navigation.NavHostController
import kotlinx.coroutines.launch
import net.perfectdreams.butterscotch.android.GameImporter
import net.perfectdreams.butterscotch.android.components.ButterscotchBackButton
import net.perfectdreams.butterscotch.android.components.ButterscotchTopBar
import net.perfectdreams.butterscotch.android.layouts.LayoutLibrary
import net.perfectdreams.butterscotch.android.library.GameLibrary
import java.util.UUID

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun UpdateGameScreen(
    gameLibrary: GameLibrary,
    layoutLibrary: LayoutLibrary,
    gameId: UUID,
    nav: NavHostController
) {
    val entry = gameLibrary.findById(gameId) ?: return
    val context = LocalContext.current
    val scope = rememberCoroutineScope()
    var state by remember { mutableStateOf<UpdateUIState>(UpdateUIState.Intro) }
    var showConfirmDialog by remember { mutableStateOf(false) }
    var pendingResult by remember { mutableStateOf<GameImporter.Result.Success?>(null) }

    val pickFolder = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocumentTree()) { uri: Uri? ->
        if (uri == null) return@rememberLauncherForActivityResult
        val copyingState = UpdateUIState.Copying()
        state = copyingState
        scope.launch {
            val result = GameImporter.import(context, uri, gameLibrary) { copyingState.currentFile = it }
            when (result) {
                is GameImporter.Result.Success -> {
                    pendingResult = result
                    showConfirmDialog = true
                    state = UpdateUIState.Intro
                }
                is GameImporter.Result.MissingWad -> {
                    state = UpdateUIState.Error("Missing WAD in folder!\n\nExpected one of: ${GameImporter.WAD_FILENAMES.joinToString(", ")}")
                }
                is GameImporter.Result.Failure -> {
                    state = UpdateUIState.Error(result.message)
                }
            }
        }
    }

    val pickZip = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        if (uri == null) return@rememberLauncherForActivityResult
        val copyingState = UpdateUIState.Copying()
        state = copyingState
        scope.launch {
            val result = GameImporter.importZip(context, uri, gameLibrary) { copyingState.currentFile = it }
            when (result) {
                is GameImporter.Result.Success -> {
                    pendingResult = result
                    showConfirmDialog = true
                    state = UpdateUIState.Intro
                }
                is GameImporter.Result.MissingWad -> {
                    state = UpdateUIState.Error("Missing WAD in ZIP!\n\nExpected one of: ${GameImporter.WAD_FILENAMES.joinToString(", ")}")
                }
                is GameImporter.Result.Failure -> {
                    state = UpdateUIState.Error(result.message)
                }
            }
        }
    }

    Scaffold(
        topBar = {
            ButterscotchTopBar(
                { Text("Update ${entry.title}") },
                nav,
                navigationIcon = { ButterscotchBackButton(nav) }
            )
        },
    ) { innerPadding ->
        Box(Modifier.fillMaxSize().padding(innerPadding).padding(24.dp)) {
            when (val s = state) {
                UpdateUIState.Intro -> UpdateIntroPane(
                    onSelectFolder = { pickFolder.launch(null) },
                    onSelectZip = { pickZip.launch(arrayOf("application/zip", "application/x-zip-compressed", "application/octet-stream")) }
                )
                is UpdateUIState.Copying -> UpdateCopyingPane(s.currentFile)
                is UpdateUIState.Error -> UpdateErrorPane(s.message) { state = UpdateUIState.Intro }
            }
        }
    }

    if (showConfirmDialog && pendingResult != null) {
        AlertDialog(
            onDismissRequest = { 
                showConfirmDialog = false
                gameLibrary.discardStaging(pendingResult!!.staged)
                pendingResult = null
            },
            title = { Text("Overwrite Game Files?") },
            text = { Text("This will directly overwrite the game files in the bundle folder. This can't be undone.") },
            confirmButton = {
                TextButton(onClick = {
                    val result = pendingResult!!
                    showConfirmDialog = false
                    pendingResult = null
                    scope.launch {
                        try {
                            gameLibrary.updateGameFiles(result.staged, entry.id)
                            Toast.makeText(context, "Game updated successfully", Toast.LENGTH_SHORT).show()
                            nav.popBackStack()
                        } catch (e: Exception) {
                            state = UpdateUIState.Error("Failed to update game: ${e.message}")
                        }
                    }
                }) { Text("Update", color = MaterialTheme.colorScheme.error) }
            },
            dismissButton = {
                TextButton(onClick = { 
                    showConfirmDialog = false
                    gameLibrary.discardStaging(pendingResult!!.staged)
                    pendingResult = null
                }) { Text("Cancel") }
            },
        )
    }
}

private sealed interface UpdateUIState {
    data object Intro : UpdateUIState
    class Copying : UpdateUIState {
        var currentFile by mutableStateOf<String?>(null)
    }
    data class Error(val message: String) : UpdateUIState
}

@Composable
private fun UpdateIntroPane(
    onSelectFolder: () -> Unit,
    onSelectZip: () -> Unit
) {
    Column(
        Modifier.fillMaxSize().verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        Text(
            "Update your game by selecting a new version from a folder or ZIP file. The bundle files will be overwritten directly.",
            style = MaterialTheme.typography.bodyMedium
        )
        Button(
            onClick = onSelectFolder,
            modifier = Modifier.fillMaxWidth().height(56.dp)
        ) {
            Text("Select Folder")
        }
        Button(
            onClick = onSelectZip,
            modifier = Modifier.fillMaxWidth().height(56.dp)
        ) {
            Text("Select ZIP")
        }
        Spacer(Modifier.weight(1f))
    }
}

@Composable
private fun UpdateCopyingPane(currentFile: String?) {
    Column(
        Modifier.fillMaxSize(),
        verticalArrangement = Arrangement.Center,
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        CircularProgressIndicator()
        Spacer(Modifier.height(16.dp))
        Text("Copying files...")
        if (currentFile != null) {
            Text(currentFile, style = MaterialTheme.typography.bodySmall, fontSize = 12.sp)
        }
    }
}

@Composable
private fun UpdateErrorPane(message: String, onBack: () -> Unit) {
    Column(
        Modifier.fillMaxSize().verticalScroll(rememberScrollState()),
        verticalArrangement = Arrangement.spacedBy(16.dp)
    ) {
        Text(message, style = MaterialTheme.typography.bodyMedium, textAlign = TextAlign.Start)
        Button(onClick = onBack, modifier = Modifier.fillMaxWidth()) {
            Text("Back")
        }
    }
}
