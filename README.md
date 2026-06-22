# monitor-dock-plugin

一个 DDE Dock 插件，用于在任务栏查看当前 CPU 使用率、内存占用、所有本地硬盘使用情况、CPU 封装温度、封装功耗、CPU 信息和网络下载速度。

CPU 封装温度和功耗来自 `turbostat --quiet --show PkgTmp,PkgWatt -i 1`。如果系统没有安装 `turbostat`，或者当前环境没有读取相关硬件计数器的权限，该展示项会保持为空。

CPU 名称来自 `/proc/cpuinfo`。大小核数量和性能核平均频率来自 `/sys/devices/system/cpu/cpufreq/policy*`，按最大频率档位的主要断层区分性能核和能效核；无明显断层时按同构 CPU 处理。硬盘使用情况来自 Qt 的挂载卷信息，按 `/sys/class/block` 归并到 `lsblk` 中的硬件块设备名，例如 `nvme0n1`、`nvme1n1`。下载速度来自 `/proc/net/dev` 中非 loopback 网卡的接收字节差值。

左键点击 Dock 插件会启动 `cpu-x`，Debian 包依赖中包含 `cpu-x`。

## 构建

```bash
cmake -S . -B build
cmake --build build
```

## 安装

```bash
sudo cmake --install build
```

安装后重启 DDE Dock 或重新登录，使插件被加载。

## Debian 打包

```bash
dpkg-buildpackage -us -uc -b
sudo apt install ../monitor-dock-plugin_0.1.0-1_$(dpkg --print-architecture).deb
```

deb 包会把插件安装到 `/usr/lib/dde-dock/plugins/libmonitor-dock-plugin.so`。
