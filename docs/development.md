# 開発者向け

## 起動クラッシュの原因

確認したUnityはD4Uが2019.4.1f1、セカイが2019.4.3f1です。両方ともUnityのメモリ追跡でSIGSEGVが発生しました。上位32bitごとの追跡テーブルが5枠しかなく、Scudoがアドレスを分散すると空き枠を使い切り、無効なインデックスを参照します。実際に16GiB以上のRAMを消費したという意味ではありません。

Unity公式の説明：[Android 11 ARM64 native heap allocator issues](https://discussions.unity.com/t/android-11-arm64-native-heap-allocator-issues/826358?page=2)。アプリ作者向けのエンジン更新は本モジュールとは別の修正方法です。

## 実装

`HookEntry`は対象ゲームのメインプロセスに限り、`Application.attach`で自アプリのProviderからスイッチを読み、ネイティブライブラリをロードします。設定取得が失敗した場合は無効のままです。VectorのネイティブAPIがUnityのロードを通知した後、ELF Build ID・実行領域・関数先頭を照合してから追跡処理を補います。

`Compatibility.cpp`の3つのラッパーは元の追跡表で扱えるポインターを元の関数へ渡します。5枠に収まらないポインターのみ、allocatorの所有者とアドレスの組を`OverflowTracker`へ追加します。検索は両方を確認し、解放時は追加分から削除します。実際のmalloc/free、ゲームの保存データ、通信は変更しません。

追加表はmutexで保護します。対応外のエンジン・署名不一致の関数にはフックしません。途中でフックのインストールが失敗した場合は、先に入れたフックを取り外します。原本のAPKや共有ライブラリは書き換えません。

Providerの読み取りと診断書き込みは、対象ゲームのUIDから、そのゲーム自身の項目だけに許可します。設定変更は自アプリ内で行います。インターネット権限は要求しません。

## ビルドと確認

- JDK 21、Gradle 9.6.1、AGP 9.3.2
- compile/target SDK 37、Build Tools 37.0.0
- NDK 30.0.16248370、CMake 3.22.1、arm64-v8a
- Material 3、libxposed API 102

```sh
python3 tools/check_native.py
./gradlew :app:assembleRelease :app:lintRelease
```

ホスト側のC++試験で、離れた多数のアドレス領域、異なるallocator、重複登録、解放後のアドレス再利用を確認します。ゲームのバイナリは試験に不要です。

Android 17の実機では修正前に2本とも同じ追跡系統で起動クラッシュを確認しました。修正後は2本の互換処理の有効化とプロセスの継続を確認し、D4Uは「Connecting…」、セカイはSEGAロゴ後の未成年向け注意書きまで描画されました。サービス終了後の通信待ちや、プレイ全体は確認対象に含めません。

実機のログ・スクリーンショット・抽出した共有ライブラリ・署名情報は公開ソースに追加しません。診断目的で強制終了したのは対象ゲーム2本だけです。端末・SystemUI・One UIホームは再起動していません。

## セカイの英数字BPM ID

対象バージョン・ELF照合・呼び出し元とパターンの限定・検査は[実装メモ](sekai-bpm.md)を参照してください。Pythonを使わないビルドは `node tools/check_native.mjs` と `node tools/package_release.mjs --source-ref HEAD` で実行できます。

- [Kotlin移行の設計と検証範囲](kotlin-migration.md)
