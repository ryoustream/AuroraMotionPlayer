package com.aurora.player.ui.settings

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.*
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import com.aurora.player.R
import com.aurora.player.viewmodel.SettingsViewModel

/**
 * Aurora Motion Player — Settings Fragment
 *
 * Tabs:
 *  1. Video    — decoder, hardware accel, renderer backend
 *  2. AI       — interpolation model, upscaler, FPS target
 *  3. Audio    — passthrough, equalizer, sync offset
 *  4. Subtitle — font, size, style, download service
 *  5. Advanced — buffer size, thread count, debug overlay
 */
class SettingsFragment : Fragment() {

    private val viewModel: SettingsViewModel by activityViewModels()

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        val view = inflater.inflate(R.layout.fragment_settings, container, false)
        setupVideoTab(view)
        setupAITab(view)
        setupAudioTab(view)
        setupSubtitleTab(view)
        setupAdvancedTab(view)
        return view
    }

    // ── Video tab ─────────────────────────────────────────────────────────────
    private fun setupVideoTab(view: View) {
        val spinDecoder    = view.findViewById<Spinner>(R.id.spinDecoder)
        val switchHWAccel  = view.findViewById<Switch>(R.id.switchHWAccel)
        val spinRenderer   = view.findViewById<Spinner>(R.id.spinRenderer)
        val switchHDR      = view.findViewById<Switch>(R.id.switchHDR)
        val spinToneMap    = view.findViewById<Spinner>(R.id.spinToneMap)

        spinDecoder?.let { spinner ->
            val decoders = arrayOf("Auto", "FFmpeg Software", "MediaCodec", "MediaCodec + FFmpeg")
            spinner.adapter = ArrayAdapter(requireContext(),
                android.R.layout.simple_spinner_dropdown_item, decoders)
            spinner.setSelection(viewModel.decoderIndex.value ?: 0)
            spinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
                override fun onItemSelected(p: AdapterView<*>?, v: View?, pos: Int, id: Long) {
                    viewModel.setDecoder(pos)
                }
                override fun onNothingSelected(p: AdapterView<*>?) {}
            }
        }

        switchHWAccel?.apply {
            isChecked = viewModel.hwAccelEnabled.value == true
            setOnCheckedChangeListener { _, checked -> viewModel.setHWAccel(checked) }
        }

        spinRenderer?.let { spinner ->
            val renderers = arrayOf("Auto", "Vulkan", "OpenGL ES", "Software")
            spinner.adapter = ArrayAdapter(requireContext(),
                android.R.layout.simple_spinner_dropdown_item, renderers)
            spinner.setSelection(viewModel.rendererIndex.value ?: 0)
            spinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
                override fun onItemSelected(p: AdapterView<*>?, v: View?, pos: Int, id: Long) {
                    viewModel.setRenderer(pos)
                }
                override fun onNothingSelected(p: AdapterView<*>?) {}
            }
        }

        switchHDR?.apply {
            isChecked = viewModel.hdrEnabled.value == true
            setOnCheckedChangeListener { _, checked ->
                viewModel.setHDR(checked)
                spinToneMap?.isEnabled = checked
            }
        }

        spinToneMap?.let { spinner ->
            val modes = arrayOf("BT.2390", "Mobius", "ACES", "Reinhard")
            spinner.adapter = ArrayAdapter(requireContext(),
                android.R.layout.simple_spinner_dropdown_item, modes)
            spinner.setSelection(viewModel.toneMappingIndex.value ?: 0)
            spinner.isEnabled = viewModel.hdrEnabled.value == true
            spinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
                override fun onItemSelected(p: AdapterView<*>?, v: View?, pos: Int, id: Long) {
                    viewModel.setToneMapping(pos)
                }
                override fun onNothingSelected(p: AdapterView<*>?) {}
            }
        }
    }

    // ── AI tab ────────────────────────────────────────────────────────────────
    private fun setupAITab(view: View) {
        val switchInterp     = view.findViewById<Switch>(R.id.switchInterpolation)
        val spinInterpModel  = view.findViewById<Spinner>(R.id.spinInterpModel)
        val spinTargetFPS    = view.findViewById<Spinner>(R.id.spinTargetFPS)
        val switchUpscale    = view.findViewById<Switch>(R.id.switchUpscale)
        val spinUpscaler     = view.findViewById<Spinner>(R.id.spinUpscaler)
        val spinUpscaleMode  = view.findViewById<Spinner>(R.id.spinUpscaleMode)
        val switchAutoSelect = view.findViewById<Switch>(R.id.switchAutoSelect)
        val sbQuality        = view.findViewById<SeekBar>(R.id.sbQuality)
        val tvQualityLabel   = view.findViewById<TextView>(R.id.tvQualityLabel)

        switchInterp?.apply {
            isChecked = viewModel.interpEnabled.value == true
            setOnCheckedChangeListener { _, checked ->
                viewModel.setInterp(checked)
                spinInterpModel?.isEnabled = checked
                spinTargetFPS?.isEnabled   = checked
            }
        }

        spinInterpModel?.let { spinner ->
            val models = arrayOf("RIFE v4.6", "RIFE v4.0", "IFRNet", "FILM", "GMFlow")
            spinner.adapter = ArrayAdapter(requireContext(),
                android.R.layout.simple_spinner_dropdown_item, models)
            spinner.setSelection(viewModel.interpModelIndex.value ?: 0)
            spinner.isEnabled = viewModel.interpEnabled.value == true
            spinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
                override fun onItemSelected(p: AdapterView<*>?, v: View?, pos: Int, id: Long) {
                    viewModel.setInterpModel(pos)
                }
                override fun onNothingSelected(p: AdapterView<*>?) {}
            }
        }

        spinTargetFPS?.let { spinner ->
            val fpsOptions = arrayOf("48 FPS", "60 FPS", "90 FPS", "120 FPS", "144 FPS")
            spinner.adapter = ArrayAdapter(requireContext(),
                android.R.layout.simple_spinner_dropdown_item, fpsOptions)
            spinner.setSelection(viewModel.targetFPSIndex.value ?: 1)
            spinner.isEnabled = viewModel.interpEnabled.value == true
            spinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
                override fun onItemSelected(p: AdapterView<*>?, v: View?, pos: Int, id: Long) {
                    viewModel.setTargetFPS(pos)
                }
                override fun onNothingSelected(p: AdapterView<*>?) {}
            }
        }

        switchUpscale?.apply {
            isChecked = viewModel.upscaleEnabled.value == true
            setOnCheckedChangeListener { _, checked ->
                viewModel.setUpscale(checked)
                spinUpscaler?.isEnabled    = checked
                spinUpscaleMode?.isEnabled = checked
            }
        }

        spinUpscaler?.let { spinner ->
            val upscalers = arrayOf("RealESRGAN", "SPAN", "Anime4K", "FSRCNN")
            spinner.adapter = ArrayAdapter(requireContext(),
                android.R.layout.simple_spinner_dropdown_item, upscalers)
            spinner.setSelection(viewModel.upscalerIndex.value ?: 0)
            spinner.isEnabled = viewModel.upscaleEnabled.value == true
            spinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
                override fun onItemSelected(p: AdapterView<*>?, v: View?, pos: Int, id: Long) {
                    viewModel.setUpscaler(pos)
                }
                override fun onNothingSelected(p: AdapterView<*>?) {}
            }
        }

        spinUpscaleMode?.let { spinner ->
            val modes = arrayOf("2x", "4x", "8x")
            spinner.adapter = ArrayAdapter(requireContext(),
                android.R.layout.simple_spinner_dropdown_item, modes)
            spinner.setSelection(viewModel.upscaleModeIndex.value ?: 0)
            spinner.isEnabled = viewModel.upscaleEnabled.value == true
            spinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
                override fun onItemSelected(p: AdapterView<*>?, v: View?, pos: Int, id: Long) {
                    viewModel.setUpscaleMode(pos)
                }
                override fun onNothingSelected(p: AdapterView<*>?) {}
            }
        }

        switchAutoSelect?.apply {
            isChecked = viewModel.autoSelectEnabled.value == true
            setOnCheckedChangeListener { _, checked -> viewModel.setAutoSelect(checked) }
        }

        sbQuality?.apply {
            progress = viewModel.qualityLevel.value ?: 1
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(sb: SeekBar?, prog: Int, user: Boolean) {
                    val labels = arrayOf("Fast", "Balanced", "High", "Ultra")
                    tvQualityLabel?.text = labels.getOrElse(prog) { "Balanced" }
                    viewModel.setQualityLevel(prog)
                }
                override fun onStartTrackingTouch(sb: SeekBar?) {}
                override fun onStopTrackingTouch(sb: SeekBar?) {}
            })
        }
    }

    // ── Audio tab ─────────────────────────────────────────────────────────────
    private fun setupAudioTab(view: View) {
        val switchPassthrough = view.findViewById<Switch>(R.id.switchPassthrough)
        val sbAudioSync       = view.findViewById<SeekBar>(R.id.sbAudioSync)
        val tvSyncLabel       = view.findViewById<TextView>(R.id.tvSyncLabel)
        val switchNormalize   = view.findViewById<Switch>(R.id.switchNormalize)

        switchPassthrough?.apply {
            isChecked = viewModel.passthroughEnabled.value == true
            setOnCheckedChangeListener { _, checked -> viewModel.setPassthrough(checked) }
        }

        sbAudioSync?.apply {
            // Range: -500ms to +500ms → progress 0-1000, center=500
            progress = (viewModel.audioSyncOffset.value ?: 0) + 500
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(sb: SeekBar?, prog: Int, user: Boolean) {
                    val offset = prog - 500
                    tvSyncLabel?.text = "${offset}ms"
                    viewModel.setAudioSync(offset)
                }
                override fun onStartTrackingTouch(sb: SeekBar?) {}
                override fun onStopTrackingTouch(sb: SeekBar?) {}
            })
        }

        switchNormalize?.apply {
            isChecked = viewModel.normalizeAudio.value == true
            setOnCheckedChangeListener { _, checked -> viewModel.setNormalize(checked) }
        }
    }

    // ── Subtitle tab ──────────────────────────────────────────────────────────
    private fun setupSubtitleTab(view: View) {
        val sbFontSize    = view.findViewById<SeekBar>(R.id.sbSubFontSize)
        val tvFontLabel   = view.findViewById<TextView>(R.id.tvSubFontLabel)
        val switchBold    = view.findViewById<Switch>(R.id.switchSubBold)
        val switchOutline = view.findViewById<Switch>(R.id.switchSubOutline)
        val btnDownload   = view.findViewById<Button>(R.id.btnSubtitleDownload)
        val spinSubLang   = view.findViewById<Spinner>(R.id.spinSubLanguage)

        sbFontSize?.apply {
            progress = viewModel.subtitleFontSize.value ?: 16
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(sb: SeekBar?, prog: Int, user: Boolean) {
                    val size = prog.coerceIn(8, 64)
                    tvFontLabel?.text = "${size}sp"
                    viewModel.setSubtitleFontSize(size)
                }
                override fun onStartTrackingTouch(sb: SeekBar?) {}
                override fun onStopTrackingTouch(sb: SeekBar?) {}
            })
        }

        switchBold?.apply {
            isChecked = viewModel.subtitleBold.value == true
            setOnCheckedChangeListener { _, checked -> viewModel.setSubtitleBold(checked) }
        }

        switchOutline?.apply {
            isChecked = viewModel.subtitleOutline.value == true
            setOnCheckedChangeListener { _, checked -> viewModel.setSubtitleOutline(checked) }
        }

        spinSubLang?.let { spinner ->
            val langs = arrayOf("Auto", "English", "Indonesian", "Japanese",
                                "Chinese", "Korean", "Spanish", "French", "German")
            spinner.adapter = ArrayAdapter(requireContext(),
                android.R.layout.simple_spinner_dropdown_item, langs)
            spinner.setSelection(viewModel.subtitleLangIndex.value ?: 0)
            spinner.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
                override fun onItemSelected(p: AdapterView<*>?, v: View?, pos: Int, id: Long) {
                    viewModel.setSubtitleLang(pos)
                }
                override fun onNothingSelected(p: AdapterView<*>?) {}
            }
        }

        btnDownload?.setOnClickListener {
            viewModel.requestSubtitleDownload()
        }
    }

    // ── Advanced tab ──────────────────────────────────────────────────────────
    private fun setupAdvancedTab(view: View) {
        val switchDebugOverlay = view.findViewById<Switch>(R.id.switchDebugOverlay)
        val sbBufferSize       = view.findViewById<SeekBar>(R.id.sbBufferSize)
        val tvBufferLabel      = view.findViewById<TextView>(R.id.tvBufferLabel)
        val btnClearCache      = view.findViewById<Button>(R.id.btnClearCache)
        val switchVSync        = view.findViewById<Switch>(R.id.switchVSync)

        switchDebugOverlay?.apply {
            isChecked = viewModel.debugOverlay.value == true
            setOnCheckedChangeListener { _, checked -> viewModel.setDebugOverlay(checked) }
        }

        sbBufferSize?.apply {
            progress = viewModel.bufferSizeIndex.value ?: 2
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(sb: SeekBar?, prog: Int, user: Boolean) {
                    val sizes = arrayOf("1MB", "4MB", "8MB", "16MB", "32MB")
                    tvBufferLabel?.text = sizes.getOrElse(prog) { "8MB" }
                    viewModel.setBufferSize(prog)
                }
                override fun onStartTrackingTouch(sb: SeekBar?) {}
                override fun onStopTrackingTouch(sb: SeekBar?) {}
            })
        }

        btnClearCache?.setOnClickListener {
            viewModel.clearCache(requireContext())
            Toast.makeText(requireContext(), "Cache cleared", Toast.LENGTH_SHORT).show()
        }

        switchVSync?.apply {
            isChecked = viewModel.vsyncEnabled.value == true
            setOnCheckedChangeListener { _, checked -> viewModel.setVSync(checked) }
        }
    }
}
