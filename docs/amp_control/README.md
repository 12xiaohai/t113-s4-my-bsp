# T113 A7 Linux + C906 FreeRTOS 控制框架

本版本先建立可验证且默认安全的 AMP 基线：

- A7 Linux 负责启动、网络、日志、配置和上层应用。
- C906 FreeRTOS 运行 1 kHz 控制任务。
- Linux 与 C906 通过 RPMsg 服务 `t113-control` 通信。
- 支持 `PING`、`GET`、`SET -1000..1000`、`ENABLE`、`DISABLE`、`STOP`。
- 命令中断超过 500 ms 后，C906 自动禁止输出并把目标归零。
- 当前 `output` 是虚拟千分比输出；尚未接管 PWM 寄存器和引脚。

## 构建结果

- 固件：`t113_s4_linux_xiaohai_t113s4_nand_uart0_amp_c906_headless.img`
- 大小：22,670,336 bytes
- SHA-256：`e6de92d26253695b72cfd38a83edc94d6dfd270686632700f28cd6519821a73f`
- C906 `rt_system.bin`：238,784 bytes
- C906 ELF 固件：243,816 bytes
- C906 链接占用：245,824 bytes / 6 MiB（3.91%）
- rootfs 镜像：6,553,600 bytes

`t113_s4_linux_xiaohai_t113s4_nand_uart0_amp_autoboot.img` 和
`t113_s4_linux_xiaohai_t113s4_nand_uart0_amp_late_start.img` 都是中间验证版本，
已被本版本替代，不要烧写。`late_start` 版本虽然能启动 C906，但 C906
FreeRTOS 和 A7 Linux 同时使用 UART0，会导致 Linux 串口控制台看起来卡死。

最终版本关闭了 C906 的 UART 日志、UART CLI、Multi Console 和 RPMsg Console，
只保留 OpenAMP 与项目自己的 `t113-control` RPMsg 服务。Linux 独占 UART0，
C906 作为无串口后台实时核运行。

C906 固件存放在 SquashFS 的
`/lib/firmware/amp_rv0.bin` 中，因此不能依赖内核早期 `auto-boot` 取固件。
最终版本由 `/etc/init.d/c906-control` 在根文件系统挂载完成后启动 C906。

## 烧写后的检查

```sh
dmesg | grep -i -E 'c906|rproc|remoteproc|rpmsg|msgbox|mailbox|c906-control'
ls -l /sys/class/remoteproc/
for r in /sys/class/remoteproc/remoteproc*; do
    echo "== $r =="
    cat "$r/name"
    cat "$r/state"
    cat "$r/firmware"
done
ls -l /dev/rpmsg*
t113-control-test
```

原版本虽然已编入 remoteproc/RPMsg 驱动和 C906 固件，但只注册 remoteproc，
没有在根文件系统就绪后调用 `rproc_boot()`，所以看不到
`/dev/rpmsg_ctrl-*`。最终版本将 `c906-control` 加入
`/etc/init.d/load_script.conf`，启动时等待 remoteproc 注册，然后向对应
`state` 节点写入 `start`。

自测最终应看到：

```text
remote processor c906_rproc is now up
PONG c906
...
STATUS enabled=0 target=0 output=0 ...
```

并且启动 C906 后，Linux UART0 串口控制台仍可正常输入命令。

## 下一阶段：真实 PWM

板级 DTS 当前只明确配置了 PWM3/PB0 与 PWM7/PD22，其中 PWM3 已被
`vdd_cpu` 调压器占用，不能作为执行器 PWM。接入 3 路真实输出前必须依据
原理图确定三个可用 PWM 通道、对应引脚和电平极性，再将这些资源从 Linux
设备树释放给 C906，并启用 FreeRTOS Sunxi PWM 驱动。
