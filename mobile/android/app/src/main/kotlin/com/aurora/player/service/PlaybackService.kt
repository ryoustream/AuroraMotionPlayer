package com.aurora.player.service

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.os.Binder
import android.os.IBinder
import android.support.v4.media.session.MediaSessionCompat
import androidx.core.app.NotificationCompat
import com.aurora.player.MainActivity
import com.aurora.player.player.NativePlayer

/**
 * Foreground service for background/notification-controlled playback.
 * Keeps the player alive when the app is in background (e.g. PiP, screen off).
 */
class PlaybackService : Service() {

    private val binder = LocalBinder()
    private lateinit var player: NativePlayer
    private lateinit var mediaSession: MediaSessionCompat
    private lateinit var notificationManager: NotificationManager

    companion object {
        const val CHANNEL_ID    = "aurora_playback"
        const val NOTIFICATION_ID = 1001
        const val ACTION_PLAY   = "com.aurora.player.PLAY"
        const val ACTION_PAUSE  = "com.aurora.player.PAUSE"
        const val ACTION_STOP   = "com.aurora.player.STOP"
    }

    inner class LocalBinder : Binder() {
        fun getService(): PlaybackService = this@PlaybackService
    }

    override fun onCreate() {
        super.onCreate()
        player = NativePlayer()
        createNotificationChannel()

        mediaSession = MediaSessionCompat(this, "AuroraMediaSession")
        mediaSession.isActive = true
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_PLAY  -> player.play()
            ACTION_PAUSE -> player.pause()
            ACTION_STOP  -> {
                player.stop()
                stopForeground(STOP_FOREGROUND_REMOVE)
                stopSelf()
                return START_NOT_STICKY
            }
        }
        startForeground(NOTIFICATION_ID, buildNotification())
        return START_STICKY
    }

    override fun onBind(intent: Intent?): IBinder = binder

    override fun onDestroy() {
        player.stop()
        player.release()
        mediaSession.release()
        super.onDestroy()
    }

    fun getPlayer(): NativePlayer = player

    private fun createNotificationChannel() {
        notificationManager = getSystemService(NotificationManager::class.java)
        val channel = NotificationChannel(
            CHANNEL_ID,
            "Aurora Playback",
            NotificationManager.IMPORTANCE_LOW
        ).apply {
            description = "Aurora Motion Player background playback"
            setSound(null, null)
            setShowBadge(false)
        }
        notificationManager.createNotificationChannel(channel)
    }

    private fun buildNotification(): Notification {
        val pendingIntent = PendingIntent.getActivity(
            this, 0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )

        val pauseAction = buildAction(ACTION_PAUSE, "Pause", android.R.drawable.ic_media_pause)
        val stopAction  = buildAction(ACTION_STOP,  "Stop",  android.R.drawable.ic_media_next)

        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("Aurora Motion Player")
            .setContentText("Playing...")
            .setSmallIcon(android.R.drawable.ic_media_play)
            .setContentIntent(pendingIntent)
            .addAction(pauseAction)
            .addAction(stopAction)
            .setStyle(androidx.media.app.NotificationCompat.MediaStyle()
                .setMediaSession(mediaSession.sessionToken)
                .setShowActionsInCompactView(0, 1))
            .setOngoing(true)
            .setSilent(true)
            .build()
    }

    private fun buildAction(action: String, title: String, icon: Int):
            NotificationCompat.Action {
        val intent = PendingIntent.getService(
            this, 0,
            Intent(this, PlaybackService::class.java).setAction(action),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        return NotificationCompat.Action(icon, title, intent)
    }
}
