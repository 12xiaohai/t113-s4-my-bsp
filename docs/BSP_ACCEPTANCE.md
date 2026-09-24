# T113-S4 BSP 验收记录

更新日期：2026-09-23

## 已完成

- C906 FreeRTOS 由 Linux remoteproc 自动启动，固件为 `amp_rv0.bin`。
- 修正 RPMsg 初始化顺序：先发布 `sunxi,rpmsg_ctrl`，再注册 `t113-control` 动态服务。
- 删除会阻断 C906 OpenAMP 名称服务的 `CONFIG_DISABLE_ALL_UART_LOG=y`；保留关闭 CLI/multi-console 的精简配置。
- `t113-control-test` 已验证 Linux -> C906 命令、C906 -> Linux 应答和 500 ms 失联归零。
- C906 接管 PWM4/PD5、PWM5/PD6、PWM6/PD7，周期为 200000 ns（5 kHz）。
- 示波器实测三路频率均为 5 kHz，占空比分别为 25%、50%、75%。
- 控制命令支持 `SET channel duty`、`SETALL duty1 duty2 duty3`、`ENABLE`、`STOP`和 `GET`。
- Linux LCD0 已禁用，避免 PD5/PD6/PD7 与 LVDS 引脚复用冲突。
- 已根据原理图将目标无线方案修正为板载 XR829MQ：Wi-Fi 使用 4-bit SDIO，蓝牙使用 UART；DTS、`xr829.ko`、XR829 固件及板端脚本已经完成构建集成。
- 根文件系统已集成 `wpad`、`iw`、`iperf3`、`t113-wifi`和 `bsp-acceptance`。
- 根文件系统已集成 `wireless-regdb`，避免 cfg80211 启动时缺少 `regulatory.db`。
- `t113-wifi` 支持 STA、AP、开机自动连接和 STA 断线重连。
- 旧镜像板端统一验收结果为 `9 PASS / 0 FAIL / 6 WARN`；XR829MQ 新镜像已生成，待烧录后重新生成板端验收基线。

## 当前构建数据

| 项目 | 数值 |
| --- | ---: |
| XR829MQ 烧录镜像 | 24,773,632 B（23.63 MiB） |
| SquashFS `rootfs.img` | 8,650,752 B（8.25 MiB） |
| Linux `zImage` | 4,573,216 B（4.36 MiB） |
| `xr829.ko` | 573,624 B（560.18 KiB） |
| C906 `amp_rv0.bin` | 253,800 B（247.85 KiB） |
| C906 ELF RAM 用量 | 257,080 B / 6 MiB（4.09%） |
| 板端 `MemTotal` | 234,888 KiB |
| 空闲状态 `MemAvailable` | 208,748 KiB |
| Kernel init 完成 | 约 5.55 s |
| C906 remoteproc running | 约 7.80 s |
| ADB USB configured | 约 8.22 s |

与此前 AIC USB Wi-Fi 构建相比，XR829MQ 版本的根文件系统和烧录镜像均增加 1,048,576 B（1 MiB）；`zImage` 基本不变，XR829 驱动以模块方式进入根文件系统。

## 验收命令

```sh
# RPMsg 与失联保护
t113-control-test

# 示波器模式：PD5=25%、PD6=50%、PD7=75%，持续 30 s
t113-control-test scope 30

# 统一验收，包含延迟/丢包和 iperf3 上下行
PING_TARGET=192.168.1.1 IPERF_SERVER=192.168.1.100 bsp-acceptance
```

Wi-Fi 配置：

```sh
cp /etc/t113-wifi.conf.example /etc/t113-wifi.conf
chmod 600 /etc/t113-wifi.conf
vi /etc/t113-wifi.conf
t113-wifi restart
```

## 需实物补齐的证据

- 当前板卡未贴装 XR829MQ；完成芯片、电源/去耦、时钟、天线和外围器件贴装后，再执行 SDIO 枚举、STA/AP、断线恢复和 `iperf3` 实测。
- 烧录新镜像后重新采集冷启动时间、内存和 8～24 h 稳定性数据。

## 镜像

`out/t113_s4_linux_xiaohai_t113s4_nand_xr829_release.img`

镜像 SHA-256：`6be6ae2455b1af30c73640997d234a7b5b2cda18f39fb6f277cf385c9d7f50aa`

C906 固件 SHA-256：`f9304228e8e6a5804c36d8bb021ab35731f630a16d8d1936e61c96b191828f82`
