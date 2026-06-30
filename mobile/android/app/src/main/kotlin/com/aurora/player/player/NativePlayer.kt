package com.aurora.player.player

import android.view.Surface

/**
 * Kotlin wrapper around C++ aurora_jni shared library.
 * All heavy work (decode, AI interpolation, rendering) runs in native code.
 */
class NativePlayer {

    private var handle: Long = 0L

    init { handle = nativeCreate() }

    fun release() {
        if (handle != 0L) { nativeDestroy(handle); handle = 0L }
    }

    // Surface lifecycle
    fun surfaceCreated(surface: Surface) = nativeSurfaceCreated(handle, surface)
    fun surfaceChanged(w: Int, h: Int)   = nativeSurfaceChanged(handle, w, h)
    fun surfaceDestroyed()               = nativeSurfaceDestroyed(handle)

    // Playback
    fun open(path: String): Boolean = nativeOpen(handle, path)
    fun play()                      = nativePlay(handle)
    fun pause()                     = nativePause(handle)
    fun stop()                      = nativeStop(handle)
    fun seek(seconds: Double)       = nativeSeek(handle, seconds)

    val position: Double  get() = nativeGetPosition(handle)
    val duration:  Double  get() = nativeGetDuration(handle)

    // Settings
    fun setVolume(percent: Int)     = nativeSetVolume(handle, percent)
    fun setInterpolation(enabled: Boolean, targetFPS: Float = 60f) =
        nativeSetInterpolation(handle, enabled, targetFPS)
    fun setUpscaler(model: String)  = nativeSetUpscaler(handle, model)
    fun loadSubtitle(path: String)  = nativeLoadSubtitle(handle, path)

    // Diagnostics
    fun getBenchmarkStats(): String = nativeGetBenchmarkStats(handle)

    // ── JNI declarations ─────────────────────────────────────────────────────
    fun setSpeed(speed: Float)             = nativeSetSpeed(handle, speed)
    fun setSubtitleFontSize(sp: Int)       = nativeSetSubtitleFontSize(handle, sp)
    fun setSubtitleBold(bold: Boolean)     = nativeSetSubtitleBold(handle, bold)
    fun setSubtitleOutline(on: Boolean)    = nativeSetSubtitleOutline(handle, on)
    fun setSubtitleDelay(ms: Int)          = nativeSetSubtitleDelay(handle, ms)
    fun setSubtitleOffset(pct: Int)        = nativeSetSubtitleOffset(handle, pct)

    private external fun nativeCreate(): Long
    private external fun nativeDestroy(handle: Long)
    private external fun nativeSurfaceCreated(handle: Long, surface: Surface)
    private external fun nativeSurfaceChanged(handle: Long, w: Int, h: Int)
    private external fun nativeSurfaceDestroyed(handle: Long)
    private external fun nativeOpen(handle: Long, path: String): Boolean
    private external fun nativePlay(handle: Long)
    private external fun nativePause(handle: Long)
    private external fun nativeStop(handle: Long)
    private external fun nativeSeek(handle: Long, seconds: Double)
    private external fun nativeGetPosition(handle: Long): Double
    private external fun nativeGetDuration(handle: Long): Double
    private external fun nativeSetVolume(handle: Long, percent: Int)
    private external fun nativeSetInterpolation(handle: Long, enabled: Boolean, targetFPS: Float)
    private external fun nativeSetUpscaler(handle: Long, model: String)
    private external fun nativeLoadSubtitle(handle: Long, path: String)
    private external fun nativeGetBenchmarkStats(handle: Long): String

    private external fun nativeSetSpeed(handle: Long, speed: Float)
    private external fun nativeSetSubtitleFontSize(handle: Long, sp: Int)
    private external fun nativeSetSubtitleBold(handle: Long, bold: Boolean)
    private external fun nativeSetSubtitleOutline(handle: Long, on: Boolean)
    private external fun nativeSetSubtitleDelay(handle: Long, ms: Int)
    private external fun nativeSetSubtitleOffset(handle: Long, pct: Int)

        companion object {
        init { System.loadLibrary("aurora_jni") }
    }
}

