# monitor-sni-tray

一个独立运行的系统监控托盘程序。程序直接在 session D-Bus 上导出
`org.kde.StatusNotifierItem`，并向 `org.kde.StatusNotifierWatcher` 注册，支持
DDE、KDE Plasma 等提供 SNI 托盘宿主的桌面环境，不再构建或加载 dde-dock `.so` 插件。

托盘 Tooltip 每秒更新 CPU、内存、磁盘、下载速度、CPU 温度/功耗以及 GPU
温度和风扇状态。点击托盘后直接显示硬件信息菜单，不启动终端或 CPU-X。程序
同时导出 `com.canonical.dbusmenu`，菜单中的数据也会每秒刷新。

CPU 封装温度和功耗来自 `turbostat --quiet --show PkgTmp,PkgWatt -i 1`。如果系统
没有安装 `turbostat`，或者当前用户没有读取硬件计数器的权限，对应信息不会显示。
GPU 信息来自 `/sys/class/drm/*/device/hwmon`，磁盘信息来自系统挂载卷，网络速度
来自 `/proc/net/dev`。

## 依赖

Debian/Ubuntu 构建依赖：

```bash
sudo apt install build-essential cmake pkg-config qt6-base-dev libdbus-1-dev
```

运行期可选/功能依赖：

```bash
sudo apt install linux-tools-common libcap2-bin
```

部分发行版将 `turbostat` 放在 `linux-cpupower` 或与当前内核版本对应的
`linux-tools-$(uname -r)` 中。

## 构建和运行

```bash
cmake -S . -B build
cmake --build build
./build/monitor-sni-tray
```

桌面环境必须正在运行 `org.kde.StatusNotifierWatcher`。如果托盘宿主晚于本程序
启动，程序会监听其上线并自动重新注册。

## 安装

```bash
sudo cmake --install build
```

安装内容包括：

- `/usr/local/bin/monitor-sni-tray`
- `/usr/local/share/applications/monitor-sni-tray.desktop`
- `/etc/xdg/autostart/monitor-sni-tray.desktop`

默认会通过 XDG autostart 随桌面会话启动。CMake 默认前缀是 `/usr/local`；打包时
Debhelper 会使用 `/usr`。

## Debian 打包

```bash
dpkg-buildpackage -us -uc -b
sudo apt install ../monitor-dock-plugin_0.3.0-1_$(dpkg --print-architecture).deb
```

Deb 包安装的是独立可执行程序 `/usr/bin/monitor-sni-tray`，不再向
`/usr/lib/dde-dock/plugins` 安装共享库。
