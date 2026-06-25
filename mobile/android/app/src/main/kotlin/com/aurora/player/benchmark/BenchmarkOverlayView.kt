package com.aurora.player.benchmark

import android.content.Context
import android.graphics.*
import android.util.AttributeSet
import android.view.View
import androidx.annotation.ColorInt
import kotlin.math.max
import kotlin.math.min

// ============================================================================
//  Aurora Motion Player — BenchmarkOverlayView
//  Session 10: GPU Benchmark System
//
//  Custom View that renders a transparent benchmark HUD on the player surface.
//  Renders FPS, CPU/GPU usage, VRAM, temperature, mini graphs.
// ============================================================================

class BenchmarkOverlayView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
) : View(context, attrs) {

    private var stats: BenchmarkStats = BenchmarkStats()

    // ── History buffers ───────────────────────────────────────────────────────
    private val fpsHistory = ArrayDeque<Float>(GRAPH_POINTS)
    private val gpuHistory = ArrayDeque<Float>(GRAPH_POINTS)
    private val cpuHistory = ArrayDeque<Float>(GRAPH_POINTS)

    // ── Paints ────────────────────────────────────────────────────────────────
    private val bgPaint = Paint().apply {
        color = Color.argb(200, 10, 12, 18)
        isAntiAlias = true
    }
    private val labelPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(160, 170, 190)
        textSize = 28f
        typeface = Typeface.MONOSPACE
    }
    private val valuePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(230, 235, 245)
        textSize = 28f
        typeface = Typeface.create(Typeface.MONOSPACE, Typeface.BOLD)
    }
    private val headerPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.rgb(80, 200, 255)
        textSize = 28f
        typeface = Typeface.create(Typeface.MONOSPACE, Typeface.BOLD)
    }
    private val dividerPaint = Paint().apply {
        color = Color.argb(150, 50, 55, 70)
        strokeWidth = 1.5f
    }
    private val barBgPaint = Paint().apply { color = Color.rgb(40, 45, 55) }
    private val barFgPaint = Paint()
    private val graphBgPaint = Paint().apply { color = Color.rgb(20, 24, 32) }
    private val graphLinePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE
        strokeWidth = 3f
    }
    private val graphFillPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.FILL
    }
    private val cornerPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(210, 10, 12, 18)
    }

    private val bgRect = RectF()
    private val barRect = RectF()
    private val graphRect = RectF()
    private val graphPath = Path()
    private val fillPath  = Path()

    companion object {
        private const val GRAPH_POINTS = 60
        private const val PAD   = 16f
        private const val ROW_H = 42f
        private const val BAR_H = 10f
        private const val BAR_W = 160f
        private const val LABEL_W = 190f
        private const val GRAPH_H = 80f
        private const val CORNER_R = 10f
        private const val WIDTH = 660f

        @ColorInt
        fun usageColor(pct: Float): Int {
            return when {
                pct < 50 -> lerpColor(Color.rgb(0, 200, 80), Color.rgb(220, 220, 0), pct / 50f)
                pct < 80 -> lerpColor(Color.rgb(220, 220, 0), Color.rgb(255, 120, 0), (pct-50)/30f)
                else     -> lerpColor(Color.rgb(255, 120, 0), Color.rgb(255, 40, 40), (pct-80)/20f)
            }
        }

        @ColorInt
        fun lerpColor(@ColorInt a: Int, @ColorInt b: Int, t: Float): Int {
            val f = t.coerceIn(0f, 1f)
            val r = Color.red(a)   + (f * (Color.red(b)   - Color.red(a))).toInt()
            val g = Color.green(a) + (f * (Color.green(b) - Color.green(a))).toInt()
            val bl= Color.blue(a)  + (f * (Color.blue(b)  - Color.blue(a))).toInt()
            return Color.rgb(r, g, bl)
        }
    }

    fun updateStats(newStats: BenchmarkStats) {
        stats = newStats
        fun push(q: ArrayDeque<Float>, v: Float) {
            if (q.size >= GRAPH_POINTS) q.removeFirst()
            q.addLast(v)
        }
        push(fpsHistory, newStats.renderFps)
        push(gpuHistory, newStats.gpu.usagePct)
        push(cpuHistory, newStats.cpu.totalUsagePct)
        invalidate()
    }

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        // Calculate height based on content
        val h = PAD * 2 + ROW_H + 4 +   // header
                ROW_H * 4 + 4 +           // FPS + dropped
                ROW_H * 3 + 4 +           // CPU/GPU/VRAM
                ROW_H     + 4 +           // frame time
                16 + GRAPH_H + 4 +        // FPS graph
                16 + GRAPH_H + 4 +        // GPU/CPU graph
                ROW_H                     // footer
        setMeasuredDimension(WIDTH.toInt(), h.toInt())
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        bgRect.set(0f, 0f, width.toFloat(), height.toFloat())
        canvas.drawRoundRect(bgRect, CORNER_R, CORNER_R, bgPaint)

        var y = PAD

        // ── Header ──────────────────────────────────────────────────────────
        canvas.drawText("Aurora Motion Player — Benchmark", PAD, y + ROW_H * 0.7f, headerPaint)
        y += ROW_H + 2
        canvas.drawLine(PAD, y, width - PAD, y, dividerPaint); y += 6f

        // ── FPS ─────────────────────────────────────────────────────────────
        drawRow(canvas, y, "Render FPS", "%.1f".format(stats.renderFps),
                stats.renderFps / 120f * 100f, Color.rgb(0, 210, 130))
        y += ROW_H
        drawRow(canvas, y, "Decode FPS", "%.1f".format(stats.decodeFps),
                stats.decodeFps / 120f * 100f, Color.rgb(60, 180, 255))
        y += ROW_H
        drawRow(canvas, y, "Interp FPS", "%.1f".format(stats.interpFps),
                stats.interpFps / 120f * 100f, Color.rgb(180, 100, 255))
        y += ROW_H
        drawRow(canvas, y, "Dropped", "${stats.droppedFrames}", -1f, 0)
        y += ROW_H

        canvas.drawLine(PAD, y, width - PAD, y, dividerPaint); y += 6f

        // ── CPU / GPU / VRAM ────────────────────────────────────────────────
        drawRow(canvas, y, "CPU",
                "%.1f%%  %.0f°C".format(stats.cpu.totalUsagePct, stats.cpu.temperatureC),
                stats.cpu.totalUsagePct, usageColor(stats.cpu.totalUsagePct))
        y += ROW_H
        drawRow(canvas, y, "GPU",
                "%.1f%%  %.0f°C".format(stats.gpu.usagePct, stats.gpu.temperatureC),
                stats.gpu.usagePct, usageColor(stats.gpu.usagePct))
        y += ROW_H
        val vramPct = if (stats.gpu.vramTotalMB > 0)
            100f * stats.gpu.vramUsedMB / stats.gpu.vramTotalMB else 0f
        drawRow(canvas, y, "VRAM",
                "${stats.gpu.vramUsedMB}/${stats.gpu.vramTotalMB} MB",
                vramPct, usageColor(vramPct))
        y += ROW_H

        canvas.drawLine(PAD, y, width - PAD, y, dividerPaint); y += 6f

        // ── Frame time ──────────────────────────────────────────────────────
        drawRow(canvas, y, "Frame Time",
                "%.2f ms".format(stats.avgFrameMs),
                (stats.avgFrameMs / 33f * 100f).coerceIn(0f, 100f), Color.rgb(255, 180, 0))
        y += ROW_H

        canvas.drawLine(PAD, y, width - PAD, y, dividerPaint); y += 6f

        // ── FPS Graph ───────────────────────────────────────────────────────
        drawGraphLabel(canvas, y, "FPS (0–120)")
        y += 16
        graphRect.set(PAD, y, width - PAD, y + GRAPH_H)
        canvas.drawRect(graphRect, graphBgPaint)
        drawGraph(canvas, graphRect, fpsHistory, 120f, Color.rgb(0, 210, 130))
        y += GRAPH_H + 6

        // ── GPU/CPU Graph ───────────────────────────────────────────────────
        drawGraphLabel(canvas, y, "GPU (orange) / CPU (blue) %")
        y += 16
        graphRect.set(PAD, y, width - PAD, y + GRAPH_H)
        canvas.drawRect(graphRect, graphBgPaint)
        drawGraph(canvas, graphRect, gpuHistory, 100f, Color.rgb(255, 120, 40))
        drawGraph(canvas, graphRect, cpuHistory, 100f, Color.rgb(80, 160, 255))
        y += GRAPH_H + 6

        canvas.drawLine(PAD, y, width - PAD, y, dividerPaint); y += 4f

        // ── Footer (GPU name) ───────────────────────────────────────────────
        val footer = if (stats.gpu.gpuName.isNotBlank())
            "[${stats.gpu.vendor}] ${stats.gpu.gpuName}" else "GPU: Unknown"
        labelPaint.textSize = 22f
        canvas.drawText(footer, PAD, y + ROW_H * 0.7f, labelPaint)
        labelPaint.textSize = 28f
    }

    private fun drawRow(canvas: Canvas, y: Float, label: String, value: String,
                         pct: Float, @ColorInt barColor: Int) {
        canvas.drawText(label, PAD, y + ROW_H * 0.68f, labelPaint)
        canvas.drawText(value, PAD + LABEL_W, y + ROW_H * 0.68f, valuePaint)

        if (pct >= 0f) {
            val bx = width - PAD - BAR_W
            val by = y + (ROW_H - BAR_H) / 2f
            barRect.set(bx, by, bx + BAR_W, by + BAR_H)
            canvas.drawRect(barRect, barBgPaint)
            val fill = (BAR_W * pct.coerceIn(0f, 100f) / 100f)
            if (fill > 0f) {
                barFgPaint.color = barColor
                barRect.right = bx + fill
                canvas.drawRect(barRect, barFgPaint)
            }
        }
    }

    private fun drawGraphLabel(canvas: Canvas, y: Float, label: String) {
        labelPaint.textSize = 22f
        canvas.drawText(label, PAD, y + 14f, labelPaint)
        labelPaint.textSize = 28f
    }

    private fun drawGraph(canvas: Canvas, r: RectF, data: ArrayDeque<Float>,
                           maxVal: Float, @ColorInt lineColor: Int) {
        if (data.size < 2) return
        val xStep = r.width() / (GRAPH_POINTS - 1).toFloat()
        val start = max(0, data.size - GRAPH_POINTS)

        fun yPos(v: Float) = r.bottom - (if (maxVal > 0) v / maxVal else 0f) * r.height()

        fillPath.reset()
        graphPath.reset()
        var first = true
        for (i in start until data.size) {
            val x = r.left + (i - start) * xStep
            val y = yPos(data[i])
            if (first) {
                fillPath.moveTo(r.left, r.bottom)
                fillPath.lineTo(x, y)
                graphPath.moveTo(x, y)
                first = false
            } else {
                fillPath.lineTo(x, y)
                graphPath.lineTo(x, y)
            }
        }
        fillPath.lineTo(r.right, r.bottom)
        fillPath.close()

        graphFillPaint.color = (lineColor and 0x00FFFFFF) or 0x3C000000
        canvas.drawPath(fillPath, graphFillPaint)

        graphLinePaint.color = lineColor
        canvas.drawPath(graphPath, graphLinePaint)
    }
}
