# ESPHome 组件：HLK-LD2410D

第三方 ESPHome 外部组件，用于海凌科 **HLK-LD2410D** 24GHz 静止人体存在传感器。
协议参考官方手册 V1.02 编写，用法风格参考官方 `ld2410` / `ld2410b/c` 组件。

## 与 LD2410 / LD2410B 的主要差异

| 项目 | LD2410 / B | LD2410D（本组件） |
| --- | --- | --- |
| 波特率 | 256000 | **115200** |
| 距离门数量 | 9 | **16** |
| 普通模式输出 | 二进制帧 | **ASCII 文本**（`OFF` / `distance: XX`） |
| 工程帧头/尾 | `F4F3F2F1` / `F8F7F6F5` | 相同 |
| 命令帧头/尾 | `FDFCFBFA` / `04030201` | 相同 |
| 能量/门限单位 | 原始值 | **dB**（`dB = 10·log10(raw)`） |
| 最大距离参数 | 按距离门 | **0.1m 单位**（0.7~10m） |

因协议差异较大，本组件独立实现，不能直接复用官方 `ld2410`。

## 安装

推荐直接从 GitHub 仓库引用（无需本地拷贝）：

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/2107596808/esphome-ld2410d
    components: [ld2410d]
```

也可以把 `components/` 目录放到配置同级，使用本地引用：

```yaml
external_components:
  - source:
      type: local
      path: components
    components: [ld2410d]
```

## 快速开始

见 [`example.yaml`](example.yaml)。最小配置：

```yaml
uart:
  id: uart_bus
  tx_pin: GPIO17
  rx_pin: GPIO16
  baud_rate: 115200

ld2410d:
  id: radar
  uart_id: uart_bus

binary_sensor:
  - platform: ld2410d
    ld2410d_id: radar
    has_target:
      name: "Presence"
```

## 支持的实体

- **binary_sensor**：`has_target`（有人）、`moving_target`（运动）、`still_target`（静止）
- **sensor**：`detection_distance`（cm）；`g0..g15_move_energy`、`g0..g15_still_energy`（dB，仅工程模式）
- **text_sensor**：`version`（固件版本）、`sn`（序列号）
- **number**：`max_distance`（0.7~10m）、`timeout`（0~65535s）、
  `g0..g15_move_threshold`、`g0..g15_still_threshold`（dB）、
  `trigger_coefficient` / `hold_coefficient` / `micro_coefficient`（1.0~20.0，用于自动门限生成）
- **button**：`query_params`（读取参数）、`generate_threshold`（自动门限生成）、`auto_gain`（上电自动增益）
- **switch**：`engineering_mode`（工程模式，开启后输出各距离门能量值）

## 使用说明

- **普通模式**：只上报有人/无人与目标距离。
- **工程模式**：额外上报 16 个距离门的运动能量与微动/静止能量（dB）。通过 `engineering_mode` 开关切换。
- **自动门限生成**：先设置 `trigger/hold/micro_coefficient`，保持检测区域空旷，再点击 `generate_threshold`。
  生成后建议点击 `query_params` 刷新门限值。
- **参数写入**：修改任何 number 后组件会自动进入配置模式、写入并掉电保存（`0x00FD`），再退出配置模式。

## 协议实现要点

- 门限 dB ↔ 原始值：`raw = 10^(dB/10)`，`dB = 10·log10(raw)`（手册 5.2.7）。
- 最大距离参数按 `距离(m) × 10` 编码（手册 5.2.7 示例）。
- 工程帧负载：`结果(1) + 距离(2) + 16×运动能量(4) + 16×微动能量(4)`（手册 5.5.2）。
- 所有多字节字段为小端。

## 免责声明

本组件为第三方实现，与深圳市海凌科电子有限公司无关，按“原样”提供。
