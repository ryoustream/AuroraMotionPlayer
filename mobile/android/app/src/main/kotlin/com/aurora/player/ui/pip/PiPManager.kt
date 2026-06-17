package com.aurora.player.ui.pip

import android.app.Activity
import android.app.PendingIntent
import android.app.PictureInPictureParams
import android.app.RemoteAction
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.graphics.drawable.Icon
import android.os.Build
import android.util.Rational
import androidx.annotation.RequiresApi
import com.aurora.player.service.PlaybackService

/**
 * Aurora Motion Player — PiP Manager (Session 7)
 *
 * Manages Picture-in-Picture lifecycle for the player activity.
 *
 * Features:
 *  - Enters PiP with correct aspect ratio derived from video surface dimensions
 *  - MediaSession-based remote actions (Play/Pause, Stop, Seek ±10s)
 *  - Auto-enters PiP on home key press (Activity.onUserLeaveHint)
 *  - Restores fullscreen when user taps the PiP window
 *  - Android 12+ seamless transition support
 */
class PiPManager(private val activity: Activity) {

    companion object {
        private const val ACTION_MEDIA_CONTROL = "com.aurora.player.pip.MEDIA_CONTROL"
        private const val EXTRA_CONTROL_TYPE   = "control_type"

        const val CONTROL_PLAY_PAUSE = 1
        const val CONTROL_STOP       = 2
        const val CONTROL_SEEK_BACK  = 3
        const val CONTROL_SEEK_FWD   = 4

        private const val REQUEST_PLAY_PAUSE = 101
        private const val REQUEST_STOP       = 102
        private const val REQUEST_SEEK_BACK  = 103
        private const val REQUEST_SEEK_FWD   = 104
    }

    // Aspect ratio of the video being played (updated by the player)
    private var videoWidth  = 16
    private var videoHeight = 9

    // Receiver for PiP media control actions
    private val controlReceiver = object : BroadcastReceiver() {
        override fun onReceive(ctx: Context, intent: Intent) {
            when (intent.getIntExtra(EXTRA_CONTROL_TYPE, -1)) {
                CONTROL_PLAY_PAUSE -> onPlayPauseCallback?.invoke()
                CONTROL_STOP       -> onStopCallback?.invoke()
                CONTROL_SEEK_BACK  -> onSeekCallback?.invoke(-10.0)
                CONTROL_SEEK_FWD   -> onSeekCallback?.invoke(+10.0)
            }
        }
    }

    var onPlayPauseCallback: (() -> Unit)?      = null
    var onStopCallback:      (() -> Unit)?      = null
    var onSeekCallback:      ((Double) -> Unit)? = null

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    fun register() {
        val filter = IntentFilter(ACTION_MEDIA_CONTROL)
        activity.registerReceiver(controlReceiver, filter,
            Context.RECEIVER_NOT_EXPORTED)
    }

    fun unregister() {
        runCatching { activity.unregisterReceiver(controlReceiver) }
    }

    // ── Enter PiP ─────────────────────────────────────────────────────────────

    fun enterPiP(
        isPlaying : Boolean,
        position  : Double,
        duration  : Double
    ) {
        if (!activity.packageManager.hasSystemFeature(
                android.content.pm.PackageManager.FEATURE_PICTURE_IN_PICTURE)) return

        val params = buildPiPParams(isPlaying, position, duration)
        activity.enterPictureInPictureMode(params)
    }

    /** Call from Activity.onPictureInPictureModeChanged to keep params fresh. */
    fun onPiPModeChanged(isInPiP: Boolean, isPlaying: Boolean) {
        if (isInPiP) return
        // Restore fullscreen — nothing special needed (Activity handles UI)
    }

    /** Call from Activity.onUserLeaveHint for auto-PiP on home press. */
    fun onUserLeaveHint(isPlaying: Boolean, position: Double, duration: Double) {
        if (isPlaying) enterPiP(isPlaying, position, duration)
    }

    /** Call when video dimensions are known. */
    fun setVideoSize(width: Int, height: Int) {
        if (width > 0 && height > 0) {
            videoWidth  = width
            videoHeight = height
        }
    }

    // ── Params builder ────────────────────────────────────────────────────────

    private fun buildPiPParams(
        isPlaying: Boolean,
        position : Double,
        duration : Double
    ): PictureInPictureParams {
        val gcd  = gcd(videoWidth, videoHeight)
        val ratio = Rational(
            (videoWidth  / gcd).coerceIn(1, 239),
            (videoHeight / gcd).coerceIn(1, 239)
        )

        val actions = buildRemoteActions(isPlaying)

        return PictureInPictureParams.Builder()
            .setAspectRatio(ratio)
            .setActions(actions)
            .apply {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                    setAutoEnterEnabled(isPlaying)
                    setSeamlessResizeEnabled(true)
                }
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU && duration > 0) {
                    setTitle("Aurora Motion Player")
                    setSubtitle(formatTime(position) + " / " + formatTime(duration))
                }
            }
            .build()
    }

    private fun buildRemoteActions(isPlaying: Boolean): List<RemoteAction> {
        val actions = mutableListOf<RemoteAction>()

        // Seek -10s
        actions.add(RemoteAction(
            Icon.createWithResource(activity, android.R.drawable.ic_media_rew),
            "−10s", "Seek back 10 seconds",
            makePendingIntent(CONTROL_SEEK_BACK, REQUEST_SEEK_BACK)
        ))

        // Play/Pause
        val playPauseIcon = if (isPlaying) android.R.drawable.ic_media_pause
                            else           android.R.drawable.ic_media_play
        val playPauseLabel = if (isPlaying) "Pause" else "Play"
        actions.add(RemoteAction(
            Icon.createWithResource(activity, playPauseIcon),
            playPauseLabel, playPauseLabel,
            makePendingIntent(CONTROL_PLAY_PAUSE, REQUEST_PLAY_PAUSE)
        ).also { if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) it.isEnabled = true })

        // Seek +10s
        actions.add(RemoteAction(
            Icon.createWithResource(activity, android.R.drawable.ic_media_ff),
            "+10s", "Seek forward 10 seconds",
            makePendingIntent(CONTROL_SEEK_FWD, REQUEST_SEEK_FWD)
        ))

        // Stop
        actions.add(RemoteAction(
            Icon.createWithResource(activity, android.R.drawable.ic_menu_close_clear_cancel),
            "Stop", "Stop playback",
            makePendingIntent(CONTROL_STOP, REQUEST_STOP)
        ))

        return actions
    }

    private fun makePendingIntent(controlType: Int, requestCode: Int): PendingIntent {
        val intent = Intent(ACTION_MEDIA_CONTROL).putExtra(EXTRA_CONTROL_TYPE, controlType)
        return PendingIntent.getBroadcast(
            activity, requestCode, intent,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
    }

    // ── Utils ─────────────────────────────────────────────────────────────────

    private tailrec fun gcd(a: Int, b: Int): Int = if (b == 0) a else gcd(b, a % b)

    private fun formatTime(sec: Double): String {
        val s = sec.toLong()
        val h = s / 3600; val m = (s % 3600) / 60; val ss = s % 60
        return if (h > 0) "%d:%02d:%02d".format(h, m, ss)
        else              "%d:%02d".format(m, ss)
    }
}
