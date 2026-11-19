# 风速遥测监测页面

该页面通过浏览器的 Web Serial 接口读取地面接收板输出的串口数据，不需要修改现有 `wind_rx` 固件，也不需要安装第三方 Python 库。

## 第一次使用

1. 给地面板烧录 `wind_rx`，接好 410 至 493 MHz 天线，再用 USB 数据线连接电脑。
2. 关闭 PlatformIO 串口监视器和其他占用该 COM 口的软件。
3. 双击 `start_dashboard.bat`。网页服务会在后台运行，不需要保留黑色窗口。
4. 页面自动打开后点击“连接地面接收端”。
5. 在浏览器弹出的串口列表中选择地面接收板。
6. 打开空中发送端电源，收到第一包 `WV1` 数据后页面自动更新。

实时演示可给空中端烧录 `wind_tx_realtime`，页面约每 0.5 秒更新；一般联调使用 `wind_tx_test`，约每 2 秒更新；联调通过后重新烧录正式 `wind_tx`，页面约每 60 秒更新。

浏览器会记住已经授权的串口。以后再次启动页面时，如果只有一个已授权串口，程序会尝试自动连接。

需要完全停止本地网页服务时，双击 `stop_dashboard.bat`。仅关闭浏览器页面不会停止后台服务；再次双击启动脚本会直接重新打开已经运行的页面。

## 模拟测试

没有连接实物时，点击“模拟演示”可以每两秒生成一包测试数据。也可以运行：

```powershell
python serve_dashboard.py --demo
```

## 串口数据格式

页面只解析以 `WV1,` 开头的行，其他启动信息会自动忽略。

```text
WV1,node,seq,avg,min,max,sd,n,raw,batt_mv,status,rssi_dbm,snr_db
```

示例：

```text
WV1,KITE01,128,6.420,6.100,6.830,0.210,10,2475,4360,0,-62.0,9.5
```

## 常见问题

- 无法打开串口：关闭 PlatformIO 的 Monitor，再重新连接。
- 浏览器显示 `ERR_CONNECTION_REFUSED`：重新双击 `start_dashboard.bat`，等待页面自动打开；如果仍失败，查看 `dashboard-server-error.log`。
- 浏览器没有串口功能：使用 `start_dashboard.bat` 自动打开的 Microsoft Edge 或 Google Chrome。
- 页面已连接但没有数据：确认地面板烧录的是 `wind_rx`，串口波特率为 115200，并确认空中端已经发包。
- OLED 有数据但页面没有：确认选择的是地面接收板对应的 COM 口。
- 正式模式等待时间较长：`wind_tx` 约每 60 秒发送一包，`wind_tx_test` 约每 2 秒发送一包，`wind_tx_realtime` 约每 0.5 秒发送一包。

## 数据保存

页面最多在内存中保存 1000 条记录。点击“导出 CSV”可保存当前会话数据。刷新或关闭页面前请先导出需要保留的数据。
