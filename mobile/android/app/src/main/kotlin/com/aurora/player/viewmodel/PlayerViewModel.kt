package com.aurora.player.viewmodel

import android.app.Application
import android.net.Uri
import android.os.Build
import androidx.annotation.OptIn
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.viewModelScope
import androidx.media3.common.MediaItem
import androidx.media3.common.PlaybackException
import androidx.media3.common.Player
import androidx.media3.common.util.UnstableApi
import androidx.media3.exoplayer.ExoPlayer
import androidx.media3.exoplayer.source.DefaultMediaSourceFactory
import com.aurora.player.player.NativePlayer
import com.aurora.player.util.UriUtils
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import java.io.File

/**
 * Aurora Motion Player — PlayerViewModel (S16)
 *
 * Key changes vs S7:
 *  - ExoPlayer is now the PRIMARY playback engine (handles all content:// / file:// / http:// URIs)
 *  - NativePlayer kept as AI overlay layer (frame interpolation, upscaling)
 *  - open(uri) resolves URI and passes to ExoPlayer directly
 *  - Position/duration polled from ExoPlayer (not native stubs)
 *  - ExoPlayer listener updates isPlaying, isBuffering, errorMsg LiveData
 */
@OptIn(UnstableApi::class)
class PlayerViewModel(app: Application) : AndroidViewModel(app) {

    // ── ExoPlayer (primary engine) ────────────────────────────────────────────
    val exoPlayer: ExoPlayer = ExoPlayer.Builder(app)
        .setMediaSourceFactory(DefaultMediaSourceFactory(app))
        .build()
        .also { p ->
            p.addListener(object : Player.Listener {
                override fun onIsPlayingChanged(playing: Boolean) {
                    isPlaying.postValue(playing)
                }
                override fun onPlaybackStateChanged(state: Int) {
                    isBuffering.postValue(state == Player.STATE_BUFFERING)
                }
                override fun onPlayerError(error: PlaybackException) {
                    errorMsg.postValue("Playback error: ${error.message}")
                }
                override fun onMediaItemTransition(item: MediaItem?, reason: Int) {
                    title.postValue(
                        item?.mediaMetadata?.title?.toString()
                            ?: item?.localConfiguration?.uri?.lastPathSegment
                            ?: "Aurora Player"
                    )
                }
            })
        }

    // ── NativePlayer (AI overlay — frame interpolation / upscaling) ───────────
    val player = NativePlayer()

    // ── Playback state LiveData ────────────────────────────────────────────────
    val isPlaying    = MutableLiveData(false)
    val position     = MutableLiveData(0.0)
    val duration     = MutableLiveData(0.0)
    val isBuffering  = MutableLiveData(false)
    val title        = MutableLiveData<String?>(null)
    val speed        = MutableLiveData(1.0)
    val errorMsg     = MutableLiveData<String?>(null)

    // ── AI / stats ────────────────────────────────────────────────────────────
    val benchStats   = MutableLiveData<String?>(null)
    private var aiEnabled = false

    // ── Subtitle ──────────────────────────────────────────────────────────────
    val subtitleText = MutableLiveData<String?>(null)

    init {
        // Poll position every 200 ms from ExoPlayer
        viewModelScope.launch(Dispatchers.Main) {
            while (isActive) {
                val pos = exoPlayer.currentPosition.coerceAtLeast(0L)
                val dur = exoPlayer.duration.takeIf { it > 0 } ?: 0L
                position.value = pos / 1000.0
                duration.value = dur / 1000.0
                delay(200L)
            }
        }
    }

    // ── Open ──────────────────────────────────────────────────────────────────

    /**
     * Primary open: pass URI directly to ExoPlayer.
     * ExoPlayer handles content://, file://, http(s)://, rtsp:// natively.
     */
    fun open(uri: Uri) {
        viewModelScope.launch(Dispatchers.Main) {
            val mediaItem = MediaItem.Builder()
                .setUri(uri)
                .build()
            exoPlayer.stop()
            exoPlayer.clearMediaItems()
            exoPlayer.setMediaItem(mediaItem)
            exoPlayer.prepare()
            exoPlayer.playWhenReady = true
            title.value = uri.lastPathSegment ?: "Aurora Player"

            // Also try to open in native for AI overlay (non-critical)
            viewModelScope.launch(Dispatchers.IO) {
                try {
                    val path = UriUtils.resolveToPath(getApplication(), uri)
                    if (!path.isNullOrBlank()) {
                        player.open(path)
                    }
                } catch (_: Exception) {}
            }
        }
    }

    /** Open a pre-resolved path or network URL. */
    fun openPath(path: String, displayTitle: String? = null) {
        val uri = when {
            path.startsWith("content://") -> Uri.parse(path)
            path.startsWith("http://") || path.startsWith("https://") -> Uri.parse(path)
            path.startsWith("rtsp://")  || path.startsWith("rtmp://")  -> Uri.parse(path)
            else -> Uri.fromFile(File(path))
        }
        viewModelScope.launch(Dispatchers.Main) {
            val mediaItem = MediaItem.Builder()
                .setUri(uri)
                .apply { displayTitle?.let { setMediaMetadata(
                    androidx.media3.common.MediaMetadata.Builder().setTitle(it).build()
                ) } }
                .build()
            exoPlayer.stop()
            exoPlayer.clearMediaItems()
            exoPlayer.setMediaItem(mediaItem)
            exoPlayer.prepare()
            exoPlayer.playWhenReady = true
            title.value = displayTitle ?: path.substringAfterLast('/')
        }
    }

    // ── Transport ─────────────────────────────────────────────────────────────

    fun togglePlayPause() {
        if (exoPlayer.isPlaying) exoPlayer.pause() else exoPlayer.play()
    }

    fun stop() {
        exoPlayer.stop()
        exoPlayer.clearMediaItems()
        isPlaying.value    = false
        position.value     = 0.0
        subtitleText.value = null
        title.value        = null
        player.stop()
    }

    fun seek(seconds: Double) {
        val ms = (seconds * 1000).toLong().coerceAtLeast(0L)
        exoPlayer.seekTo(ms)
        player.seek(seconds)
    }

    fun seekRelative(deltaSec: Double) {
        val newSec = ((position.value ?: 0.0) + deltaSec).coerceAtLeast(0.0)
        seek(newSec)
    }

    // ── Speed ─────────────────────────────────────────────────────────────────

    fun setSpeed(s: Double) {
        val clamped = s.coerceIn(0.05, 4.0)
        speed.value = clamped
        exoPlayer.setPlaybackSpeed(clamped.toFloat())
        player.setSpeed(clamped.toFloat())
    }

    // ── Volume ────────────────────────────────────────────────────────────────

    fun setVolume(percent: Int) {
        exoPlayer.volume = percent / 100f
        player.setVolume(percent)
    }

    // ── AI ────────────────────────────────────────────────────────────────────

    fun toggleAI() {
        aiEnabled = !aiEnabled
        player.setInterpolation(aiEnabled, 60f)
    }

    fun enableInterpolation(enabled: Boolean, targetFPS: Float = 60f) {
        aiEnabled = enabled
        player.setInterpolation(enabled, targetFPS)
    }

    fun setUpscaler(model: String) = player.setUpscaler(model)

    // ── Subtitle ──────────────────────────────────────────────────────────────

    fun loadSubtitle(path: String) = player.loadSubtitle(path)

    fun loadSubtitleFile(uri: Uri) {
        viewModelScope.launch(Dispatchers.IO) {
            val path = UriUtils.resolveToPath(getApplication(), uri)
                ?: UriUtils.copyToCache(getApplication(), uri)
            if (!path.isNullOrBlank()) player.loadSubtitle(path)
        }
    }

    fun setSubtitleFontSize(sp: Int)    = player.setSubtitleFontSize(sp)
    fun setSubtitleBold(bold: Boolean)  = player.setSubtitleBold(bold)
    fun setSubtitleOutline(on: Boolean) = player.setSubtitleOutline(on)
    fun setSubtitleDelay(ms: Int)       = player.setSubtitleDelay(ms)
    fun setSubtitleOffset(pct: Int)     = player.setSubtitleOffset(pct)

    // ── Settings passthrough ──────────────────────────────────────────────────

    val decoderIndex      = MutableLiveData(0)
    val hwAccelEnabled    = MutableLiveData(true)
    val rendererIndex     = MutableLiveData(0)
    val hdrEnabled        = MutableLiveData(true)
    val toneMappingIndex  = MutableLiveData(0)
    val interpEnabled     = MutableLiveData(false)
    val interpModelIndex  = MutableLiveData(0)
    val targetFPSIndex    = MutableLiveData(1)
    val upscaleEnabled    = MutableLiveData(false)
    val upscalerIndex     = MutableLiveData(0)
    val upscaleModeIndex  = MutableLiveData(0)
    val autoSelectEnabled = MutableLiveData(true)
    val qualityLevel      = MutableLiveData(1)
    val passthroughEnabled= MutableLiveData(false)
    val audioSyncOffset   = MutableLiveData(0)
    val normalizeAudio    = MutableLiveData(false)
    val subtitleFontSize  = MutableLiveData(18)
    val subtitleBold      = MutableLiveData(false)
    val subtitleOutline   = MutableLiveData(true)
    val subtitleLangIndex = MutableLiveData(0)
    val debugOverlay      = MutableLiveData(false)
    val bufferSizeIndex   = MutableLiveData(2)
    val vsyncEnabled      = MutableLiveData(true)

    // ── Cleanup ───────────────────────────────────────────────────────────────

    override fun onCleared() {
        exoPlayer.release()
        player.stop()
        player.release()
        super.onCleared()
    }
}
