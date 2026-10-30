package dev.ryuya.legacygametools

import android.content.Context
import android.content.DialogInterface
import android.content.Intent
import android.content.SharedPreferences
import android.content.pm.PackageManager.NameNotFoundException
import android.net.Uri
import android.os.Bundle
import android.view.View
import android.widget.CompoundButton
import android.widget.CompoundButton.OnCheckedChangeListener
import android.widget.LinearLayout
import android.widget.LinearLayout.LayoutParams
import android.widget.ScrollView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.OnApplyWindowInsetsListener
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsCompat.Type
import com.google.android.material.R as MaterialR
import com.google.android.material.appbar.MaterialToolbar
import com.google.android.material.button.MaterialButton
import com.google.android.material.card.MaterialCardView
import com.google.android.material.color.MaterialColors
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.materialswitch.MaterialSwitch
import com.google.android.material.textview.MaterialTextView
import java.io.ByteArrayOutputStream
import java.io.IOException
import java.nio.charset.StandardCharsets
import java.util.HashMap

/* Each supported game gets an installed-version summary, toggle, and launch action. */
class MainActivity : AppCompatActivity() {
    private val statuses: MutableMap<String, MaterialTextView> = HashMap<String, MaterialTextView>()
    private lateinit var preferences: SharedPreferences

    public override fun onCreate(state: Bundle?) {
        super.onCreate(state)
        preferences = getSharedPreferences("settings", Context.MODE_PRIVATE)
        WindowCompat.setDecorFitsSystemWindows(getWindow(), false)
        val root = column()
        root.setBackgroundColor(MaterialColors.getColor(root, MaterialR.attr.colorSurface))
        setContentView(root)
        ViewCompat.setOnApplyWindowInsetsListener(
            root,
            OnApplyWindowInsetsListener { view: View, insets: WindowInsetsCompat ->
                val bars = insets.getInsets(Type.systemBars() or Type.displayCutout())
                view.setPadding(bars.left, bars.top, bars.right, bars.bottom)
                insets
            },
        )
        val toolbar = MaterialToolbar(this)
        toolbar.setTitle(R.string.app_name)
        root.addView(toolbar, LayoutParams(-1, -2))
        val scroll = ScrollView(this)
        root.addView(scroll, LayoutParams(-1, 0, 1f))
        val page = column()
        page.setPadding(dp(20f), dp(12f), dp(20f), dp(24f))
        scroll.addView(page)
        page.addView(text(getString(R.string.heading), true))
        page.addView(text(getString(R.string.setup), false))
        for (game in Games.PACKAGES) addGame(page, game)
        val about = card(page)
        about.addView(text(getString(R.string.about), true))
        about.addView(text(getString(R.string.scope_note), false))
        val source = button(getString(R.string.source))
        source.setOnClickListener(
            android.view.View.OnClickListener { view: View ->
                startActivity(
                    Intent(
                        Intent.ACTION_VIEW,
                        Uri.parse("https://github.com/OneUI-Mods/legacy-game-tools"),
                    )
                )
            }
        )
        about.addView(source)
        val licenses = button(getString(R.string.licenses))
        licenses.setOnClickListener(
            android.view.View.OnClickListener { view: View -> showLicenses() }
        )
        about.addView(licenses)
    }

    private fun addGame(page: LinearLayout, game: String) {
        val card = card(page)
        val label: String? = if (Games.D4U == game) "グルミク D4U" else "セカイ"
        var installed: Boolean
        var version: String?
        try {
            val info = getPackageManager().getPackageInfo(game, 0)
            version = info.versionName
            installed = true
        } catch (missing: NameNotFoundException) {
            version = getString(R.string.not_installed)
            installed = false
        }
        card.addView(text(label, true))
        card.addView(text(version, false))
        val toggle = MaterialSwitch(this)
        toggle.setText(R.string.compatibility)
        toggle.setMinHeight(dp(56f))
        toggle.setChecked(preferences.getBoolean(game, true))
        toggle.setOnCheckedChangeListener(
            OnCheckedChangeListener { button: CompoundButton?, checked: Boolean ->
                preferences.edit().putBoolean(game, checked).apply()
            }
        )
        card.addView(toggle)
        if (Games.SEKAI == game) {
            val life = MaterialSwitch(this)
            life.setText(R.string.prevent_life_loss)
            life.setMinHeight(dp(56f))
            life.setChecked(preferences.getBoolean("sekai.prevent_life_loss", true))
            life.setEnabled(toggle.isChecked())
            life.setOnCheckedChangeListener(
                OnCheckedChangeListener { button: CompoundButton?, checked: Boolean ->
                    preferences.edit().putBoolean("sekai.prevent_life_loss", checked).apply()
                }
            )
            toggle.setOnCheckedChangeListener(
                OnCheckedChangeListener { button: CompoundButton?, checked: Boolean ->
                    preferences.edit().putBoolean(game, checked).apply()
                    life.setEnabled(checked)
                    card.findViewWithTag<View?>("title_tap").setEnabled(checked)
                    card.findViewWithTag<View?>("fast_transition").setEnabled(checked)
                }
            )
            card.addView(life)
            card.addView(text(getString(R.string.prevent_life_loss_note), false))
            addSekaiOption(card, "title_tap", R.string.title_tap, toggle.isChecked())
            addSekaiOption(card, "fast_transition", R.string.fast_transition, toggle.isChecked())
            card.addView(text(getString(R.string.fast_transition_note), false))
        }
        card.addView(text(getString(R.string.restart_note), false))
        val status = text("", false)
        statuses.put(game, status)
        card.addView(status)
        val open = button(getString(R.string.open_game))
        open.setEnabled(installed)
        open.setOnClickListener(
            android.view.View.OnClickListener { view: View ->
                val launch = getPackageManager().getLaunchIntentForPackage(game)
                if (launch != null) startActivity(launch)
            }
        )
        card.addView(open)
    }

    private fun addSekaiOption(card: LinearLayout, key: String?, label: Int, enabled: Boolean) {
        val option = MaterialSwitch(this)
        option.setTag(key)
        option.setText(label)
        option.setMinHeight(dp(56f))
        option.setEnabled(enabled)
        option.setChecked(preferences.getBoolean("sekai." + key, true))
        option.setOnCheckedChangeListener(
            OnCheckedChangeListener { button: CompoundButton?, checked: Boolean ->
                preferences.edit().putBoolean("sekai." + key, checked).apply()
            }
        )
        card.addView(option)
    }

    override fun onResume() {
        super.onResume()
        statuses.forEach { (game: String, view: MaterialTextView) ->
            val status = preferences.getString(game + ".status", "not_run")
            val label: Int =
                when (status) {
                    "active" -> R.string.active
                    "disabled" -> R.string.disabled
                    "unsupported_engine",
                    "signature_mismatch" -> R.string.unsupported
                    "ready",
                    "waiting" -> R.string.waiting
                    "load_failed",
                    "hook_failed" -> R.string.failed
                    else -> R.string.not_run
                }
            view.setText(
                if ("not_run" == status) getString(label)
                else getString(R.string.last_status, getString(label))
            )
        }
    }

    private fun showLicenses() {
        val components =
            arrayOf<String>(
                "Legacy Game Tools · MIT",
                "AndroidX / Material · Apache 2.0",
                "libxposed API · Apache 2.0",
                "LLVM libc++ · Apache 2.0 with LLVM exceptions",
            )
        val files =
            arrayOf<String>(
                "APP_LICENSE.txt",
                "THIRD_PARTY_NOTICES.txt",
                "Apache-2.0.txt",
                "LIBCXX-NOTICE.txt",
            )
        MaterialAlertDialogBuilder(this)
            .setTitle(R.string.licenses)
            .setItems(
                components,
                android.content.DialogInterface.OnClickListener {
                    dialog: DialogInterface?,
                    index: Int ->
                    try {
                        getAssets().open(files[index]).use { input ->
                            val output = ByteArrayOutputStream()
                            val buffer = ByteArray(8192)
                            var count: Int
                            while ((input.read(buffer).also { count = it }) != -1) {
                                output.write(buffer, 0, count)
                            }
                            val content =
                                text(output.toString(StandardCharsets.UTF_8.name()), false)
                            content.setTextIsSelectable(true)
                            content.setPadding(dp(24f), dp(12f), dp(24f), dp(12f))
                            val scroll = ScrollView(this)
                            scroll.addView(content)
                            MaterialAlertDialogBuilder(this)
                                .setTitle(components[index])
                                .setView(scroll)
                                .setPositiveButton(R.string.close, null)
                                .show()
                        }
                    } catch (error: IOException) {
                        throw IllegalStateException("Bundled license missing", error)
                    }
                },
            )
            .setNegativeButton(R.string.close, null)
            .show()
    }

    private fun card(page: LinearLayout): LinearLayout {
        val card = MaterialCardView(this)
        card.setRadius(dp(24f).toFloat())
        card.setStrokeWidth(dp(1f))
        card.setStrokeColor(MaterialColors.getColor(card, MaterialR.attr.colorOutlineVariant))
        val layout = LayoutParams(-1, -2)
        layout.topMargin = dp(16f)
        page.addView(card, layout)
        val content = column()
        content.setPadding(dp(20f), dp(16f), dp(20f), dp(16f))
        card.addView(content)
        return content
    }

    private fun column(): LinearLayout {
        val result = LinearLayout(this)
        result.setOrientation(LinearLayout.VERTICAL)
        return result
    }

    private fun text(value: String?, title: Boolean): MaterialTextView {
        val result = MaterialTextView(this)
        result.setText(value)
        result.setTextAppearance(
            if (title) MaterialR.style.TextAppearance_Material3_TitleLarge
            else MaterialR.style.TextAppearance_Material3_BodyMedium
        )
        result.setPadding(0, dp(4f), 0, dp(8f))
        return result
    }

    private fun button(title: String?): MaterialButton {
        val result = MaterialButton(this)
        result.setText(title)
        result.setMinHeight(dp(48f))
        return result
    }

    private fun dp(value: Float): Int {
        return Math.round(value * getResources().getDisplayMetrics().density)
    }
}
