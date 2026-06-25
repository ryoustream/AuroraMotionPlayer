package com.aurora.player.benchmark

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels

// ============================================================================
//  Aurora Motion Player — BenchmarkFragment
//  Session 10: GPU Benchmark System
//
//  Overlay fragment that hosts BenchmarkOverlayView.
//  Attach/detach over the player surface via FragmentManager.
//  Toggle visibility with showOverlay() / hideOverlay() / toggleOverlay().
// ============================================================================

class BenchmarkFragment : Fragment() {

    private val viewModel: BenchmarkViewModel by activityViewModels()
    private var overlayView: BenchmarkOverlayView? = null

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?,
    ): View {
        val frame = FrameLayout(requireContext()).apply {
            layoutParams = FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT,
            )
        }

        val overlay = BenchmarkOverlayView(requireContext()).also { overlayView = it }
        frame.addView(overlay)
        return frame
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)

        // Observe benchmark stats
        viewModel.stats.observe(viewLifecycleOwner) { stats ->
            overlayView?.updateStats(stats)
        }

        viewModel.startPolling(500L)
    }

    override fun onDestroyView() {
        super.onDestroyView()
        viewModel.stopPolling()
        overlayView = null
    }

    // ── Public API ────────────────────────────────────────────────────────────
    fun showOverlay() { view?.visibility = View.VISIBLE }
    fun hideOverlay() { view?.visibility = View.GONE }
    fun toggleOverlay() {
        view?.let { it.visibility = if (it.visibility == View.VISIBLE) View.GONE else View.VISIBLE }
    }

    companion object {
        const val TAG = "BenchmarkFragment"
        fun newInstance() = BenchmarkFragment()
    }
}
