package dev.ryuya.legacygametools;

import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Bundle;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowCompat;
import androidx.core.view.WindowInsetsCompat;
import com.google.android.material.appbar.MaterialToolbar;
import com.google.android.material.button.MaterialButton;
import com.google.android.material.card.MaterialCardView;
import com.google.android.material.color.MaterialColors;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.materialswitch.MaterialSwitch;
import com.google.android.material.textview.MaterialTextView;
import java.io.IOException;
import java.io.ByteArrayOutputStream;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.Map;

/** Each supported game gets an installed-version summary, toggle, and launch action. */
public final class MainActivity extends AppCompatActivity {
  private final Map<String, MaterialTextView> statuses = new HashMap<>();
  private SharedPreferences preferences;

  @Override
  public void onCreate(Bundle state) {
    super.onCreate(state);
    preferences = getSharedPreferences("settings", MODE_PRIVATE);
    WindowCompat.setDecorFitsSystemWindows(getWindow(), false);
    LinearLayout root = column();
    root.setBackgroundColor(
        MaterialColors.getColor(root, com.google.android.material.R.attr.colorSurface));
    setContentView(root);
    ViewCompat.setOnApplyWindowInsetsListener(
        root,
        (view, insets) -> {
          var bars =
              insets.getInsets(
                  WindowInsetsCompat.Type.systemBars() | WindowInsetsCompat.Type.displayCutout());
          view.setPadding(bars.left, bars.top, bars.right, bars.bottom);
          return insets;
        });
    MaterialToolbar toolbar = new MaterialToolbar(this);
    toolbar.setTitle(R.string.app_name);
    root.addView(toolbar, new LinearLayout.LayoutParams(-1, -2));
    ScrollView scroll = new ScrollView(this);
    root.addView(scroll, new LinearLayout.LayoutParams(-1, 0, 1));
    LinearLayout page = column();
    page.setPadding(dp(20), dp(12), dp(20), dp(24));
    scroll.addView(page);
    page.addView(text(getString(R.string.heading), true));
    page.addView(text(getString(R.string.setup), false));
    for (String game : Games.PACKAGES) addGame(page, game);
    LinearLayout about = card(page);
    about.addView(text(getString(R.string.about), true));
    about.addView(text(getString(R.string.scope_note), false));
    MaterialButton source = button(getString(R.string.source));
    source.setOnClickListener(
        view ->
            startActivity(
                new Intent(
                    Intent.ACTION_VIEW,
                    Uri.parse("https://github.com/OneUI-Mods/legacy-game-tools"))));
    about.addView(source);
    MaterialButton licenses = button(getString(R.string.licenses));
    licenses.setOnClickListener(view -> showLicenses());
    about.addView(licenses);
  }

  private void addGame(LinearLayout page, String game) {
    LinearLayout card = card(page);
    String label = Games.D4U.equals(game) ? "グルミク D4U" : "セカイ";
    boolean installed;
    String version;
    try {
      var info = getPackageManager().getPackageInfo(game, 0);
      version = info.versionName;
      installed = true;
    } catch (PackageManager.NameNotFoundException missing) {
      version = getString(R.string.not_installed);
      installed = false;
    }
    card.addView(text(label, true));
    card.addView(text(version, false));
    MaterialSwitch toggle = new MaterialSwitch(this);
    toggle.setText(R.string.compatibility);
    toggle.setMinHeight(dp(56));
    toggle.setChecked(preferences.getBoolean(game, true));
    toggle.setOnCheckedChangeListener(
        (button, checked) -> preferences.edit().putBoolean(game, checked).apply());
    card.addView(toggle);
    if (Games.SEKAI.equals(game)) {
      MaterialSwitch life = new MaterialSwitch(this);
      life.setText(R.string.prevent_life_loss);
      life.setMinHeight(dp(56));
      life.setChecked(preferences.getBoolean("sekai.prevent_life_loss", true));
      life.setEnabled(toggle.isChecked());
      life.setOnCheckedChangeListener(
          (button, checked) -> preferences.edit().putBoolean("sekai.prevent_life_loss", checked).apply());
      toggle.setOnCheckedChangeListener(
          (button, checked) -> {
            preferences.edit().putBoolean(game, checked).apply();
            life.setEnabled(checked);
          });
      card.addView(life);
      card.addView(text(getString(R.string.prevent_life_loss_note), false));
    }
    card.addView(text(getString(R.string.restart_note), false));
    MaterialTextView status = text("", false);
    statuses.put(game, status);
    card.addView(status);
    MaterialButton open = button(getString(R.string.open_game));
    open.setEnabled(installed);
    open.setOnClickListener(
        view -> {
          Intent launch = getPackageManager().getLaunchIntentForPackage(game);
          if (launch != null) startActivity(launch);
        });
    card.addView(open);
  }

  @Override
  protected void onResume() {
    super.onResume();
    statuses.forEach(
        (game, view) -> {
          String status = preferences.getString(game + ".status", "not_run");
          int label =
              switch (status) {
                case "active" -> R.string.active;
                case "disabled" -> R.string.disabled;
                case "unsupported_engine", "signature_mismatch" -> R.string.unsupported;
                case "ready", "waiting" -> R.string.waiting;
                case "load_failed", "hook_failed" -> R.string.failed;
                default -> R.string.not_run;
              };
          view.setText(
              "not_run".equals(status)
                  ? getString(label)
                  : getString(R.string.last_status, getString(label)));
        });
  }

  private void showLicenses() {
    String[] components = {
      "Legacy Game Tools · MIT",
      "AndroidX / Material · Apache 2.0",
      "libxposed API · Apache 2.0",
      "LLVM libc++ · Apache 2.0 with LLVM exceptions"
    };
    String[] files = {
      "APP_LICENSE.txt", "THIRD_PARTY_NOTICES.txt", "Apache-2.0.txt", "LIBCXX-NOTICE.txt"
    };
    new MaterialAlertDialogBuilder(this)
        .setTitle(R.string.licenses)
        .setItems(
            components,
            (dialog, index) -> {
              try (var input = getAssets().open(files[index])) {
                ByteArrayOutputStream output = new ByteArrayOutputStream();
                byte[] buffer = new byte[8192];
                for (int count; (count = input.read(buffer)) != -1; )
                  output.write(buffer, 0, count);
                MaterialTextView content =
                    text(output.toString(StandardCharsets.UTF_8.name()), false);
                content.setTextIsSelectable(true);
                content.setPadding(dp(24), dp(12), dp(24), dp(12));
                ScrollView scroll = new ScrollView(this);
                scroll.addView(content);
                new MaterialAlertDialogBuilder(this)
                    .setTitle(components[index])
                    .setView(scroll)
                    .setPositiveButton(R.string.close, null)
                    .show();
              } catch (IOException error) {
                throw new IllegalStateException("Bundled license missing", error);
              }
            })
        .setNegativeButton(R.string.close, null)
        .show();
  }

  private LinearLayout card(LinearLayout page) {
    MaterialCardView card = new MaterialCardView(this);
    card.setRadius(dp(24));
    card.setStrokeWidth(dp(1));
    card.setStrokeColor(
        MaterialColors.getColor(card, com.google.android.material.R.attr.colorOutlineVariant));
    LinearLayout.LayoutParams layout = new LinearLayout.LayoutParams(-1, -2);
    layout.topMargin = dp(16);
    page.addView(card, layout);
    LinearLayout content = column();
    content.setPadding(dp(20), dp(16), dp(20), dp(16));
    card.addView(content);
    return content;
  }

  private LinearLayout column() {
    LinearLayout result = new LinearLayout(this);
    result.setOrientation(LinearLayout.VERTICAL);
    return result;
  }

  private MaterialTextView text(String value, boolean title) {
    MaterialTextView result = new MaterialTextView(this);
    result.setText(value);
    result.setTextAppearance(
        title
            ? com.google.android.material.R.style.TextAppearance_Material3_TitleLarge
            : com.google.android.material.R.style.TextAppearance_Material3_BodyMedium);
    result.setPadding(0, dp(4), 0, dp(8));
    return result;
  }

  private MaterialButton button(String title) {
    MaterialButton result = new MaterialButton(this);
    result.setText(title);
    result.setMinHeight(dp(48));
    return result;
  }

  private int dp(float value) {
    return Math.round(value * getResources().getDisplayMetrics().density);
  }
}
