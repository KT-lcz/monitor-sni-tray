# monitor-dock-plugin

一个 DDE Dock 插件，用于在任务栏查看当前 CPU 使用率、内存占用、CPU 封装温度和封装功耗。

CPU 封装温度和功耗来自 `turbostat --quiet --show PkgTmp,PkgWatt -i 1`。如果系统没有安装 `turbostat`，或者当前环境没有读取相关硬件计数器的权限，该展示项会保持为空。

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
