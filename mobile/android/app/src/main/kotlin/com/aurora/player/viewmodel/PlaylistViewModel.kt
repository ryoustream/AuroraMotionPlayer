package com.aurora.player.viewmodel

import android.net.Uri
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import com.aurora.player.ui.playlist.PlaylistItem

/**
 * Aurora Motion Player — Playlist ViewModel
 *
 * Shared between PlaylistFragment, PlayerFragment, and MainActivity.
 * Manages the ordered list of video items and the current playback index.
 */
class PlaylistViewModel : ViewModel() {

    // ── Playlist state ────────────────────────────────────────────────────────
    private val _playlist     = MutableLiveData<List<PlaylistItem>>(emptyList())
    val playlist: LiveData<List<PlaylistItem>> = _playlist

    private val _currentIndex = MutableLiveData(-1)
    val currentIndex: LiveData<Int> = _currentIndex

    private val _currentItem  = MutableLiveData<PlaylistItem?>()
    val currentItem: LiveData<PlaylistItem?> = _currentItem

    // ── File picker event (one-shot) ──────────────────────────────────────────
    private val _filePickerRequest = MutableLiveData<Unit>()
    val filePickerRequest: LiveData<Unit> = _filePickerRequest

    // ── Playback control ──────────────────────────────────────────────────────

    fun playItem(item: PlaylistItem) {
        val idx = _playlist.value?.indexOf(item) ?: return
        _currentIndex.value = idx
        _currentItem.value  = item
    }

    fun playIndex(idx: Int) {
        val list = _playlist.value ?: return
        if (idx < 0 || idx >= list.size) return
        _currentIndex.value = idx
        _currentItem.value  = list[idx]
    }

    fun playNext() {
        val idx  = _currentIndex.value ?: -1
        val size = _playlist.value?.size ?: 0
        if (size > 0) playIndex((idx + 1) % size)
    }

    fun playPrevious() {
        val idx  = _currentIndex.value ?: 0
        val size = _playlist.value?.size ?: 0
        if (size > 0) playIndex(if (idx > 0) idx - 1 else size - 1)
    }

    // ── List manipulation ─────────────────────────────────────────────────────

    fun addItem(item: PlaylistItem) {
        val current = _playlist.value.orEmpty().toMutableList()
        if (current.none { it.id == item.id }) {
            current.add(item)
            _playlist.value = current
        }
    }

    fun addItems(items: List<PlaylistItem>) {
        val current = _playlist.value.orEmpty().toMutableList()
        val newIds  = current.map { it.id }.toSet()
        items.filterNot { it.id in newIds }.forEach { current.add(it) }
        _playlist.value = current
    }

    fun addUri(uri: Uri, title: String? = null) {
        val item = PlaylistItem(
            id       = uri.toString(),
            title    = title ?: uri.lastPathSegment ?: "Unknown",
            uri      = uri,
            duration = 0L,
            fileSize = 0L
        )
        addItem(item)
    }

    fun removeItem(item: PlaylistItem) {
        val current = _playlist.value.orEmpty().toMutableList()
        val idx     = current.indexOf(item)
        if (idx < 0) return
        current.removeAt(idx)
        _playlist.value = current

        // Adjust current index
        val ci = _currentIndex.value ?: -1
        when {
            idx < ci            -> _currentIndex.value = ci - 1
            idx == ci && current.isNotEmpty() ->
                playIndex(minOf(ci, current.size - 1))
            else -> _currentIndex.value = -1
        }
    }

    fun moveItem(fromIdx: Int, toIdx: Int) {
        val current = _playlist.value.orEmpty().toMutableList()
        if (fromIdx < 0 || fromIdx >= current.size) return
        if (toIdx   < 0 || toIdx   >= current.size) return
        val item = current.removeAt(fromIdx)
        current.add(toIdx, item)
        _playlist.value = current
    }

    fun clearPlaylist() {
        _playlist.value  = emptyList()
        _currentIndex.value = -1
        _currentItem.value  = null
    }

    // ── SAF file picker ───────────────────────────────────────────────────────
    fun requestFilePicker() {
        _filePickerRequest.value = Unit
    }
}
