package com.aurora.player.benchmark

import android.content.Context
import android.os.Build
import java.io.File
import java.io.RandomAccessFile
import kotlin.math.roundToLong

// ============================================================================
//  Aurora Motion Player — GPUStats (Android)
//  Session 10: GPU Benchmark System
//
//  Reads GPU stats from /sys/class/kgsl (Adreno) or Mali sysfs paths.
//  Falls back to zero values when paths are unavailable.
// ============================================================================

data class GPUStats(
    val usagePct:    Float = 0f,
    val clockMHz:    Float = 0f,
    val temperatureC: Float = 0f,
    val vramUsedMB:  Long  = 0L,
    val vramTotalMB: Long  = 0L,
    val vendor:      String = "Unknown",
    val gpuName:     String = "Unknown",
)

data class CPUStats(
    val totalUsagePct: Float = 0f,
    val coreUsages:    List<Float> = emptyList(),
    val temperatureC:  Float = 0f,
)

data class BenchmarkStats(
    val gpu:           GPUStats = GPUStats(),
    val cpu:           CPUStats = CPUStats(),
    val renderFps:     Float = 0f,
    val decodeFps:     Float = 0f,
    val interpFps:     Float = 0f,
    val droppedFrames: Long  = 0L,
    val avgFrameMs:    Float = 0f,
)

object SystemMonitor {

    // ── GPU detection ─────────────────────────────────────────────────────────
    private val kgslBusyPaths = listOf(
        "/sys/class/kgsl/kgsl-3d0/gpu_busy_percentage",
        "/sys/kernel/gpu/gpu_busy",
        "/sys/devices/soc/kgsl-3d0/gpu_busy_percentage",
    )
    private val kgslFreqPaths = listOf(
        "/sys/class/kgsl/kgsl-3d0/gpuclk",
        "/sys/class/kgsl/kgsl-3d0/devfreq/cur_freq",
        "/sys/kernel/gpu/gpu_clock",
    )
    private val maliLoadPaths = listOf(
        "/sys/bus/platform/drivers/mali/gpu/utilization",
        "/sys/class/misc/mali0/device/utilization",
        "/sys/devices/platform/mali/utilization",
    )
    private val maliFreqPaths = listOf(
        "/sys/class/misc/mali0/device/clock",
        "/sys/devices/platform/mali/devfreq/cur_freq",
    )

    private val kgslBusy: File? = kgslBusyPaths.map(::File).firstOrNull { it.exists() }
    private val kgslFreq: File? = kgslFreqPaths.map(::File).firstOrNull { it.exists() }
    private val maliBusy: File? = maliLoadPaths.map(::File).firstOrNull { it.exists() }
    private val maliFreq: File? = maliFreqPaths.map(::File).firstOrNull { it.exists() }

    val isAdreno: Boolean = kgslBusy != null
    val isMali:   Boolean = !isAdreno && maliBusy != null

    val gpuVendor: String = when {
        isAdreno -> "Qualcomm"
        isMali   -> "ARM"
        else     -> "Unknown"
    }
    val gpuName: String = when {
        isAdreno -> detectAdrenoName()
        isMali   -> "Mali GPU"
        else     -> "Unknown GPU"
    }

    private fun detectAdrenoName(): String {
        // Try reading from /proc/cpuinfo or build props
        return try {
            File("/proc/cpuinfo").readLines()
                .firstOrNull { it.startsWith("Hardware") }
                ?.substringAfter(":")?.trim()
                ?.let { "Adreno ($it)" }
                ?: "Adreno ${Build.HARDWARE}"
        } catch (_: Exception) {
            "Adreno GPU"
        }
    }

    // ── sysfs helpers ─────────────────────────────────────────────────────────
    private fun readSysfs(f: File?): String = try { f?.readText()?.trim() ?: "" } catch (_: Exception) { "" }
    private fun readSysfsLong(f: File?, def: Long = 0L): Long =
        readSysfs(f).toLongOrNull() ?: def
    private fun readSysfsFloat(f: File?, def: Float = 0f): Float =
        readSysfs(f).trim().split(" ").firstOrNull()?.toFloatOrNull() ?: def

    // ── GPU sample ────────────────────────────────────────────────────────────
    fun sampleGPU(): GPUStats {
        val (usagePct, clockMHz) = when {
            isAdreno -> {
                val u = readSysfsFloat(kgslBusy)
                val hz = readSysfsLong(kgslFreq)
                Pair(u, hz / 1_000_000f)
            }
            isMali -> {
                val u = readSysfsFloat(maliBusy)
                val hz = readSysfsLong(maliFreq)
                Pair(u, hz / 1_000_000f)
            }
            else -> Pair(0f, 0f)
        }

        // GPU temperature — try thermal zones
        val tempC = readGpuTemperature()

        // VRAM: mobile GPUs share system RAM; approximate
        val memInfo = readMemInfo()
        return GPUStats(
            usagePct    = usagePct,
            clockMHz    = clockMHz,
            temperatureC = tempC,
            vramUsedMB  = (memInfo.totalKB - memInfo.availKB) / 1024L,
            vramTotalMB = memInfo.totalKB / 1024L,
            vendor      = gpuVendor,
            gpuName     = gpuName,
        )
    }

    private fun readGpuTemperature(): Float {
        // Try standard thermal zones for GPU
        for (i in 0..9) {
            val typeFile = File("/sys/class/thermal/thermal_zone$i/type")
            val tempFile = File("/sys/class/thermal/thermal_zone$i/temp")
            val type = readSysfs(typeFile).lowercase()
            if (type.contains("gpu") || type.contains("tsens_tz_sensor")) {
                val mC = readSysfsLong(tempFile)
                if (mC > 1000) return mC / 1000f  // millidegrees
            }
        }
        return 0f
    }

    // ── CPU sample ────────────────────────────────────────────────────────────
    private var prevCpuTimes: List<LongArray> = emptyList()

    data class MemInfo(val totalKB: Long, val availKB: Long)

    fun readMemInfo(): MemInfo {
        var total = 0L; var avail = 0L
        try {
            File("/proc/meminfo").forEachLine { line ->
                when {
                    line.startsWith("MemTotal:") ->
                        total = line.substringAfter(":").trim().split(" ")[0].toLongOrNull() ?: 0L
                    line.startsWith("MemAvailable:") ->
                        avail = line.substringAfter(":").trim().split(" ")[0].toLongOrNull() ?: 0L
                }
            }
        } catch (_: Exception) {}
        return MemInfo(total, avail)
    }

    fun sampleCPU(): CPUStats {
        return try {
            val lines = File("/proc/stat").readLines()
                .filter { it.startsWith("cpu") }
            if (lines.isEmpty()) return CPUStats()

            // Parse: cpu user nice system idle iowait irq softirq steal
            fun parseLine(line: String): LongArray {
                val parts = line.trimStart().split("\\s+".toRegex()).drop(1)
                return LongArray(parts.size) { parts.getOrNull(it)?.toLongOrNull() ?: 0L }
            }

            val cur = lines.map { parseLine(it) }
            if (prevCpuTimes.isEmpty() || prevCpuTimes.size != cur.size) {
                prevCpuTimes = cur
                return CPUStats()
            }

            val deltas = cur.zip(prevCpuTimes).map { (c, p) ->
                LongArray(c.size) { c[it] - p[it] }
            }
            prevCpuTimes = cur

            val coreUsages = deltas.drop(1).map { d ->  // skip aggregate (index 0)
                val total = d.sum()
                val idle  = d.getOrElse(3) { 0L }
                if (total > 0) (1f - idle.toFloat() / total.toFloat()) * 100f else 0f
            }
            val aggrDelta = deltas[0]
            val aggrTotal = aggrDelta.sum()
            val aggrIdle  = aggrDelta.getOrElse(3) { 0L }
            val totalUsage = if (aggrTotal > 0)
                (1f - aggrIdle.toFloat() / aggrTotal.toFloat()) * 100f else 0f

            // Temperature
            val tempC = try {
                File("/sys/class/thermal/thermal_zone0/temp")
                    .readText().trim().toLongOrNull()?.let { it / 1000f } ?: 0f
            } catch (_: Exception) { 0f }

            CPUStats(
                totalUsagePct = totalUsage,
                coreUsages    = coreUsages,
                temperatureC  = tempC,
            )
        } catch (_: Exception) { CPUStats() }
    }
}
