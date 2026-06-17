package com.aurora.player

import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Bundle
import android.view.WindowManager
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.commit
import androidx.lifecycle.lifecycleScope
import com.aurora.player.databinding.ActivityMainBinding
import com.aurora.player.ui.PlayerFragment
import com.aurora.player.ui.pip.PiPManager
import com.aurora.player.ui.playlist.PlaylistFragment
import com.aurora.player.ui.subtitle.SubtitleFragment
import com.aurora.player.viewmodel.PlayerViewModel
import androidx.activity.viewModels
import kotlinx.coroutines.launch

/**
 * Aurora Motion Player — MainActivity (Session 7 update)
 *
 * Hosts all fragments via FragmentContainerView.
 * Wires PiPManager, SAF pickers, and intent handling.
 */
class MainActivity : AppCompatActivity() {

    private lateinit var binding   : ActivityMainBinding
    private lateinit var pipManager: PiPManager
    private val vm: PlayerViewModel by viewModels()

    // ── SAF pickers ───────────────────────────────────────────────────────────

    private val videoFilePicker = registerForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri -> uri?.let { openUri(it) } }

    private val subtitleFilePicker = registerForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri ->
        uri?.let { u ->
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

        // Wire PiP callbacks to ViewModel
        pipManager.onPlayPauseCallback = { vm.togglePlayPause() }
        pipManager.onStopCallback      = { vm.stop() }
        pipManager.onSeekCallback      = { delta -> vm.seekRelative(delta) }

        if (savedInstanceState == null) {
            supportFragmentManager.commit {
                replace(binding.fragmentContainer.id, PlayerFragment())
            }
        }

        handleIntent(intent)
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        handleIntent(intent)
    }

    override fun onUserLeaveHint() {
        super.onUserLeaveHint()
        // Auto-enter PiP when user presses home during playback
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
            intent.data?.let { openUri(it) }
        }
    }

    private fun openUri(uri: Uri) {
        playerFragment()?.openUri(uri)
    }

    private fun playerFragment(): PlayerFragment? =
        supportFragmentManager.findFragmentById(binding.fragmentContainer.id)
            as? PlayerFragment
}
