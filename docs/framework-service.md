# API 102 アプリ側のBinder接続

フック実装用のcompileOnly APIと、設定アプリへ配信されるServiceは別の接続です。Vector v2.2 / 3112（ddeed8ca）のModuleAppServiceは、`<applicationId>.XposedService`を探し、SEND_BINDERのBundleでIXposedServiceを渡します。受け口がなかったためproviderを取得できず、3回の失敗後は再試行の抑制へ入っていました。

`implementation("io.github.libxposed:service:102.0.0")`で公式XposedProviderをマニフェストへマージします。ServiceHelperはApplication生成前の配信をキャッシュし、Application.onCreateで登録したlistenerへ渡します。listener内でリモート問い合わせを待たず、1本の遅延生成workerでAPI版・framework名・版番号を読み、OneUIModsBinderへ結果を記録します。フレームワーク不在時はworkerも生成せず、待機やポーリングをしません。Binder死亡通知は接続断として記録します。

既存Applicationのテーマ処理・起動処理は残します。新しい権限、フォアグラウンドサービス、自動root、フック対象の追加、設定の移行やリモート設定への置き換えはありません。デーモン、SystemUI、system_server、ホームやカメラの再起動は不要です。

ServiceとInterface 102.0.0のApache 2.0表記をライセンスへ追加し、release検査で公式providerとauthorityの欠落を拒否します。

参照:
- [稼働Vectorと同じリビジョンの配信処理](https://github.com/JingMatrix/Vector/blob/ddeed8ca1ffedfaba7f4341b911089906862d5c4/daemon/src/main/kotlin/org/matrix/vector/daemon/ipc/ModuleAppService.kt)
- [公式Serviceの導入方法](https://github.com/JingMatrix/libxposed-service/tree/3318940876192e29cf6ab07637e899e22a87ebf0)

実機試験ではproviderの存在だけでなく、Vectorの配信成功ログとアプリ側のメタデータ往復を確認します。再起動後の再接続はアプリ自身のプロセスだけで検証し、ホストプロセスの再起動とは区別します。

## 2026-10-31の確認

同じ配布鍵の署名済みreleaseをAndroid 17 / One UI 9 BZI9の実機へ上書きし、端末上のAPKハッシュが検査済みAPKと一致することを確認しました。公式providerの公開後、Vector API 102 / framework version 3112のメタデータ往復が成功。設定アプリ自身のプロセスを終了・再生成した後も再接続しました。

8本全体でJVM試験369件とrelease lint・ライセンス・署名・構造の検査が成功し、全アプリの初回接続8件とプロセス再生成後の接続8件を確認しています。受信後の期間で対象8本の新しいBinder失敗警告は0件でした。Captureは最初の配信が既存の再試行抑制に待たされた後、接続と再接続が成立しました。

設定の照合で機能設定の差分は0。状態時刻などの診断情報のみ更新されました。OSのboot IDとsystem_server・SystemUI・ホームのPIDは開始前と一致しました。Vectorデーモンの再起動操作、ホストの再起動、ソフトリブート、音声再生は行っていません。Home Tweaksの純正部品プレビューも引き続き描画されました。

この試験はアプリ側の接続を確認したもので、全フック機能やAndroid 16実機を再試験したものではありません。Android 16/17の既存契約やネイティブ処理は変更していません。生ログ・設定・画面は公開しません。
