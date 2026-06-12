package com.aurora.player.viewmodel

import android.content.Context
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel

/**
 * Aurora Motion Player — Settings ViewModel
 *
 * Holds all player configuration. Persisted via SharedPreferences.
 * Observed by SettingsFragment and applied to the native player engine.
 */
class SettingsViewModel : ViewModel() {

    // ── Video ─────────────────────────────────────────────────────────────────
    private val _decoderIndex    = MutableLiveData(0)
    val decoderIndex: LiveData<Int> = _decoderIndex
    fun setDecoder(idx: Int) { _decoderIndex.value = idx }

    private val _hwAccelEnabled  = MutableLiveData(true)
    val hwAccelEnabled: LiveData<Boolean> = _hwAccelEnabled
    fun setHWAccel(on: Boolean) { _hwAccelEnabled.value = on }

    private val _rendererIndex   = MutableLiveData(0)
    val rendererIndex: LiveData<Int> = _rendererIndex
    fun setRenderer(idx: Int) { _rendererIndex.value = idx }

    private val _hdrEnabled      = MutableLiveData(false)
    val hdrEnabled: LiveData<Boolean> = _hdrEnabled
    fun setHDR(on: Boolean) { _hdrEnabled.value = on }

    private val _toneMappingIndex = MutableLiveData(0)
    val toneMappingIndex: LiveData<Int> = _toneMappingIndex
    fun setToneMapping(idx: Int) { _toneMappingIndex.value = idx }

    // ── AI ────────────────────────────────────────────────────────────────────
    private val _interpEnabled   = MutableLiveData(false)
    val interpEnabled: LiveData<Boolean> = _interpEnabled
    fun setInterp(on: Boolean) { _interpEnabled.value = on }

    private val _interpModelIndex = MutableLiveData(0)
    val interpModelIndex: LiveData<Int> = _interpModelIndex
    fun setInterpModel(idx: Int) { _interpModelIndex.value = idx }

    private val _targetFPSIndex   = MutableLiveData(1)
    val targetFPSIndex: LiveData<Int> = _targetFPSIndex
    fun setTargetFPS(idx: Int) { _targetFPSIndex.value = idx }

    private val _upscaleEnabled  = MutableLiveData(false)
    val upscaleEnabled: LiveData<Boolean> = _upscaleEnabled
    fun setUpscale(on: Boolean) { _upscaleEnabled.value = on }

    private val _upscalerIndex   = MutableLiveData(0)
    val upscalerIndex: LiveData<Int> = _upscalerIndex
    fun setUpscaler(idx: Int) { _upscalerIndex.value = idx }

    private val _upscaleModeIndex = MutableLiveData(0)
    val upscaleModeIndex: LiveData<Int> = _upscaleModeIndex
    fun setUpscaleMode(idx: Int) { _upscaleModeIndex.value = idx }

    private val _autoSelectEnabled = MutableLiveData(true)
    val autoSelectEnabled: LiveData<Boolean> = _autoSelectEnabled
    fun setAutoSelect(on: Boolean) { _autoSelectEnabled.value = on }

    private val _qualityLevel    = MutableLiveData(1)
    val qualityLevel: LiveData<Int> = _qualityLevel
    fun setQualityLevel(lvl: Int) { _qualityLevel.value = lvl }

    // ── Audio ─────────────────────────────────────────────────────────────────
    private val _passthroughEnabled = MutableLiveData(false)
    val passthroughEnabled: LiveData<Boolean> = _passthroughEnabled
    fun setPassthrough(on: Boolean) { _passthroughEnabled.value = on }

    private val _audioSyncOffset = MutableLiveData(0)
    val audioSyncOffset: LiveData<Int> = _audioSyncOffset
    fun setAudioSync(offsetMs: Int) { _audioSyncOffset.value = offsetMs }

    private val _normalizeAudio  = MutableLiveData(false)
    val normalizeAudio: LiveData<Boolean> = _normalizeAudio
    fun setNormalize(on: Boolean) { _normalizeAudio.value = on }

    // ── Subtitle ──────────────────────────────────────────────────────────────
    private val _subtitleFontSize = MutableLiveData(16)
    val subtitleFontSize: LiveData<Int> = _subtitleFontSize
    fun setSubtitleFontSize(size: Int) { _subtitleFontSize.value = size }

    private val _subtitleBold    = MutableLiveData(false)
    val subtitleBold: LiveData<Boolean> = _subtitleBold
    fun setSubtitleBold(on: Boolean) { _subtitleBold.value = on }

    private val _subtitleOutline = MutableLiveData(true)
    val subtitleOutline: LiveData<Boolean> = _subtitleOutline
    fun setSubtitleOutline(on: Boolean) { _subtitleOutline.value = on }

    private val _subtitleLangIndex = MutableLiveData(0)
    val subtitleLangIndex: LiveData<Int> = _subtitleLangIndex
    fun setSubtitleLang(idx: Int) { _subtitleLangIndex.value = idx }

    private val _subtitleDownloadRequest = MutableLiveData<Unit>()
    val subtitleDownloadRequest: LiveData<Unit> = _subtitleDownloadRequest
    fun requestSubtitleDownload() { _subtitleDownloadRequest.value = Unit }

    // ── Advanced ──────────────────────────────────────────────────────────────
    private val _debugOverlay    = MutableLiveData(false)
    val debugOverlay: LiveData<Boolean> = _debugOverlay
    fun setDebugOverlay(on: Boolean) { _debugOverlay.value = on }

    private val _bufferSizeIndex = MutableLiveData(2)
    val bufferSizeIndex: LiveData<Int> = _bufferSizeIndex
    fun setBufferSize(idx: Int) { _bufferSizeIndex.value = idx }

    private val _vsyncEnabled    = MutableLiveData(true)
    val vsyncEnabled: LiveData<Boolean> = _vsyncEnabled
    fun setVSync(on: Boolean) { _vsyncEnabled.value = on }

    fun clearCache(ctx: Context) {
        ctx.cacheDir.deleteRecursively()
        ctx.externalCacheDir?.deleteRecursively()
    }
}
