# 署名と配布

配布するAPKはrelease variantに限ります。debug・検証用Activity・ゲームのバイナリを含めません。秘密鍵やパスワードはリポジトリ、配布物、ログへ記録しません。

1. 全ソースをコミットし、作業ツリーをクリーンにします。
2. `JAVA_HOME`をJDK 21、`ANDROID_HOME`をSDK、`ONEUI_SIGNING_PROPERTIES`を非公開の署名設定ファイルへ設定します。
3. `python3 tools/package_release.py --source-ref HEAD`を実行します。
4. `artifacts/release/バージョン/`の署名済みAPK、対応ソースZIP、SHA-256一覧、署名検証結果をGitHub Releaseへ添付します。

配布証明書のSHA-256：`f3a3227d5836f3906b033b82e894b493f82b779fda1d3972b30ebfd7e2cff3f0`。

スクリプトはC++試験、releaseビルド、lint、非debuggable、パッケージ、API 102と対象範囲、ネイティブエントリー、ライセンス同梱、16KiB alignment、署名を確認します。初版の実機検証に使った署名済みrelease APKも同じ鍵を使用します。

リリース文は外部利用者向けに、対応ゲーム・OS・使い方・更新操作・既知の制限を説明します。内部の関数名、フック件数、個人の端末設定、CIの作業経緯は載せません。既存タグは移動しません。

CIは未署名releaseを検証しますが、APKをアーティファクトへ保存しません。GitHub Releaseの公開時には管理アプリ向けのメタデータが作られます。署名鍵はアプリ別の非公開フォルダでパスワードと一緒にバックアップします。
