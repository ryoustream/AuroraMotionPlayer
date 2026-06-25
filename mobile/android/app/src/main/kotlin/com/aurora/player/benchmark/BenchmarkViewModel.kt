package com.aurora.player.benchmark

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

// ============================================================================
//  Aurora Motion Player — BenchmarkViewModel
//  Session 10: GPU Benchmark System
// ============================================================================

class BenchmarkViewModel : ViewModel() {

    private val _stats = MutableLiveData<BenchmarkStats>()
    val stats: LiveData<BenchmarkStats> = _stats

    // Frame counters (updated from player pipeline via JNI callbacks)
    private var renderFrameCount = 0L
    private var decodeFrameCount = 0L
    private var interpFrameCount = 0L
    private var droppedCount     = 0L
    private var pollJob: Job?    = null
    private var startMs          = System.currentTimeMillis()

    // Rolling frame-time tracking
    private val frameDurQueue = ArrayDeque<Float>(60)

    fun startPolling(intervalMs: Long = 500L) {
        startMs = System.currentTimeMillis()
        pollJob?.cancel()
        pollJob = viewModelScope.launch {
            while (isActive) {
                val gpu = SystemMonitor.sampleGPU()
                val cpu = SystemMonitor.sampleCPU()
                val elapsedSec = (System.currentTimeMillis() - startMs) / 1000.0

                val avgFrameMs = if (frameDurQueue.isNotEmpty())
                    frameDurQueue.average().toFloat() else 0f

                _stats.postValue(
                    BenchmarkStats(
                        gpu           = gpu,
                        cpu           = cpu,
                        renderFps     = if (elapsedSec > 0) (renderFrameCount / elapsedSec).toFloat() else 0f,
                        decodeFps     = if (elapsedSec > 0) (decodeFrameCount / elapsedSec).toFloat() else 0f,
                        interpFps     = if (elapsedSec > 0) (interpFrameCount / elapsedSec).toFloat() else 0f,
                        droppedFrames = droppedCount,
                        avgFrameMs    = avgFrameMs,
                    )
                )
                delay(intervalMs)
            }
        }
    }

    fun stopPolling() { pollJob?.cancel() }

    // ── Frame event hooks (call from JNI bridge / player) ────────────────────
    fun onFrameRendered() { renderFrameCount++ }
    fun onFrameDecoded()  { decodeFrameCount++ }
    fun onFrameInterpolated() { interpFrameCount++ }
    fun onFrameDropped()  { droppedCount++ }

    fun onFrameTime(ms: Float) {
        if (frameDurQueue.size >= 60) frameDurQueue.removeFirst()
        frameDurQueue.addLast(ms)
    }

    fun reset() {
        renderFrameCount = 0; decodeFrameCount = 0
        interpFrameCount = 0; droppedCount = 0
        frameDurQueue.clear()
        startMs = System.currentTimeMillis()
    }

    override fun onCleared() {
        super.onCleared()
        stopPolling()
    }
}
