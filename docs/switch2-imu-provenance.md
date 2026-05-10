# Switch 2 IMU mode2 由来メモ

このフォークでは、Switch Pro `TOGGLE_IMU=0x02` / mode2 quaternion packing を MissionControl 由来の実装として扱います。

## 参照元

- Repository: https://github.com/ndeadly/MissionControl
- Files:
  - `mc_mitm/source/controllers/switch_motion_packing.hpp`
  - `mc_mitm/source/controllers/switch_motion_packing.cpp`
- Upstream license: `GPL-2.0-only`
- Upstream copyright: Copyright (c) 2020-2026 ndeadly

このリポジトリ側の対応箇所は次の通りです。

- `headers/drivers/switchpro/SwitchProDriver.h`
- `src/drivers/switchpro/SwitchProDriver.cpp`

このフォーク全体は `GPL-2.0-only` として配布します。元の GP2040-CE 由来コードや第三者ライブラリに含まれる MIT / BSD / Apache などの表示は、元著作権・元ライセンスの通知として保持します。