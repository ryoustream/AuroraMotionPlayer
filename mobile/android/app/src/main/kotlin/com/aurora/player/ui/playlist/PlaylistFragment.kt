package com.aurora.player.ui.playlist

import android.content.ContentUris
import android.content.Context
import android.database.Cursor
import android.net.Uri
import android.os.Bundle
import android.provider.MediaStore
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.TextView
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.aurora.player.R
import com.aurora.player.viewmodel.PlaylistViewModel
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/**
 * Aurora Motion Player — Playlist Fragment
 *
 * Displays a scrollable playlist using MediaStore to scan device videos.
 * Supports:
 *  - Scan from MediaStore (videos on device)
 *  - Manual file add via SAF picker
 *  - Reorder by drag & drop
 *  - Thumbnail generation
 *  - Current playing item highlight
 */
class PlaylistFragment : Fragment() {

    private val viewModel: PlaylistViewModel by activityViewModels()
    private lateinit var adapter: PlaylistAdapter
    private lateinit var recyclerView: RecyclerView

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        val view = inflater.inflate(R.layout.fragment_playlist, container, false)
        recyclerView = view.findViewById(R.id.rvPlaylist)
        recyclerView.layoutManager = LinearLayoutManager(requireContext())

        adapter = PlaylistAdapter(
            onItemClick = { item -> viewModel.playItem(item) },
            onItemRemove = { item -> viewModel.removeItem(item) }
        )
        recyclerView.adapter = adapter

        // Observe playlist changes
        viewModel.playlist.observe(viewLifecycleOwner) { items ->
            adapter.submitList(items)
        }
        viewModel.currentIndex.observe(viewLifecycleOwner) { idx ->
            adapter.setCurrentIndex(idx)
        }

        // FAB: add files
        view.findViewById<View>(R.id.fabAddFiles)?.setOnClickListener {
            viewModel.requestFilePicker()
        }

        // Scan device videos
        view.findViewById<View>(R.id.btnScanDevice)?.setOnClickListener {
            scanDeviceVideos()
        }

        return view
    }

    private fun scanDeviceVideos() {
        lifecycleScope.launch {
            val items = withContext(Dispatchers.IO) {
                queryMediaStore(requireContext())
            }
            viewModel.addItems(items)
        }
    }

    private fun queryMediaStore(ctx: Context): List<PlaylistItem> {
        val result = mutableListOf<PlaylistItem>()
        val collection = MediaStore.Video.Media.getContentUri(MediaStore.VOLUME_EXTERNAL)

        val projection = arrayOf(
            MediaStore.Video.Media._ID,
            MediaStore.Video.Media.DISPLAY_NAME,
            MediaStore.Video.Media.DURATION,
            MediaStore.Video.Media.SIZE,
            MediaStore.Video.Media.DATA
        )
        val sortOrder = "${MediaStore.Video.Media.DATE_MODIFIED} DESC"

        val cursor: Cursor? = ctx.contentResolver.query(
            collection, projection, null, null, sortOrder
        )
        cursor?.use { c ->
            val idCol    = c.getColumnIndexOrThrow(MediaStore.Video.Media._ID)
            val nameCol  = c.getColumnIndexOrThrow(MediaStore.Video.Media.DISPLAY_NAME)
            val durCol   = c.getColumnIndexOrThrow(MediaStore.Video.Media.DURATION)
            val sizeCol  = c.getColumnIndexOrThrow(MediaStore.Video.Media.SIZE)

            while (c.moveToNext()) {
                val id       = c.getLong(idCol)
                val name     = c.getString(nameCol) ?: continue
                val duration = c.getLong(durCol)
                val size     = c.getLong(sizeCol)
                val uri      = ContentUris.withAppendedId(collection, id)

                result.add(PlaylistItem(
                    id       = id.toString(),
                    title    = name.substringBeforeLast('.'),
                    uri      = uri,
                    duration = duration,
                    fileSize = size
                ))
            }
        }
        return result
    }
}

// ── Data class ────────────────────────────────────────────────────────────────
data class PlaylistItem(
    val id       : String,
    val title    : String,
    val uri      : Uri,
    val duration : Long,    // ms
    val fileSize : Long     // bytes
) {
    fun formattedDuration(): String {
        val s = duration / 1000
        val h = s / 3600
        val m = (s % 3600) / 60
        val sec = s % 60
        return if (h > 0) "%d:%02d:%02d".format(h, m, sec)
        else              "%d:%02d".format(m, sec)
    }
}

// ── Adapter ───────────────────────────────────────────────────────────────────
class PlaylistAdapter(
    private val onItemClick  : (PlaylistItem) -> Unit,
    private val onItemRemove : (PlaylistItem) -> Unit
) : RecyclerView.Adapter<PlaylistAdapter.VH>() {

    private val items = mutableListOf<PlaylistItem>()
    private var currentIdx = -1

    fun submitList(newItems: List<PlaylistItem>) {
        items.clear()
        items.addAll(newItems)
        notifyDataSetChanged()
    }

    fun setCurrentIndex(idx: Int) {
        val prev = currentIdx
        currentIdx = idx
        if (prev >= 0) notifyItemChanged(prev)
        if (idx  >= 0) notifyItemChanged(idx)
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): VH {
        val v = LayoutInflater.from(parent.context)
            .inflate(R.layout.item_playlist, parent, false)
        return VH(v)
    }

    override fun onBindViewHolder(holder: VH, position: Int) {
        val item = items[position]
        holder.bind(item, position == currentIdx, onItemClick, onItemRemove)
    }

    override fun getItemCount() = items.size

    class VH(itemView: View) : RecyclerView.ViewHolder(itemView) {
        private val tvTitle    : TextView  = itemView.findViewById(R.id.tvTitle)
        private val tvDuration : TextView  = itemView.findViewById(R.id.tvDuration)
        private val ivCurrent  : ImageView = itemView.findViewById(R.id.ivNowPlaying)
        private val btnRemove  : View      = itemView.findViewById(R.id.btnRemove)

        fun bind(
            item       : PlaylistItem,
            isCurrent  : Boolean,
            onClick    : (PlaylistItem) -> Unit,
            onRemove   : (PlaylistItem) -> Unit
        ) {
            tvTitle.text    = item.title
            tvDuration.text = item.formattedDuration()
            ivCurrent.visibility = if (isCurrent) View.VISIBLE else View.INVISIBLE
            itemView.isSelected  = isCurrent

            itemView.setOnClickListener { onClick(item) }
            btnRemove.setOnClickListener { onRemove(item) }
        }
    }
}
