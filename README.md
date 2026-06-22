# monitor-dock-plugin

一个 DDE Dock 插件，用于在任务栏查看当前 CPU 使用率和内存占用。

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
