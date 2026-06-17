package com.aurora.player.util

import android.content.Context
import android.database.Cursor
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.DocumentsContract
import android.provider.MediaStore
import android.provider.OpenableColumns
import java.io.File

/**
 * Aurora Motion Player — UriUtils (Session 7)
 *
 * Resolves Android content:// and file:// URIs to real filesystem paths
 * for FFmpeg/NativePlayer.  Handles:
 *  - MediaStore content URIs          (content://media/...)
 *  - DocumentsProvider URIs           (content://com.android.externalstorage...)
 *  - SAF tree / document URIs         (content://com.android.providers.downloads...)
 *  - External SD card via SAF
 *  - USB OTG storage
 *  - Plain file:// URIs
 *  - Network URIs (http/https/rtsp/rtmp — returned as-is)
 *
 * When a real path cannot be resolved (e.g., scoped-storage-only URI),
 * the file is copied to the app's cache dir and the cache path is returned.
 */
object UriUtils {

    private val NETWORK_SCHEMES = setOf("http", "https", "rtsp", "rtmp", "rtmps", "ftp", "smb")

    /**
     * Attempts to resolve [uri] to an absolute filesystem path.
     * Returns null if the URI cannot be resolved to a path — in that case,
     * callers should use a [ContentResolver] stream (or call [copyToCache]).
     */
    fun resolveToPath(ctx: Context, uri: Uri): String? {
        val scheme = uri.scheme?.lowercase() ?: return null

        // ── Network URIs: pass through unchanged ──────────────────────────────
        if (scheme in NETWORK_SCHEMES) return uri.toString()

        // ── file:// ───────────────────────────────────────────────────────────
        if (scheme == "file") return uri.path

        // ── content:// ────────────────────────────────────────────────────────
        if (scheme != "content") return null

        val authority = uri.authority ?: return null

        // DocumentsProvider URI
        if (DocumentsContract.isDocumentUri(ctx, uri)) {
            when {
                isExternalStorageDocument(authority) ->
                    return resolveExternalStorageDoc(ctx, uri)

                isDownloadsDocument(authority) ->
                    return resolveDownloadsDoc(ctx, uri)

                isMediaDocument(authority) ->
                    return resolveMediaDoc(ctx, uri)
            }
        }

        // Generic MediaStore content URI
        if (authority.startsWith("media")) {
            return queryDataColumn(ctx, uri)
        }

        // Fallback: copy to cache
        return copyToCache(ctx, uri)
    }

    /** Returns display name (filename) for a URI. */
    fun getDisplayName(ctx: Context, uri: Uri): String? {
        if (uri.scheme == "file") return File(uri.path!!).name
        return ctx.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME),
            null, null, null)?.use { c ->
            if (c.moveToFirst()) c.getString(0) else null
        }
    }

    /**
     * Opens the URI as an InputStream and copies it to a temporary file
     * in the app cache directory.  Returns the path of the cache file,
     * or null on error.
     */
    fun copyToCache(ctx: Context, uri: Uri): String? {
        return try {
            val name = getDisplayName(ctx, uri) ?: "aurora_${System.currentTimeMillis()}.tmp"
            val dest = File(ctx.cacheDir, name)
            ctx.contentResolver.openInputStream(uri)?.use { input ->
                dest.outputStream().use { out -> input.copyTo(out, bufferSize = 65536) }
            }
            dest.absolutePath
        } catch (e: Exception) {
            null
        }
    }

    /** Returns the file size in bytes, or -1 on failure. */
    fun getFileSize(ctx: Context, uri: Uri): Long {
        if (uri.scheme == "file") return File(uri.path!!).length()
        return ctx.contentResolver.query(uri, arrayOf(OpenableColumns.SIZE),
            null, null, null)?.use { c ->
            if (c.moveToFirst()) c.getLong(0) else -1L
        } ?: -1L
    }

    /** Checks whether this URI points to a network stream. */
    fun isNetworkUri(uri: Uri): Boolean = uri.scheme?.lowercase() in NETWORK_SCHEMES

    // ── Private resolvers ─────────────────────────────────────────────────────

    private fun resolveExternalStorageDoc(ctx: Context, uri: Uri): String? {
        val docId = DocumentsContract.getDocumentId(uri)
        val split = docId.split(":")
        val type  = split.getOrNull(0) ?: return null
        val rel   = split.getOrNull(1) ?: ""
        return if (type.equals("primary", ignoreCase = true)) {
            "${Environment.getExternalStorageDirectory()}/$rel"
        } else {
            // SD card / USB OTG — try to find the volume path
            resolveRemovableStoragePath(ctx, type, rel)
        }
    }

    private fun resolveRemovableStoragePath(ctx: Context, volumeId: String, rel: String): String? {
        // Walk StorageManager volumes to find matching uuid/id
        val sm = ctx.getSystemService(Context.STORAGE_SERVICE) as android.os.storage.StorageManager
        return try {
            val volumes = sm.storageVolumes
            volumes.firstOrNull { v ->
                v.uuid?.equals(volumeId, ignoreCase = true) == true ||
                v.toString().contains(volumeId, ignoreCase = true)
            }?.let { v ->
                val dirMethod = v.javaClass.getMethod("getDirectory")
                val dir = dirMethod.invoke(v) as? File
                dir?.let { "${it.absolutePath}/$rel" }
            }
        } catch (e: Exception) {
            null
        }
    }

    private fun resolveDownloadsDoc(ctx: Context, uri: Uri): String? {
        val id = DocumentsContract.getDocumentId(uri)
        // On API 29+ the id can be a raw path
        if (id.startsWith("raw:")) return id.removePrefix("raw:")
        val contentUri = try {
            Uri.parse("content://downloads/public_downloads")
                .buildUpon().appendPath(id).build()
        } catch (e: Exception) {
            return null
        }
        return queryDataColumn(ctx, contentUri)
    }

    private fun resolveMediaDoc(ctx: Context, uri: Uri): String? {
        val docId = DocumentsContract.getDocumentId(uri)
        val split = docId.split(":")
        val contentUri = when (split.getOrNull(0)) {
            "image" -> MediaStore.Images.Media.EXTERNAL_CONTENT_URI
            "video" -> MediaStore.Video.Media.EXTERNAL_CONTENT_URI
            "audio" -> MediaStore.Audio.Media.EXTERNAL_CONTENT_URI
            else    -> return null
        }
        val id = split.getOrNull(1) ?: return null
        return queryDataColumn(ctx, contentUri, "_id=?", arrayOf(id))
    }

    private fun queryDataColumn(
        ctx       : Context,
        uri       : Uri,
        selection : String? = null,
        args      : Array<String>? = null
    ): String? {
        val col = arrayOf(MediaStore.MediaColumns.DATA)
        return ctx.contentResolver.query(uri, col, selection, args, null)?.use { c ->
            if (c.moveToFirst()) {
                val idx = c.getColumnIndex(MediaStore.MediaColumns.DATA)
                if (idx >= 0) c.getString(idx) else null
            } else null
        }
    }

    // ── Authority helpers ─────────────────────────────────────────────────────

    private fun isExternalStorageDocument(auth: String) =
        auth == "com.android.externalstorage.documents"

    private fun isDownloadsDocument(auth: String) =
        auth == "com.android.providers.downloads.documents"

    private fun isMediaDocument(auth: String) =
        auth == "com.android.providers.media.documents"
}
