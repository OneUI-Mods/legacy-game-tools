package dev.ryuya.legacygametools

import android.content.ContentProvider
import android.content.ContentValues
import android.database.Cursor
import android.net.Uri
import android.os.Binder
import android.os.Bundle
import java.util.Arrays
import kotlin.jvm.JvmField

/* Target games may read their own toggle and report status; only this app can edit settings. */
class ConfigProvider : ContentProvider() {
    override fun onCreate(): Boolean {
        return true
    }

    override fun call(method: String, game: String?, extras: Bundle?): Bundle {
        require(Games.supported(game)) { "Unknown game" }
        val callers =
            requireNotNull(context).getPackageManager().getPackagesForUid(Binder.getCallingUid())
        if (callers == null || !Arrays.asList<String?>(*callers).contains(game))
            throw SecurityException("Target game required")
        val preferences = requireNotNull(context).getSharedPreferences("settings", 0)
        val result = Bundle()
        if ("read" == method) {
            result.putBoolean("enabled", preferences.getBoolean(game, true))
            result.putBoolean(
                "prevent_life_loss",
                Games.SEKAI == game && preferences.getBoolean("sekai.prevent_life_loss", true),
            )
            result.putBoolean(
                "title_tap",
                Games.SEKAI == game && preferences.getBoolean("sekai.title_tap", true),
            )
            result.putBoolean(
                "fast_transition",
                Games.SEKAI == game && preferences.getBoolean("sekai.fast_transition", true),
            )
        } else if ("report" == method && extras != null) {
            var state = extras.getString("status", "unknown")
            if (
                !java.util.Arrays.asList<String?>(
                        "active",
                        "ready",
                        "waiting",
                        "disabled",
                        "unsupported_engine",
                        "signature_mismatch",
                        "hook_failed",
                        "load_failed",
                    )
                    .contains(state)
            )
                state = "unknown"
            preferences
                .edit()
                .putString(game + ".status", state)
                .putLong(game + ".updated", System.currentTimeMillis())
                .apply()
        } else throw IllegalArgumentException("Unknown operation")
        return result
    }

    override fun query(
        uri: Uri,
        projection: Array<String?>?,
        selection: String?,
        args: Array<String?>?,
        sort: String?,
    ): Cursor? {
        return null
    }

    override fun getType(uri: Uri): String? {
        return null
    }

    override fun insert(uri: Uri, values: ContentValues?): Uri? {
        throw UnsupportedOperationException()
    }

    override fun delete(uri: Uri, selection: String?, args: Array<String?>?): Int {
        throw UnsupportedOperationException()
    }

    override fun update(
        uri: Uri,
        values: ContentValues?,
        selection: String?,
        args: Array<String?>?,
    ): Int {
        throw UnsupportedOperationException()
    }

    companion object {
        const val AUTHORITY: String = "dev.ryuya.legacygametools.config"
        @JvmField val URI: Uri = Uri.parse("content://" + ConfigProvider.AUTHORITY)
    }
}
