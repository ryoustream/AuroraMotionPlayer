package com.aurora.player

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.WindowManager
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import com.aurora.player.databinding.ActivityMainBinding
import com.aurora.player.player.NativePlayer
import com.aurora.player.ui.PlayerFragment
import kotlinx.coroutines.launch

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        // Keep screen on during playback
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        // Edge-to-edge immersive
        window.setDecorFitsSystemWindows(false)

        // Handle intent (file open / deep link)
        handleIntent(intent)

        if (savedInstanceState == null) {
            supportFragmentManager.beginTransaction()
                .replace(binding.fragmentContainer.id, PlayerFragment())
                .commit()
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        handleIntent(intent)
    }

    private fun handleIntent(intent: Intent?) {
        when (intent?.action) {
            Intent.ACTION_VIEW -> {
                val uri = intent.data ?: return
                openUri(uri)
            }
        }
    }

    private fun openUri(uri: Uri) {
        lifecycleScope.launch {
            // Pass URI to PlayerFragment via ViewModel / shared state
            val fragment = supportFragmentManager
                .findFragmentById(binding.fragmentContainer.id)
            if (fragment is PlayerFragment) {
                fragment.openUri(uri)
            }
        }
    }

    // File picker (SAF)
    val filePicker = registerForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri ->
        uri?.let { openUri(it) }
    }

    fun openFilePicker() {
        filePicker.launch(arrayOf("video/*", "audio/*"))
    }
}
