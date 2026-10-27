# net_tran_helper

基于 Qt 的网络调试助手，支持 TCP 客户端、TCP 服务端、UDP 客户端和 UDP 服务端。

## 功能

- 十六进制与 ASCII 数据收发
- 定时自动发送
- 多客户端连接和群发
- 保存并恢复界面配置

## 构建

使用 Qt Creator 打开 `nettool.pro`，选择已安装的 Qt 桌面套件进行构建。
项目使用 Qt Core、Gui、Network 和 Widgets 模块。

本仓库不包含本地设备协议示例数据。原始说明暂保留在本地。
