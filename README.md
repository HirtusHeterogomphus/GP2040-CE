# GP2040-CE Switch Pro Controller Fork

これは [OpenStickCommunity/GP2040-CE](https://github.com/OpenStickCommunity/GP2040-CE) をベースにした、Switch Pro コントローラ互換を主目的とする RP2040 向けファームウェアです。

フォーク元の GP2040-CE が持つ低遅延なゲームパッド基盤と Web Configurator を利用しつつ、アナログスティックの校正、LSM6DS3 ジャイロセンサー、Switch Pro IMU レポートまわりを重点的に変更しています。

## 主な変更点

### アナログスティック校正

フォーク元の中心値ベースの補正処理から、スティックごとに `min` / `max` / `neutral` / `deadzone` を保存して扱う校正方式に変更しています。

- `AnalogJoystick` を使って、ADC の実測範囲をもとに X/Y 軸を正規化します。
- `DiagonalCompensation` により、必要に応じて対角方向の補正を行います。
- Web Configurator では、まずニュートラル位置を取得し、その後スティックを外周に沿って回して X/Y の最小値・最大値を測定します。
- 校正値は Web Configurator から保存し、再起動後の入力処理に反映されます。

この校正フローでは、従来のように中心値だけで左右・上下を分割するのではなく、実際のスティック可動範囲を保存して使うため、個体差のあるジョイスティックを Switch Pro 用の左右スティックとして扱いやすくしています。

### Gyro アドオン

LSM6DS3 を使う I2C ジャイロアドオンを追加しています。

- I2C アドレスは `0x6A`、`0x6B`、または自動検出を選択できます。
- 加速度センサーとジャイロスコープのオフセット値を設定として保存できます。
- Web Configurator からセンサーのバイアス値を測定し、加速度・角速度オフセットとして反映できます。
- Switch Pro モード時に、読み取った加速度・角速度サンプルを `GamepadAuxState` 経由で Switch Pro ドライバへ渡します。

Web Configurator の測定機能は `/api/measureGyroOffsets` を使い、設定された I2C バス上の LSM6DS3 を探してオフセット値を返します。

### Switch Pro IMU レポート

`SwitchProDriver` を変更し、ホストから送られる `TOGGLE_IMU` の値に応じて IMU レポート形式を切り替えられるようにしています。

- `TOGGLE_IMU=0x01`: 従来の raw IMU payload を送信します。
- `TOGGLE_IMU=0x02`: quaternion mode2 payload を生成して送信します。
- mode2 ではジャイロの積分から quaternion を更新し、Switch Pro 互換の compressed quaternion 形式に変換します。
- IMU モード変更時や IMU 無効化時は quaternion と timestamp の内部状態をリセットします。

これにより、raw 加速度・角速度を要求する環境と、quaternion mode2 を要求する環境の両方に対応することを狙っています。

### StagSwitchPro ボード設定

`configs/StagSwitchPro` を追加し、Switch Pro 互換ファームウェア向けのデフォルト設定を用意しています。

- デフォルト入力モードは `INPUT_MODE_SWITCH_PRO` です。
- 2 本のアナログスティックを ADC ピンに割り当てています。
- I2C0 と LSM6DS3 gyro アドオンを標準で有効にしています。
- アナログスティックの初期 min/max/neutral/deadzone、反転設定、円形補正を定義しています。

## 使い方

ビルド時は board config として `StagSwitchPro` を指定してください。GP2040-CE のビルド環境、書き込み方法、Web Configurator の基本操作はフォーク元のドキュメントを参照してください。

- Documentation: https://gp2040-ce.info
- Upstream repository: https://github.com/OpenStickCommunity/GP2040-CE

書き込み後は Web Configurator で以下を確認・調整します。

1. Analog アドオンで各スティックのピン、反転、校正値を確認します。
2. 手動校正を使う場合は、ニュートラル取得後にスティックを外周に沿って回して min/max を測定します。
3. Gyro アドオンで I2C アドレスを選択し、必要に応じてオフセット測定を実行します。
4. 設定を保存し、デバイスを再起動して反映します。

## 関連する設定と API

README では概要のみを扱いますが、このフォークで追加・利用している主なインターフェースは次の通りです。

- Web Configurator API:
  - `/api/getJoystickCenter`
  - `/api/getJoystickCenter2`
  - `/api/measureGyroOffsets`
- Analog config:
  - joystick min/max/neutral/deadzone
  - diagonal compensation
  - forced circularity
- Gyro config:
  - gyro address
  - accelerometer offsets
  - gyroscope offsets
- Switch command behavior:
  - `TOGGLE_IMU=0x01` raw IMU
  - `TOGGLE_IMU=0x02` quaternion mode2

## Upstream との差分について

フォーク元の GP2040-CE は、RP2040 向けの汎用マルチプラットフォームゲームパッドファームウェアです。このフォークでは、その基盤の上に Switch Pro コントローラ互換用途の変更を加えています。

実装比較では、ローカル履歴の共通祖先 `e9341063` および upstream main の該当実装を参照しています。upstream 側の analog 実装は中心値をもとに ADC 値を正規化する構成で、Switch Pro ドライバの `TOGGLE_IMU` は IMU の有効・無効を扱うだけでした。このフォークでは、アナログスティックの実測範囲保存、LSM6DS3 gyro アドオン、`TOGGLE_IMU` による raw IMU / quaternion mode2 切り替えを追加しています。

## License

このリポジトリはフォーク元と同じく MIT License です。詳しくは [LICENSE](LICENSE) を参照してください。

## Acknowledgements

このプロジェクトは [OpenStickCommunity/GP2040-CE](https://github.com/OpenStickCommunity/GP2040-CE) をベースにしています。GP2040-CE の開発者、コントリビューター、関連ライブラリの作者に感謝します。
