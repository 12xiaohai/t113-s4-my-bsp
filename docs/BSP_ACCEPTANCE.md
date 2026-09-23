# T113-S4 BSP 验收记录

更新日期：2026-09-23

## 已完成

- C906 FreeRTOS 由 Linux remoteproc 自动启动，固件为 `amp_rv0.bin`。
- 修正 RPMsg 初始化顺序：先发布 `sunxi,rpmsg_ctrl`，再注册 `t113-control` 动态服务。
- 删除会阻断 C906 OpenAMP 名称服务的 `CONFIG_DISABLE_ALL_UART_LOG=y`；保留关闭 CLI/multi-console 的精简配置。
- `t113-control-test` 已验证 Linux -> C906 命令、C906 -> Linux 应答和 500 ms 失联归零。
- C906 接管 PWM4/PD5、PWM5/PD6、PWM6/PD7，周期为 200000 ns（5 kHz）。
- 控制命令支持 `SET channel duty`、`SETALL duty1 duty2 duty3`、`ENABLE`、`STOP`和 `GET`。
- Linux LCD0 已禁用，避免 PD5/PD6/PD7 与 LVDS 引脚复用冲突。
- 选定 AIC8800D80 USB Wi-Fi，已编译 `aic8800_bsp.ko` 和 `aic8800_fdrv.ko`，并将 USB 固件打包到 `/lib/firmware/aic8800d80`。
- 根文件系统已集成 `wpad`、`iw`、`iperf3`、`t113-wifi`和 `bsp-acceptance`。
- 根文件系统已集成 `wireless-regdb`，避免 cfg80211 启动时缺少 `regulatory.db`。
- `t113-wifi` 支持 STA、AP、开机自动连接和 STA 断线重连。
- 板端统一验收结果为 `9 PASS / 0 FAIL / 6 WARN`；WARN 均为待实物或冷启动补证项目。

## 当前构建数据

| 项目 | 数值 |
| --- | ---: |
| 量产烧录镜像 | 23,725,056 B（22.63 MiB） |
| SquashFS `rootfs.img` | 7,602,176 B（7.25 MiB） |
| Linux `zImage` | 4,573,336 B（4.36 MiB） |
| C906 `amp_rv0.bin` | 253,800 B（247.85 KiB） |
| C906 ELF RAM 用量 | 257,080 B / 6 MiB（4.09%） |
| 板端 `MemTotal` | 234,888 KiB |
| 空闲状态 `MemAvailable` | 208,748 KiB |
| Kernel init 完成 | 约 5.55 s |
| C906 remoteproc running | 约 7.80 s |
| ADB USB configured | 约 8.22 s |

新根文件系统比当前板上约 6.3 MiB 的版本增加约 1 MiB，主要来自 AIC8800D80 固件、内核模块和 `wpad/iw`。

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

- 示波器测量 PD5、PD6、PD7：频率、占空比、幅值和波形截图。
- 插入 AIC8800D80 USB 网卡后，执行 STA/AP、断线恢复和 `iperf3` 实测。
- 烧录新镜像后重新采集冷启动时间、内存和 8～24 h 稳定性数据。

## 镜像

`out/t113_s4_linux_xiaohai_t113s4_nand_amp_pwm_wifi_release.img`

镜像 SHA-256：`23e0a983ee37dca301897ea5f0f990d626f9efdd6a3dd1a1e80f1c5be37b3da6`

C906 固件 SHA-256：`f9304228e8e6a5804c36d8bb021ab35731f630a16d8d1936e61c96b191828f82`
