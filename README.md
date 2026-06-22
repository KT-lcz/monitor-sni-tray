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
