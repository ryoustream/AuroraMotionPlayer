package com.aurora.player.viewmodel

import android.app.Application
import android.net.Uri
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.viewModelScope
import com.aurora.player.player.NativePlayer
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

/**
 * Aurora Motion Player — PlayerViewModel (Session 7 update)
 *
 * New in S7:
 *  - openPath(path, title)      : open pre-resolved filesystem/network path
 *  - seekRelative(delta)        : seek ±N seconds
 *  - setSpeed(speed)            : playback speed (0.05 – 4.0)
 *  - toggleAI()                 : toggle interpolation on/off
 *  - loadSubtitleFile(uri)      : load external subtitle via URI
 *  - setSubtitleFontSize / Bold / Outline / Delay / Offset
 *  - title / subtitleText / isBuffering / speed LiveData
 */
class PlayerViewModel(app: Application) : AndroidViewModel(app) {

    val player = NativePlayer()

    // ── Playback state ────────────────────────────────────────────────────────
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
        // Poll position + stats every 200 ms
        viewModelScope.launch(Dispatchers.IO) {
            while (isActive) {
                position.postValue(player.position)
                duration.postValue(player.duration)
                val stats = player.getBenchmarkStats()
                benchStats.postValue(stats.takeIf { it.isNotBlank() && aiEnabled })
                delay(200L)
            }
        }
    }

    // ── Open ──────────────────────────────────────────────────────────────────

    /** Open a file by content URI (resolves path internally). */
    fun open(uri: Uri) = openPath(uri.toString(), uri.lastPathSegment)

    /** Open a pre-resolved path or network URL. */
    fun openPath(path: String, displayTitle: String? = null) {
        viewModelScope.launch(Dispatchers.IO) {
            isBuffering.postValue(true)
            if (player.open(path)) {
                player.play()
                isPlaying.postValue(true)
                title.postValue(displayTitle ?: path.substringAfterLast('/'))
            } else {
                errorMsg.postValue("Cannot open: ${path.substringAfterLast('/')}")
            }
            isBuffering.postValue(false)
        }
    }

    // ── Transport ─────────────────────────────────────────────────────────────

    fun togglePlayPause() {
        if (isPlaying.value == true) {
            player.pause()
            isPlaying.value = false
        } else {
            player.play()
            isPlaying.value = true
        }
    }

    fun stop() {
        player.stop()
        isPlaying.value    = false
        position.value     = 0.0
        subtitleText.value = null
        title.value        = null
    }

    fun seek(seconds: Double) {
        val clamped = seconds.coerceIn(0.0, duration.value ?: 0.0)
        player.seek(clamped)
    }

    fun seekRelative(deltaSec: Double) {
        val newPos = ((position.value ?: 0.0) + deltaSec)
            .coerceIn(0.0, duration.value ?: 0.0)
        player.seek(newPos)
    }

    // ── Speed ─────────────────────────────────────────────────────────────────

    fun setSpeed(s: Double) {
        val clamped = s.coerceIn(0.05, 4.0)
        speed.value = clamped
        player.setSpeed(clamped.toFloat())
    }

    // ── Volume ────────────────────────────────────────────────────────────────

    fun setVolume(percent: Int) = player.setVolume(percent)

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
            player.loadSubtitle(uri.toString())
        }
    }

    fun setSubtitleFontSize(sp: Int)   = player.setSubtitleFontSize(sp)
    fun setSubtitleBold(bold: Boolean) = player.setSubtitleBold(bold)
    fun setSubtitleOutline(on: Boolean)= player.setSubtitleOutline(on)
    fun setSubtitleDelay(ms: Int)      = player.setSubtitleDelay(ms)
    fun setSubtitleOffset(pct: Int)    = player.setSubtitleOffset(pct)

    // ── Settings passthrough (from SettingsViewModel integration) ─────────────

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
        player.stop()
        player.release()
        super.onCleared()
    }
}
