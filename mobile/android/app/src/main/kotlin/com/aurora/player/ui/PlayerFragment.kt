package com.aurora.player.ui

import android.animation.Animator
import android.animation.AnimatorListenerAdapter
import android.app.PictureInPictureParams
import android.content.pm.ActivityInfo
import android.net.Uri
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Rational
import android.view.*
import android.widget.*
import androidx.core.view.isVisible
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import com.aurora.player.MainActivity
import com.aurora.player.R
import com.aurora.player.ui.pip.PiPManager
import com.aurora.player.util.UriUtils
import com.aurora.player.viewmodel.PlayerViewModel

/**
 * Aurora Motion Player — PlayerFragment (Session 7)
 *
 * Full-featured playback UI:
 *  - SurfaceView video output
 *  - Gesture controls  : double-tap seek, swipe volume/brightness, pinch zoom
 *  - Auto-hide controls after 3 s of inactivity
 *  - AB-repeat (long-press sets A / B)
 *  - Speed indicator (from ViewModel)
 *  - Subtitle overlay (styled text)
 *  - AI stats overlay  (interpolation FPS, GPU %)
 *  - Buffering spinner
 *  - PiP on home key via PiPManager
 *  - Fullscreen / orientation lock toggle
 *  - Chapter-aware seeking
 */
class PlayerFragment : Fragment() {

    private val vm: PlayerViewModel by activityViewModels()

    // Views
    private lateinit var root           : FrameLayout
    private lateinit var surfaceView    : SurfaceView
    private lateinit var controlsOverlay: View
    private lateinit var seekBar        : SeekBar
    private lateinit var tvPosition     : TextView
    private lateinit var tvDuration     : TextView
    private lateinit var tvTitle        : TextView
    private lateinit var btnPlayPause   : ImageButton
    private lateinit var btnSeekBack    : ImageButton
    private lateinit var btnSeekFwd     : ImageButton
    private lateinit var btnFullscreen  : ImageButton
    private lateinit var btnPlaylist    : ImageButton
    private lateinit var btnAiToggle    : ImageButton
    private lateinit var tvSubtitle     : TextView
    private lateinit var pbBuffering    : ProgressBar
    private lateinit var aiStatsOverlay : View
    private lateinit var tvInterpFps    : TextView
    private lateinit var tvGpuUsage     : TextView
    private lateinit var tvSpeedBadge   : TextView
    private lateinit var tvAbMarker     : TextView
    private lateinit var gestureDetector: GestureDetector
    private lateinit var scaleDetector  : ScaleGestureDetector

    private val pipManager by lazy { PiPManager(requireActivity()) }

    // Control auto-hide
    private val hideHandler = Handler(Looper.getMainLooper())
    private val hideRunnable = Runnable { hideControls() }
    private var controlsVisible = true

    // AB Repeat
    private var abA: Double? = null
    private var abB: Double? = null

    // Gesture state
    private var seekDragStartX   = 0f
    private var seekDragStartPos = 0.0
    private var isDraggingSeek   = false

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
    ): View {
        root = inflater.inflate(R.layout.fragment_player, container, false) as FrameLayout
        bindViews()
        setupSurface()
        setupControls()
        setupGestures()
        observeViewModel()
        scheduleHideControls()
        return root
    }

    // ── View binding ─────────────────────────────────────────────────────────

    private fun bindViews() {
        surfaceView     = root.findViewById(R.id.surface_view)
        controlsOverlay = root.findViewById(R.id.controls_overlay)
        seekBar         = root.findViewById(R.id.seek_bar)
        tvPosition      = root.findViewById(R.id.text_position)
        tvDuration      = root.findViewById(R.id.text_duration)
        tvTitle         = root.findViewById(R.id.text_title)
        btnPlayPause    = root.findViewById(R.id.btn_play_pause)
        btnSeekBack     = root.findViewById(R.id.btn_seek_back)
        btnSeekFwd      = root.findViewById(R.id.btn_seek_fwd)
        btnFullscreen   = root.findViewById(R.id.btn_fullscreen)
        btnPlaylist     = root.findViewById(R.id.btn_playlist)
        btnAiToggle     = root.findViewById(R.id.btn_ai_toggle)
        tvSubtitle      = root.findViewById(R.id.subtitle_text)
        pbBuffering     = root.findViewById(R.id.progress_buffering)
        aiStatsOverlay  = root.findViewById(R.id.ai_stats_overlay)
        tvInterpFps     = root.findViewById(R.id.text_interp_fps)
        tvGpuUsage      = root.findViewById(R.id.text_gpu_usage)
        tvSpeedBadge    = root.findViewById(R.id.text_speed_badge)
        tvAbMarker      = root.findViewById(R.id.text_ab_marker)
    }

    // ── Surface ───────────────────────────────────────────────────────────────

    private fun setupSurface() {
        surfaceView.holder.addCallback(object : SurfaceHolder.Callback {
            override fun surfaceCreated(h: SurfaceHolder) {
                // Wire ExoPlayer (primary engine) to the SurfaceView
                vm.exoPlayer.setVideoSurfaceView(surfaceView)
                // Also wire NativePlayer for AI overlay
                vm.player.surfaceCreated(h.surface)
            }
            override fun surfaceChanged(h: SurfaceHolder, f: Int, w: Int, ht: Int) {
                vm.player.surfaceChanged(w, ht)
            }
            override fun surfaceDestroyed(h: SurfaceHolder) {
                vm.exoPlayer.clearVideoSurfaceView(surfaceView)
                vm.player.surfaceDestroyed()
            }
        })
    }

    // ── Controls ──────────────────────────────────────────────────────────────

    private fun setupControls() {
        btnPlayPause.setOnClickListener {
            vm.togglePlayPause()
            resetHideTimer()
        }

        btnSeekBack.setOnClickListener {
            vm.seekRelative(-10.0)
            resetHideTimer()
        }
        btnSeekFwd.setOnClickListener {
            vm.seekRelative(10.0)
            resetHideTimer()
        }

        // Long-press seek buttons for AB repeat
        btnSeekBack.setOnLongClickListener {
            abA = vm.position.value ?: 0.0
            updateAbMarker()
            true
        }
        btnSeekFwd.setOnLongClickListener {
            abB = vm.position.value ?: 0.0
            updateAbMarker()
            true
        }

        btnFullscreen.setOnClickListener { toggleFullscreen() }

        btnPlaylist.setOnClickListener {
            (activity as? MainActivity)?.openPlaylist()
        }

        btnAiToggle.setOnClickListener {
            vm.toggleAI()
            resetHideTimer()
        }

        // SeekBar
        seekBar.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onStartTrackingTouch(sb: SeekBar) { isDraggingSeek = true }
            override fun onStopTrackingTouch(sb: SeekBar) {
                val dur = vm.duration.value ?: 0.0
                vm.seek(dur * sb.progress / 1000.0)
                isDraggingSeek = false
            }
            override fun onProgressChanged(sb: SeekBar, p: Int, fromUser: Boolean) {
                if (fromUser) {
                    val dur = vm.duration.value ?: 0.0
                    tvPosition.text = formatTime(dur * p / 1000.0)
                }
            }
        })
    }

    // ── Gesture handling ──────────────────────────────────────────────────────

    private fun setupGestures() {
        gestureDetector = GestureDetector(requireContext(),
            object : GestureDetector.SimpleOnGestureListener() {
                override fun onSingleTapConfirmed(e: MotionEvent): Boolean {
                    toggleControlsVisibility()
                    return true
                }
                override fun onDoubleTap(e: MotionEvent): Boolean {
                    val width = surfaceView.width.toFloat()
                    if (e.x < width / 3f)       vm.seekRelative(-10.0)
                    else if (e.x > width * 2 / 3f) vm.seekRelative(10.0)
                    else vm.togglePlayPause()
                    return true
                }
                override fun onLongPress(e: MotionEvent) {
                    vm.setSpeed(if (vm.speed.value == 1.0) 2.0 else 1.0)
                }
            })

        scaleDetector = ScaleGestureDetector(requireContext(),
            object : ScaleGestureDetector.SimpleOnScaleGestureListener() {
                override fun onScale(d: ScaleGestureDetector): Boolean {
                    // Pinch-to-zoom: adjust surface scale factor
                    val factor = d.scaleFactor
                    surfaceView.scaleX = (surfaceView.scaleX * factor).coerceIn(0.5f, 2.5f)
                    surfaceView.scaleY = surfaceView.scaleX
                    return true
                }
            })

        root.setOnTouchListener { _, e ->
            scaleDetector.onTouchEvent(e)
            gestureDetector.onTouchEvent(e)
            true
        }
    }

    // ── ViewModel observers ───────────────────────────────────────────────────

    private fun observeViewModel() {
        vm.isPlaying.observe(viewLifecycleOwner) { playing ->
            btnPlayPause.setImageResource(
                if (playing) android.R.drawable.ic_media_pause
                else         android.R.drawable.ic_media_play
            )
        }

        vm.position.observe(viewLifecycleOwner) { pos ->
            val dur = vm.duration.value ?: 0.0
            if (!isDraggingSeek) {
                tvPosition.text = formatTime(pos)
                if (dur > 0) seekBar.progress = (pos / dur * 1000).toInt()
            }
            // AB repeat check
            abB?.let { b -> abA?.let { a ->
                if (pos >= b) vm.seek(a)
            }}
        }

        vm.duration.observe(viewLifecycleOwner) { dur ->
            tvDuration.text = formatTime(dur)
            seekBar.max = 1000
        }

        vm.title.observe(viewLifecycleOwner) { title ->
            tvTitle.text = title ?: "Aurora Motion Player"
        }

        vm.isBuffering.observe(viewLifecycleOwner) { buffering ->
            pbBuffering.isVisible = buffering
        }

        vm.subtitleText.observe(viewLifecycleOwner) { text ->
            tvSubtitle.isVisible = !text.isNullOrEmpty()
            tvSubtitle.text = text
        }

        vm.benchStats.observe(viewLifecycleOwner) { stats ->
            if (stats != null) {
                aiStatsOverlay.isVisible = true
                val lines = stats.lines()
                tvInterpFps.text = lines.getOrNull(0) ?: stats
                tvGpuUsage.text  = lines.getOrNull(1) ?: ""
            } else {
                aiStatsOverlay.isVisible = false
            }
        }

        vm.speed.observe(viewLifecycleOwner) { speed ->
            val show = speed != null && speed != 1.0
            tvSpeedBadge.isVisible = show
            if (show) tvSpeedBadge.text = "×${String.format("%.2f", speed)}"
        }

        vm.errorMsg.observe(viewLifecycleOwner) { msg ->
            msg?.let { Toast.makeText(requireContext(), it, Toast.LENGTH_LONG).show() }
        }
    }

    // ── Control visibility ────────────────────────────────────────────────────

    private fun toggleControlsVisibility() {
        if (controlsVisible) hideControls() else showControls()
    }

    private fun showControls() {
        controlsVisible = true
        controlsOverlay.animate()
            .alpha(1f)
            .setDuration(200)
            .setListener(object : AnimatorListenerAdapter() {
                override fun onAnimationStart(a: Animator) {
                    controlsOverlay.isVisible = true
                }
            }).start()
        scheduleHideControls()
    }

    private fun hideControls() {
        controlsVisible = false
        controlsOverlay.animate()
            .alpha(0f)
            .setDuration(300)
            .setListener(object : AnimatorListenerAdapter() {
                override fun onAnimationEnd(a: Animator) {
                    controlsOverlay.isVisible = false
                }
            }).start()
    }

    private fun scheduleHideControls() {
        hideHandler.removeCallbacks(hideRunnable)
        hideHandler.postDelayed(hideRunnable, 3000L)
    }

    private fun resetHideTimer() {
        if (!controlsVisible) showControls() else scheduleHideControls()
    }

    // ── AB repeat ─────────────────────────────────────────────────────────────

    private fun updateAbMarker() {
        val a = abA; val b = abB
        tvAbMarker.isVisible = a != null
        tvAbMarker.text = when {
            a != null && b != null -> "AB: ${formatTime(a)} → ${formatTime(b)}"
            a != null              -> "A: ${formatTime(a)}  [set B with long-press ▶▶]"
            else                   -> ""
        }
    }

    // ── Fullscreen ────────────────────────────────────────────────────────────

    private fun toggleFullscreen() {
        val act = requireActivity()
        val current = act.requestedOrientation
        if (current == ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE ||
            current == ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE) {
            act.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR
            btnFullscreen.setImageResource(android.R.drawable.ic_menu_zoom)
        } else {
            act.requestedOrientation = ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
            btnFullscreen.setImageResource(android.R.drawable.ic_menu_revert)
        }
    }

    // ── PiP ───────────────────────────────────────────────────────────────────

    fun enterPiP() {
        val dur = vm.duration.value ?: 0.0
        pipManager.enterPiP(
            isPlaying = vm.isPlaying.value == true,
            position  = vm.position.value ?: 0.0,
            duration  = dur
        )
    }

    fun onPiPModeChanged(isInPiP: Boolean) {
        controlsOverlay.isVisible = !isInPiP
        tvSubtitle.isVisible      = !isInPiP && !(vm.subtitleText.value.isNullOrEmpty())
    }

    // ── Public API ────────────────────────────────────────────────────────────

    fun openUri(uri: Uri) {
        // Pass URI directly — ExoPlayer handles content://, file://, http:// natively
        vm.open(uri)
    }

    // ── Helpers ───────────────────────────────────────────────────────────────

    private fun formatTime(seconds: Double): String {
        val s = seconds.toLong()
        val h = s / 3600; val m = (s % 3600) / 60; val sec = s % 60
        return if (h > 0) "%d:%02d:%02d".format(h, m, sec)
        else              "%d:%02d".format(m, sec)
    }

    override fun onDestroyView() {
        hideHandler.removeCallbacks(hideRunnable)
        super.onDestroyView()
    }
}
