package com.aurora.player.ui

import android.net.Uri
import android.os.Bundle
import android.view.*
import android.widget.*
import androidx.fragment.app.Fragment
import androidx.fragment.app.viewModels
import com.aurora.player.MainActivity
import com.aurora.player.viewmodel.PlayerViewModel

class PlayerFragment : Fragment() {

    private val vm: PlayerViewModel by viewModels()
    private var surfaceView: SurfaceView? = null

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        // Full-screen SurfaceView with control overlay
        val frame = FrameLayout(requireContext()).apply {
            setBackgroundColor(android.graphics.Color.BLACK)
        }

        surfaceView = SurfaceView(requireContext()).also { sv ->
            sv.layoutParams = FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT
            )
            frame.addView(sv)
        }

        // Control panel
        val controls = buildControlsView()
        frame.addView(controls, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.WRAP_CONTENT,
            Gravity.BOTTOM
        ))

        // Benchmark overlay
        val benchText = TextView(requireContext()).apply {
            setTextColor(android.graphics.Color.rgb(0, 220, 120))
            textSize = 10f
            setPadding(8, 8, 8, 8)
            setBackgroundColor(android.graphics.Color.argb(140, 0, 0, 0))
        }
        frame.addView(benchText, FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.WRAP_CONTENT,
            FrameLayout.LayoutParams.WRAP_CONTENT,
            Gravity.TOP or Gravity.START
        ))

        // Surface callback
        surfaceView?.holder?.addCallback(object : SurfaceHolder.Callback {
            override fun surfaceCreated(h: SurfaceHolder) =
                vm.player.surfaceCreated(h.surface)
            override fun surfaceChanged(h: SurfaceHolder, fmt: Int, w: Int, ht: Int) =
                vm.player.surfaceChanged(w, ht)
            override fun surfaceDestroyed(h: SurfaceHolder) =
                vm.player.surfaceDestroyed()
        })

        // Observers
        vm.benchStats.observe(viewLifecycleOwner) { benchText.text = it }
        vm.errorMsg.observe(viewLifecycleOwner) { msg ->
            msg?.let { Toast.makeText(context, it, Toast.LENGTH_LONG).show() }
        }

        return frame
    }

    private fun buildControlsView(): View {
        val ctx = requireContext()
        val panel = LinearLayout(ctx).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(android.graphics.Color.argb(180, 0, 0, 0))
            setPadding(8, 4, 8, 8)
        }

        // Seek bar
        val seekBar = SeekBar(ctx).apply {
            max = 1000
            vm.position.observe(viewLifecycleOwner) { pos ->
                val dur = vm.duration.value ?: 0.0
                if (dur > 0) progress = (pos / dur * 1000).toInt()
            }
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(sb: SeekBar, p: Int, fromUser: Boolean) {
                    if (fromUser) vm.seek((vm.duration.value ?: 0.0) * p / 1000.0)
                }
                override fun onStartTrackingTouch(sb: SeekBar) {}
                override fun onStopTrackingTouch(sb: SeekBar) {}
            })
        }

        // Buttons row
        val btnRow = LinearLayout(ctx).apply { orientation = LinearLayout.HORIZONTAL }

        val btnOpen  = Button(ctx).apply {
            text = "Open"
            setOnClickListener { (activity as? MainActivity)?.openFilePicker() }
        }
        val btnPlay  = Button(ctx).apply {
            text = "Play"
            setOnClickListener {
                vm.togglePlayPause()
                vm.isPlaying.observe(viewLifecycleOwner) { playing ->
                    text = if (playing) "Pause" else "Play"
                }
            }
        }
        val btnStop  = Button(ctx).apply {
            text = "Stop"
            setOnClickListener { vm.stop() }
        }

        // Time label
        val timeLbl = TextView(ctx).apply {
            text = "0:00 / 0:00"
            setTextColor(android.graphics.Color.WHITE)
            textSize = 11f
            setPadding(12, 0, 0, 0)
        }
        vm.position.observe(viewLifecycleOwner) { pos ->
            val dur = vm.duration.value ?: 0.0
            timeLbl.text = "${formatTime(pos)} / ${formatTime(dur)}"
        }

        btnRow.addView(btnOpen)
        btnRow.addView(btnPlay)
        btnRow.addView(btnStop)
        btnRow.addView(timeLbl)

        panel.addView(seekBar)
        panel.addView(btnRow)
        return panel
    }

    fun openUri(uri: Uri) = vm.open(uri)

    private fun formatTime(seconds: Double): String {
        val s = seconds.toInt()
        val h = s / 3600; val m = (s % 3600) / 60; val sec = s % 60
        return if (h > 0) "%d:%02d:%02d".format(h, m, sec)
        else "%d:%02d".format(m, sec)
    }
}
