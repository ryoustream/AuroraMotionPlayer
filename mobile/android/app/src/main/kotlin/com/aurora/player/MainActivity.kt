package com.aurora.player

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.provider.Settings
import android.view.WindowManager
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.fragment.app.commit
import androidx.lifecycle.lifecycleScope
import com.aurora.player.databinding.ActivityMainBinding
import com.aurora.player.ui.PlayerFragment
import com.aurora.player.ui.pip.PiPManager
import com.aurora.player.ui.playlist.PlaylistFragment
import com.aurora.player.ui.subtitle.SubtitleFragment
import com.aurora.player.viewmodel.PlayerViewModel
import kotlinx.coroutines.launch

/**
 * Aurora Motion Player — MainActivity (S16)
 *
 * Added:
 *  - Runtime permission request (READ_MEDIA_VIDEO, READ_MEDIA_AUDIO / READ_EXTERNAL_STORAGE)
 *  - MANAGE_EXTERNAL_STORAGE flow for full filesystem access on Android 11+
 *  - Re-request on resume if permission was revoked
 */
class MainActivity : AppCompatActivity() {

    private lateinit var binding   : ActivityMainBinding
    private lateinit var pipManager: PiPManager
    private val vm: PlayerViewModel by viewModels()

    // ── Runtime permission launcher ───────────────────────────────────────────

    private val permLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { results ->
        val denied = results.filterValues { !it }.keys
        if (denied.isNotEmpty()) {
            Toast.makeText(
                this,
                "Akses media diperlukan untuk memutar video.\n" +
                "Buka Pengaturan → Izin → Berikan akses media.",
                Toast.LENGTH_LONG
            ).show()
        }
    }

    // ── SAF pickers ───────────────────────────────────────────────────────────

    private val videoFilePicker = registerForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri ->
        uri?.let {
            // Persist permission so we can read the file across sessions
            contentResolver.takePersistableUriPermission(
                it, Intent.FLAG_GRANT_READ_URI_PERMISSION
            )
            openUri(it)
        }
    }

    private val subtitleFilePicker = registerForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri ->
        uri?.let { u ->
            contentResolver.takePersistableUriPermission(
                u, Intent.FLAG_GRANT_READ_URI_PERMISSION
            )
            lifecycleScope.launch { vm.loadSubtitleFile(u) }
        }
    }

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        window.setDecorFitsSystemWindows(false)

        pipManager = PiPManager(this).also { it.register() }
        pipManager.onPlayPauseCallback = { vm.togglePlayPause() }
        pipManager.onStopCallback      = { vm.stop() }
        pipManager.onSeekCallback      = { delta -> vm.seekRelative(delta) }

        if (savedInstanceState == null) {
            supportFragmentManager.commit {
                replace(binding.fragmentContainer.id, PlayerFragment())
            }
        }

        // Request permissions immediately on first launch
        requestMediaPermissions()

        handleIntent(intent)
    }

    override fun onResume() {
        super.onResume()
        // Re-check after user returns from Settings
        requestMediaPermissions()
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        handleIntent(intent)
    }

    override fun onUserLeaveHint() {
        super.onUserLeaveHint()
        pipManager.onUserLeaveHint(
            isPlaying = vm.isPlaying.value == true,
            position  = vm.position.value ?: 0.0,
            duration  = vm.duration.value ?: 0.0
        )
    }

    override fun onPictureInPictureModeChanged(
        isInPictureInPictureMode: Boolean,
        newConfig: android.content.res.Configuration
    ) {
        super.onPictureInPictureModeChanged(isInPictureInPictureMode, newConfig)
        playerFragment()?.onPiPModeChanged(isInPictureInPictureMode)
        pipManager.onPiPModeChanged(isInPictureInPictureMode, vm.isPlaying.value == true)
    }

    override fun onDestroy() {
        pipManager.unregister()
        super.onDestroy()
    }

    // ── Permissions ───────────────────────────────────────────────────────────

    private fun requestMediaPermissions() {
        val needed = buildList {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                // Android 13+: granular media permissions
                if (!hasPermission(Manifest.permission.READ_MEDIA_VIDEO))
                    add(Manifest.permission.READ_MEDIA_VIDEO)
                if (!hasPermission(Manifest.permission.READ_MEDIA_AUDIO))
                    add(Manifest.permission.READ_MEDIA_AUDIO)
            } else {
                // Android 12 and below
                if (!hasPermission(Manifest.permission.READ_EXTERNAL_STORAGE))
                    add(Manifest.permission.READ_EXTERNAL_STORAGE)
            }
        }
        if (needed.isNotEmpty()) permLauncher.launch(needed.toTypedArray())

        // MANAGE_EXTERNAL_STORAGE: needed for scanning arbitrary directories
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            if (!Environment.isExternalStorageManager()) {
                AlertDialog.Builder(this)
                    .setTitle("Izin Penyimpanan Penuh")
                    .setMessage(
                        "Aurora memerlukan akses ke semua file untuk scan dan memutar video " +
                        "dari seluruh penyimpanan.\n\nKetuk OK untuk membuka pengaturan."
                    )
                    .setPositiveButton("Buka Pengaturan") { _, _ ->
                        val intent = Intent(
                            Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                            Uri.parse("package:$packageName")
                        )
                        startActivity(intent)
                    }
                    .setNegativeButton("Lewati") { d, _ -> d.dismiss() }
                    .show()
            }
        }
    }

    private fun hasPermission(perm: String) =
        ContextCompat.checkSelfPermission(this, perm) == PackageManager.PERMISSION_GRANTED

    // ── Navigation ────────────────────────────────────────────────────────────

    fun openPlaylist() {
        supportFragmentManager.commit {
            replace(binding.fragmentContainer.id, PlaylistFragment())
            addToBackStack("playlist")
        }
    }

    fun openSubtitlePanel() {
        supportFragmentManager.commit {
            replace(binding.fragmentContainer.id, SubtitleFragment())
            addToBackStack("subtitle")
        }
    }

    // ── Pickers ───────────────────────────────────────────────────────────────

    fun openFilePicker()     = videoFilePicker.launch(arrayOf("video/*", "audio/*"))
    fun openSubtitlePicker() = subtitleFilePicker.launch(
        arrayOf("text/plain", "application/x-subrip",
                "text/vtt",   "application/octet-stream"))

    // ── Intent handling ───────────────────────────────────────────────────────

    private fun handleIntent(intent: Intent?) {
        if (intent?.action == Intent.ACTION_VIEW) {
            intent.data?.let { uri ->
                // Persist URI permission if it's a content URI
                runCatching {
                    contentResolver.takePersistableUriPermission(
                        uri, Intent.FLAG_GRANT_READ_URI_PERMISSION
                    )
                }
                openUri(uri)
            }
        }
    }

    private fun openUri(uri: Uri) {
        playerFragment()?.openUri(uri)
    }

    private fun playerFragment(): PlayerFragment? =
        supportFragmentManager.findFragmentById(binding.fragmentContainer.id)
            as? PlayerFragment
}
