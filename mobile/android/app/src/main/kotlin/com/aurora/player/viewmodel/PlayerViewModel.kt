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

class PlayerViewModel(app: Application) : AndroidViewModel(app) {

    val player = NativePlayer()

    val isPlaying   = MutableLiveData(false)
    val position    = MutableLiveData(0.0)
    val duration    = MutableLiveData(0.0)
    val benchStats  = MutableLiveData("")
    val errorMsg    = MutableLiveData<String?>(null)
    val title       = MutableLiveData("Aurora Player")

    init {
        // Poll position & benchmark stats every 200ms
        viewModelScope.launch(Dispatchers.IO) {
            while (isActive) {
                position.postValue(player.position)
                duration.postValue(player.duration)
                benchStats.postValue(player.getBenchmarkStats())
                delay(200L)
            }
        }
    }

    fun open(uri: Uri) {
        viewModelScope.launch(Dispatchers.IO) {
            val path = uri.toString()
            if (player.open(path)) {
                player.play()
                isPlaying.postValue(true)
                title.postValue(uri.lastPathSegment ?: path)
            } else {
                errorMsg.postValue("Cannot open: $path")
            }
        }
    }

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
        isPlaying.value = false
        position.value  = 0.0
    }

    fun seek(seconds: Double) = player.seek(seconds)

    fun setVolume(percent: Int) = player.setVolume(percent)

    fun enableInterpolation(enabled: Boolean, targetFPS: Float = 60f) =
        player.setInterpolation(enabled, targetFPS)

    fun setUpscaler(model: String) = player.setUpscaler(model)

    fun loadSubtitle(path: String) = player.loadSubtitle(path)

    override fun onCleared() {
        player.stop()
        player.release()
        super.onCleared()
    }
}
