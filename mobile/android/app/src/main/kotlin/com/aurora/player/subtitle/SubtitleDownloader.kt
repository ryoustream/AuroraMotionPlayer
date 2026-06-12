package com.aurora.player.subtitle

import android.content.Context
import android.net.Uri
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File
import java.io.FileOutputStream
import java.net.HttpURLConnection
import java.net.URL
import java.util.zip.ZipInputStream

/**
 * Aurora Motion Player — Subtitle Downloader
 *
 * Downloads subtitles from OpenSubtitles REST API v1.
 * Supports: SRT, ASS, VTT
 *
 * Flow:
 *  1. Search by filename hash or movie title
 *  2. Pick best match by language preference
 *  3. Download & decompress .gz
 *  4. Save to app-specific external storage
 *  5. Return file Uri for SubtitleEngine
 */
class SubtitleDownloader(private val context: Context) {

    companion object {
        private const val API_BASE    = "https://api.opensubtitles.com/api/v1"
        private const val USER_AGENT  = "AuroraMotionPlayer v1.0"
        private const val APP_API_KEY = ""  // User must supply their free API key via settings
        private const val TIMEOUT_MS  = 15_000
    }

    // ── Result types ──────────────────────────────────────────────────────────
    sealed class SearchResult {
        data class Success(val subtitles: List<SubtitleEntry>) : SearchResult()
        data class Error(val message: String)                   : SearchResult()
        object NoResults                                        : SearchResult()
    }

    sealed class DownloadResult {
        data class Success(val file: File, val uri: Uri) : DownloadResult()
        data class Error(val message: String)             : DownloadResult()
    }

    data class SubtitleEntry(
        val id          : String,
        val language    : String,
        val languageName: String,
        val filename    : String,
        val format      : String,   // srt | ass | vtt
        val downloadUrl : String,
        val rating      : Float,
        val downloadCount: Int,
        val hearingImpaired: Boolean
    )

    // ── Search ────────────────────────────────────────────────────────────────
    suspend fun searchByTitle(
        title      : String,
        year       : Int?   = null,
        language   : String = "en"
    ): SearchResult = withContext(Dispatchers.IO) {
        try {
            val query = buildString {
                append("$API_BASE/subtitles?query=")
                append(Uri.encode(title))
                append("&languages=$language")
                if (year != null) append("&year=$year")
            }
            val json = httpGet(query) ?: return@withContext SearchResult.Error("Network error")
            parseSearchResponse(json)
        } catch (e: Exception) {
            SearchResult.Error(e.message ?: "Unknown error")
        }
    }

    suspend fun searchByHash(
        movieHash : String,
        movieSize : Long,
        language  : String = "en"
    ): SearchResult = withContext(Dispatchers.IO) {
        try {
            val query = "$API_BASE/subtitles?moviehash=$movieHash&languages=$language"
            val json  = httpGet(query) ?: return@withContext SearchResult.Error("Network error")
            parseSearchResponse(json)
        } catch (e: Exception) {
            SearchResult.Error(e.message ?: "Unknown error")
        }
    }

    // ── Download ──────────────────────────────────────────────────────────────
    suspend fun download(entry: SubtitleEntry): DownloadResult = withContext(Dispatchers.IO) {
        try {
            val dir  = getSubtitleDir()
            val file = File(dir, sanitizeFilename(entry.filename))

            if (file.exists()) {
                return@withContext DownloadResult.Success(file, Uri.fromFile(file))
            }

            // Request download link
            val linkJson = httpPost(
                "$API_BASE/download",
                """{"file_id":"${entry.id}","sub_format":"${entry.format}"}"""
            ) ?: return@withContext DownloadResult.Error("Failed to get download link")

            val downloadUrl = parseDownloadUrl(linkJson)
                ?: return@withContext DownloadResult.Error("Invalid download response")

            // Download file
            downloadFile(downloadUrl, file)

            DownloadResult.Success(file, Uri.fromFile(file))
        } catch (e: Exception) {
            DownloadResult.Error(e.message ?: "Unknown download error")
        }
    }

    // ── File hash (OpenSubtitles algorithm) ───────────────────────────────────
    fun computeMovieHash(file: File): String {
        val chunkSize = 65536L
        val fileSize  = file.length()
        if (fileSize < chunkSize) return "0".repeat(16)

        var hash = fileSize
        file.inputStream().use { stream ->
            // Read first 64KB
            val buf = ByteArray(8)
            repeat((chunkSize / 8).toInt()) {
                stream.read(buf)
                hash += buf.toLong()
            }
            // Seek to last 64KB
            stream.skip(fileSize - 2 * chunkSize)
            repeat((chunkSize / 8).toInt()) {
                stream.read(buf)
                hash += buf.toLong()
            }
        }
        return hash.toULong().toString(16).padStart(16, '0')
    }

    // ── Helpers ───────────────────────────────────────────────────────────────
    private fun getSubtitleDir(): File {
        val dir = File(context.getExternalFilesDir(null), "subtitles")
        if (!dir.exists()) dir.mkdirs()
        return dir
    }

    private fun sanitizeFilename(name: String): String =
        name.replace(Regex("[^a-zA-Z0-9._\\-]"), "_")

    private fun httpGet(url: String): String? {
        val conn = URL(url).openConnection() as HttpURLConnection
        conn.apply {
            requestMethod  = "GET"
            connectTimeout = TIMEOUT_MS
            readTimeout    = TIMEOUT_MS
            setRequestProperty("User-Agent",  USER_AGENT)
            setRequestProperty("Api-Key",     APP_API_KEY)
            setRequestProperty("Content-Type","application/json")
            setRequestProperty("Accept",      "application/json")
        }
        return if (conn.responseCode == 200) conn.inputStream.bufferedReader().readText()
        else null
    }

    private fun httpPost(url: String, body: String): String? {
        val conn = URL(url).openConnection() as HttpURLConnection
        conn.apply {
            requestMethod  = "POST"
            doOutput       = true
            connectTimeout = TIMEOUT_MS
            readTimeout    = TIMEOUT_MS
            setRequestProperty("User-Agent",  USER_AGENT)
            setRequestProperty("Api-Key",     APP_API_KEY)
            setRequestProperty("Content-Type","application/json")
            setRequestProperty("Accept",      "application/json")
            outputStream.use { it.write(body.toByteArray()) }
        }
        return if (conn.responseCode == 200) conn.inputStream.bufferedReader().readText()
        else null
    }

    private fun downloadFile(url: String, dest: File) {
        val conn = URL(url).openConnection() as HttpURLConnection
        conn.connectTimeout = TIMEOUT_MS
        conn.readTimeout    = 60_000

        conn.inputStream.use { input ->
            // Handle .gz compression
            val stream = if (url.endsWith(".gz")) {
                java.util.zip.GZIPInputStream(input)
            } else input

            FileOutputStream(dest).use { out ->
                stream.copyTo(out, bufferSize = 8192)
            }
        }
    }

    // ── JSON parsers (minimal, no external dep) ───────────────────────────────
    private fun parseSearchResponse(json: String): SearchResult {
        // Minimal regex-based parser for OpenSubtitles v1 response
        if (!json.contains("\"data\"")) return SearchResult.NoResults

        val entries = mutableListOf<SubtitleEntry>()
        val dataRegex  = """"data"\s*:\s*\[(.+?)\]""".toRegex(RegexOption.DOT_MATCHES_ALL)
        val dataMatch  = dataRegex.find(json) ?: return SearchResult.NoResults
        val dataArray  = dataMatch.groupValues[1]

        // Split objects by "id" field occurrences
        val idRegex = """"id"\s*:\s*"([^"]+)"""".toRegex()
        val ids     = idRegex.findAll(dataArray).map { it.groupValues[1] }.toList()

        for (id in ids.take(20)) {
            entries.add(SubtitleEntry(
                id              = id,
                language        = extractField(dataArray, id, "language") ?: "en",
                languageName    = extractField(dataArray, id, "language_name") ?: "English",
                filename        = extractField(dataArray, id, "file_name") ?: "$id.srt",
                format          = extractField(dataArray, id, "format") ?: "srt",
                downloadUrl     = "",
                rating          = extractField(dataArray, id, "ratings")?.toFloatOrNull() ?: 0f,
                downloadCount   = extractField(dataArray, id, "download_count")?.toIntOrNull() ?: 0,
                hearingImpaired = extractField(dataArray, id, "hearing_impaired") == "true"
            ))
        }

        return if (entries.isEmpty()) SearchResult.NoResults
        else SearchResult.Success(entries)
    }

    private fun parseDownloadUrl(json: String): String? {
        val match = """"link"\s*:\s*"([^"]+)"""".toRegex().find(json)
        return match?.groupValues?.get(1)
    }

    private fun extractField(json: String, id: String, field: String): String? {
        val pattern = """"$field"\s*:\s*"?([^",}\]]+)"?""".toRegex()
        // Find the region near the id occurrence
        val idIdx = json.indexOf(id)
        if (idIdx < 0) return null
        val region = json.substring(idIdx, minOf(idIdx + 2000, json.length))
        return pattern.find(region)?.groupValues?.get(1)?.trim()
    }

    private fun ByteArray.toLong(): Long {
        var result = 0L
        for (i in indices) {
            result = result or ((this[i].toLong() and 0xFF) shl (8 * i))
        }
        return result
    }
}
