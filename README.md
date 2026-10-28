# XIAO 电容触摸传感器

用 Seeed XIAO ESP32-S3(或 C3)做一个 DIY 电容触摸 / 接近传感器,并在浏览器里实时看曲线。

## 接线

```
XIAO D2 ──[ 1 MΩ 电阻 ]── 导电物体(铝箔 / 金属片) ── XIAO D3
```

按板子丝印上的 D2、D3 接。1 MΩ 适合直接触摸,4.7 到 10 MΩ 可以做几厘米的接近感应。

## 烧录固件

用 Arduino IDE 打开 `sketch_xiao_30.ino`,开发板选 **XIAO_ESP32S3**(C3 板子选 XIAO_ESP32C3),上传即可。
固件以 115200 波特率输出 `原始值,平滑值`,在串口监视器发 `r` 可重置基线。

## 看曲线:方式一,纯网页(推荐)

在线版本:**https://yolaaandaw-dotcom.github.io/xiao-touch-sensor/**

用 Chrome / Edge 打开上面的网址(或者本地双击 `index.html`),点"连接串口"选择板子即可。
不需要安装任何东西。Safari、Firefox 和手机浏览器不支持 Web Serial。

## 看曲线:方式二,Python 服务

```bash
pip install pyserial
python3 touch_monitor.py
```

然后浏览器打开 http://localhost:8765 。适合不能用 Chrome 的场合。

两种方式都不能和 Arduino IDE 的串口监视器同时使用,串口只能被一个程序占用。

## 文件

| 文件 | 用途 |
|---|---|
| `sketch_xiao_30.ino` | 板子固件 |
| `index.html` | 浏览器直连串口的监视界面 |
| `touch_monitor.py` + `touch_monitor.html` | Python 串口转网页的监视界面 |

## 许可

固件改编自 CapacitiveSensorR4(Takanori Fujiwara 和 S. Sandra Bae),其源自 Paul Badger 的 CapacitiveSensor 库,许可为 CC BY-NC-SA 4.0。
