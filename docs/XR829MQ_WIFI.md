# XR829MQ 板载 Wi-Fi 适配说明

更新时间：2026-09-24

## 原理图结论

- U9 为 XR829MQ Wi-Fi/蓝牙组合芯片。
- Wi-Fi 主机接口为 4-bit SDIO：`WL_SDIO_CLK`、`WL_SDIO_CMD`、`WL_SDIO_D0~D3`。
- Wi-Fi 控制信号为 `WL_REG_ON` 和 `WL_WAKE_AP`。
- 蓝牙主机接口为四线 UART：`BT_UART_TX/RX/CTS/RTS`。
- 蓝牙控制信号为 `BT_RST_N`、`BT_WAKE_AP` 和 `AP_WAKE_BT`。
- 无线参考时钟使用图中的 `CLK_FANOUT0` 网络；`DCXO_REFCLK` 支路标注为 NC。
- J9 为外接 IPEX 天线座。

## SDK 选型

Tina 5.0 SDK 已提供以下组件：

- 内核驱动：`drivers/net/wireless/xr829/xr829.ko`
- OpenWrt 内核包：`kmod-net-xr829`
- 固件包：`xr829-firmware`
- 主要固件：`fw_xr829.bin`、`sdd_xr829.bin`、`boot_xr829.bin`

当前选择普通 `kmod-net-xr829`，未选择 `kmod-net-xr829-40M`。最终仍应根据整机时钟原理图或厂家设计说明确认参考时钟频率。

## 当前设备树配置

参考文件来自 V853/sun8iw21p1，节点组织和 XR829 工作参数可以参考，但其 `PH15`、`PG7` 和 `PG6/fanout0` 不能直接用于 T113-S4。当前配置采用 Tina SDK 内 T113-S4 官方板型已有的管脚组合：

- SDIO1：PG0～PG5，4-bit，总线最高 50 MHz
- `WL_REG_ON`：PE3，高电平有效
- `WL_WAKE_AP`：PG10，高电平有效
- 32 kHz 时钟：PG11，`clk_fanout1` / `CLK_FANOUT1_OUT`
- XR829 VDDIO 固定为 3.3 V，禁用 SD 信号电压切换
- `sunxi-rfkill` 已启用，负责 XR829 的上电/复位时序

上述 PE3/PG10/PG11 是 T113-S4 SDK 板型配置，不是从 V853 参考文件照搬。烧录后仍需测量 `WL_REG_ON` 和 32 kHz 时钟并检查 SDIO 枚举；若定制底板修改过连线，再按完整 SoC 端原理图修正。

蓝牙目前保持禁用，待 Wi-Fi 验证通过后再启用 UART1、`BT_RST_N`、`BT_WAKE_AP` 和 `AP_WAKE_BT`，避免同时引入两组变量。

## 实机验收顺序

1. 上电后确认 `WL_REG_ON` 和 3.3 V 电源时序。
2. 检查 `dmesg | grep -i -E 'mmc|sdio|xr829|xradio|firmware'`，确认 SDIO 设备枚举。
3. 检查 `xr829.ko`、`fw_xr829.bin` 和 `sdd_xr829.bin` 已进入根文件系统。
4. 确认出现 `wlan0`，完成 STA 扫描、关联、DHCP 和断线恢复。
5. 完成 AP 模式及两块 T113 板之间的双向 `iperf3`、时延、丢包和 8～24 小时稳定性测试。

## 2026-09-24 构建结果

- DTS 编译通过，反编译确认 `sdc1` 为 50 MHz/4-bit/固定 3.3 V，`rfkill` 状态为 `okay`。
- `xr829.ko` 构建并安装成功，大小为 573,624 B。
- `boot_xr829.bin`、`fw_xr829.bin` 和 `sdd_xr829.bin` 已进入最终根文件系统。
- `t113-wifi` 已改为加载 `xr829`，统一验收脚本已改为检查 XR829 SDIO、模块和固件。
- 最终镜像：`out/t113_s4_linux_xiaohai_t113s4_nand_xr829_release.img`
- 镜像大小：24,773,632 B（23.63 MiB）
- SHA-256：`6be6ae2455b1af30c73640997d234a7b5b2cda18f39fb6f277cf385c9d7f50aa`

## 当前硬件状态

当前测试板没有焊接 XR829MQ，启动后 `/sys/class/mmc_host/mmc1` 存在，但 `/sys/bus/sdio/devices` 为空，系统不会自动加载 `xr829.ko`，也不会创建 `wlan0`。这符合未贴装 SDIO 从设备时的预期行为，不能据此判定 DTS 或驱动异常。后续贴装芯片及其电源、去耦、32 kHz 时钟、天线匹配和控制电阻后，再进行电气与联网验收。
