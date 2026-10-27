package dev.ryuya.legacygametools;

final class Games {
  static final String D4U = "com.bushiroad.d4djddedition";
  static final String SEKAI = "com.sega.pjsekai.re";
  static final String[] PACKAGES = {D4U, SEKAI};

  static boolean supported(String name) {
    return D4U.equals(name) || SEKAI.equals(name);
  }

  private Games() {}
}
