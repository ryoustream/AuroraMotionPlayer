package com.aurora.player.ui.subtitle

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.*
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.aurora.player.subtitle.SubtitleDownloader
import com.aurora.player.viewmodel.PlayerViewModel
import kotlinx.coroutines.launch

/**
 * Aurora Motion Player — SubtitleFragment (Session 7)
 *
 * Sheet-style fragment for:
 *  - Browsing and selecting embedded subtitle tracks
 *  - Searching + downloading from OpenSubtitles
 *  - Configuring: font size, bold, outline, color, vertical offset, delay
 *  - Loading external subtitle files via SAF
 */
class SubtitleFragment : Fragment() {

    private val vm: PlayerViewModel by activityViewModels()

    private lateinit var downloader     : SubtitleDownloader
    private lateinit var etSearch       : EditText
    private lateinit var btnSearch      : Button
    private lateinit var spinLang       : Spinner
    private lateinit var rvResults      : RecyclerView
    private lateinit var tvStatus       : TextView
    private lateinit var pbSearch       : ProgressBar
    private lateinit var btnLoadExternal: Button

    // Style controls
    private lateinit var sbFontSize     : SeekBar
    private lateinit var tvFontSizeLabel: TextView
    private lateinit var switchBold     : Switch
    private lateinit var switchOutline  : Switch
    private lateinit var sbDelay        : SeekBar
    private lateinit var tvDelayLabel   : TextView
    private lateinit var sbOffset       : SeekBar
    private lateinit var tvOffsetLabel  : TextView

    private var adapter: SubtitleResultAdapter? = null

    private val languages = arrayOf(
        "en" to "English", "id" to "Indonesian", "ja" to "Japanese",
        "zh" to "Chinese", "ko" to "Korean",     "es" to "Spanish",
        "fr" to "French",  "de" to "German",     "ar" to "Arabic",
        "pt" to "Portuguese"
    )

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?, savedInstanceState: Bundle?
    ): View {
        downloader = SubtitleDownloader(requireContext())

        val root = buildUI()
        setupStyleControls(root)
        return root
    }

    // ── UI builder ────────────────────────────────────────────────────────────

    private fun buildUI(): View {
        val ctx = requireContext()
        val scroll = ScrollView(ctx)
        val vl = LinearLayout(ctx).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(16, 16, 16, 16)
        }
        scroll.addView(vl)

        // ── Section: Search ──────────────────────────────────────────────────
        vl.addView(sectionHeader("Download Subtitles"))

        val langPairs = languages.map { it.second }.toTypedArray()
        spinLang = Spinner(ctx).also { sp ->
            sp.adapter = ArrayAdapter(ctx,
                android.R.layout.simple_spinner_dropdown_item, langPairs)
            vl.addView(sp)
        }

        val row = LinearLayout(ctx).apply { orientation = LinearLayout.HORIZONTAL }
        etSearch = EditText(ctx).apply {
            hint = "Movie / episode title"
            layoutParams = LinearLayout.LayoutParams(0,
                LinearLayout.LayoutParams.WRAP_CONTENT, 1f)
        }
        btnSearch = Button(ctx).apply { text = "Search" }
        row.addView(etSearch)
        row.addView(btnSearch)
        vl.addView(row)

        tvStatus  = TextView(ctx).apply { textSize = 11f }
        pbSearch  = ProgressBar(ctx).apply { visibility = View.GONE }
        vl.addView(tvStatus)
        vl.addView(pbSearch)

        rvResults = RecyclerView(ctx).apply {
            layoutManager = LinearLayoutManager(ctx)
            isNestedScrollingEnabled = false
        }
        adapter = SubtitleResultAdapter { entry -> downloadEntry(entry) }
        rvResults.adapter = adapter
        vl.addView(rvResults)

        // ── Section: External file ───────────────────────────────────────────
        vl.addView(sectionHeader("Load from Device"))
        btnLoadExternal = Button(ctx).apply {
            text = "Browse subtitle file…"
            setOnClickListener { pickExternalSubtitle() }
        }
        vl.addView(btnLoadExternal)

        // ── Section: Style ───────────────────────────────────────────────────
        vl.addView(sectionHeader("Style"))

        tvFontSizeLabel = TextView(ctx).apply { text = "Font size: 18sp" }
        sbFontSize      = SeekBar(ctx).apply { max = 48; progress = 18 }
        vl.addView(tvFontSizeLabel)
        vl.addView(sbFontSize)

        switchBold    = Switch(ctx).apply { text = "Bold" }
        switchOutline = Switch(ctx).apply { text = "Outline / shadow" }
        vl.addView(switchBold)
        vl.addView(switchOutline)

        tvDelayLabel  = TextView(ctx).apply { text = "Delay: 0 ms" }
        sbDelay       = SeekBar(ctx).apply { max = 1000; progress = 500 } // center = 0ms
        vl.addView(tvDelayLabel)
        vl.addView(sbDelay)

        tvOffsetLabel = TextView(ctx).apply { text = "Vertical offset: 0 %" }
        sbOffset      = SeekBar(ctx).apply { max = 100; progress = 50 } // center = bottom
        vl.addView(tvOffsetLabel)
        vl.addView(sbOffset)

        // Wire search
        btnSearch.setOnClickListener { doSearch() }

        return scroll
    }

    private fun sectionHeader(text: String) = TextView(requireContext()).apply {
        this.text  = text
        textSize   = 13f
        setTypeface(null, android.graphics.Typeface.BOLD)
        setPadding(0, 20, 0, 6)
    }

    // ── Style controls ────────────────────────────────────────────────────────

    private fun setupStyleControls(root: View) {
        sbFontSize.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(sb: SeekBar, p: Int, user: Boolean) {
                val size = p.coerceIn(8, 64)
                tvFontSizeLabel.text = "Font size: ${size}sp"
                vm.setSubtitleFontSize(size)
            }
            override fun onStartTrackingTouch(sb: SeekBar) {}
            override fun onStopTrackingTouch(sb: SeekBar)  {}
        })

        switchBold.setOnCheckedChangeListener    { _, c -> vm.setSubtitleBold(c) }
        switchOutline.setOnCheckedChangeListener { _, c -> vm.setSubtitleOutline(c) }

        sbDelay.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(sb: SeekBar, p: Int, user: Boolean) {
                val ms = (p - 500) * 2  // -1000ms .. +1000ms
                tvDelayLabel.text = "Delay: ${ms} ms"
                vm.setSubtitleDelay(ms)
            }
            override fun onStartTrackingTouch(sb: SeekBar) {}
            override fun onStopTrackingTouch(sb: SeekBar)  {}
        })

        sbOffset.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(sb: SeekBar, p: Int, user: Boolean) {
                val pct = p - 50   // -50% .. +50% from bottom
                tvOffsetLabel.text = "Vertical offset: ${pct} %"
                vm.setSubtitleOffset(pct)
            }
            override fun onStartTrackingTouch(sb: SeekBar) {}
            override fun onStopTrackingTouch(sb: SeekBar)  {}
        })
    }

    // ── Search & download ─────────────────────────────────────────────────────

    private fun doSearch() {
        val query = etSearch.text.toString().trim()
        if (query.isEmpty()) {
            tvStatus.text = "Enter a title to search."
            return
        }
        val langCode = languages.getOrNull(spinLang.selectedItemPosition)?.first ?: "en"

        pbSearch.visibility = View.VISIBLE
        tvStatus.text       = "Searching…"
        adapter?.submitList(emptyList())

        lifecycleScope.launch {
            when (val result = downloader.searchByTitle(query, language = langCode)) {
                is SubtitleDownloader.SearchResult.Success -> {
                    tvStatus.text = "${result.subtitles.size} result(s) found"
                    adapter?.submitList(result.subtitles)
                }
                is SubtitleDownloader.SearchResult.Error -> {
                    tvStatus.text = "Error: ${result.message}"
                }
                SubtitleDownloader.SearchResult.NoResults -> {
                    tvStatus.text = "No subtitles found for $query"
                }
            }
            pbSearch.visibility = View.GONE
        }
    }

    private fun downloadEntry(entry: SubtitleDownloader.SubtitleEntry) {
        tvStatus.text = "Downloading ${entry.filename}…"
        pbSearch.visibility = View.VISIBLE

        lifecycleScope.launch {
            when (val result = downloader.download(entry)) {
                is SubtitleDownloader.DownloadResult.Success -> {
                    tvStatus.text = "✓ Saved: ${result.file.name}"
                    vm.loadSubtitleFile(result.uri)
                }
                is SubtitleDownloader.DownloadResult.Error -> {
                    tvStatus.text = "Error: ${result.message}"
                }
            }
            pbSearch.visibility = View.GONE
        }
    }

    private fun pickExternalSubtitle() {
        // Delegate to MainActivity SAF picker
        (activity as? com.aurora.player.MainActivity)?.openSubtitlePicker()
    }
}

// ── Adapter ───────────────────────────────────────────────────────────────────

private class SubtitleResultAdapter(
    private val onDownload: (SubtitleDownloader.SubtitleEntry) -> Unit
) : RecyclerView.Adapter<SubtitleResultAdapter.VH>() {

    private val items = mutableListOf<SubtitleDownloader.SubtitleEntry>()

    fun submitList(list: List<SubtitleDownloader.SubtitleEntry>) {
        items.clear(); items.addAll(list); notifyDataSetChanged()
    }

    override fun onCreateViewHolder(parent: ViewGroup, vt: Int): VH {
        val row = LinearLayout(parent.context).apply {
            orientation = LinearLayout.HORIZONTAL
            setPadding(0, 8, 0, 8)
            layoutParams = ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
            )
        }
        val tv  = TextView(parent.context).apply {
            layoutParams = LinearLayout.LayoutParams(0,
                LinearLayout.LayoutParams.WRAP_CONTENT, 1f)
            textSize = 12f
        }
        val btn = Button(parent.context).apply { text = "↓" }
        row.addView(tv); row.addView(btn)
        return VH(row, tv, btn)
    }

    override fun onBindViewHolder(h: VH, pos: Int) {
        val e = items[pos]
        h.tvInfo.text = "[${e.language.uppercase()}] ${e.filename}\n" +
                        "⭐ ${String.format("%.1f", e.rating)}  " +
                        "⬇ ${e.downloadCount}  " +
                        (if (e.hearingImpaired) "👂 " else "") +
                        e.format.uppercase()
        h.btnDownload.setOnClickListener { onDownload(e) }
    }

    override fun getItemCount() = items.size

    class VH(root: View, val tvInfo: TextView, val btnDownload: Button) :
        RecyclerView.ViewHolder(root)
}
