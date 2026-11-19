package com.l4g.remote

import android.app.AlertDialog
import android.app.DownloadManager
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Environment
import android.util.Log
import androidx.core.content.FileProvider
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File

class AutoUpdater(private val context: Context) {
    companion object {
        private const val TAG = "AutoUpdater"
        private const val PROVIDER_AUTHORITY = "com.l4g.remote.provider"
        private const val UPDATE_CHECK_INTERVAL = 24 * 60 * 60 * 1000L // 24 hours
    }
    
    private var dohClient: DohClient? = null
    private var currentVersion = 14
    
    fun initialize(dohClient: DohClient, currentVersion: Int) {
        this.dohClient = dohClient
        this.currentVersion = currentVersion
    }
    
    suspend fun checkForUpdate(): Result<UpdateInfo?> = withContext(Dispatchers.IO) {
        if (dohClient == null) return@withContext Result.failure(Exception("DoH client not initialized"))
        
        return@withContext dohClient!!.checkUpdate(currentVersion)
    }
    
    fun showUpdateDialog(updateInfo: UpdateInfo, onConfirm: () -> Unit) {
        AlertDialog.Builder(context)
            .setTitle("Update Available v${updateInfo.version}")
            .setMessage(updateInfo.changelog)
            .setPositiveButton("Download") { _, _ ->
                downloadAndInstall(updateInfo)
                onConfirm()
            }
            .setNegativeButton("Later", null)
            .setCancelable(false)
            .show()
    }
    
    private fun downloadAndInstall(updateInfo: UpdateInfo) {
        val downloadManager = context.getSystemService(Context.DOWNLOAD_SERVICE) as DownloadManager
        
        // Save update binary to file
        val file = File(context.cacheDir, "L4G_Remote_v${updateInfo.version}.apk")
        file.writeBytes(updateInfo.data)
        
        val uri = FileProvider.getUriForFile(context, PROVIDER_AUTHORITY, file)
        
        val request = DownloadManager.Request(uri).apply {
            setTitle("L4G Remote Update")
            setDescription("Downloading v${updateInfo.version}...")
            setNotificationVisibility(DownloadManager.Request.VISIBILITY_VISIBLE_NOTIFY_COMPLETED)
            setDestinationInExternalFilesDir(context, Environment.DIRECTORY_DOWNLOADS, file.name)
            addRequestHeader("Content-Type", "application/vnd.android.package-archive")
        }
        
        val downloadId = downloadManager.enqueue(request)
        
        // Register broadcast receiver for completion
        // In practice, use a BroadcastReceiver to trigger install
    }
    
    fun installApk(file: File) {
        val uri = FileProvider.getUriForFile(context, PROVIDER_AUTHORITY, file)
        val intent = Intent(Intent.ACTION_INSTALL_PACKAGE).apply {
            data = uri
            flags = Intent.FLAG_GRANT_READ_URI_PERMISSION
        }
        context.startActivity(intent)
    }
    
    data class UpdateInfo(
        val version: Int,
        val changelog: String,
        val data: ByteArray
    )
}