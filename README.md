# XIAO 电容档位传感器

用 Seeed XIAO ESP32-S3(或 C3)做一个单路电容传感器,触摸不同位置得到不同高度的稳定值,
按稳定值落在哪一"档"来区分四个方向,用于给机械臂发方向指令。浏览器页面负责标定、识别和记录。

## 工作原理

- 固件每 100 次充放电循环报一个计数,触摸让计数升高。板子只报原始值,基线由网页跟踪。
- 网页在触摸开始后等信号稳住约 0.2 秒,取平台值,和标定好的四个档位中心比,落在哪个判定区间就是哪一档。
- 判定区间是相邻档位中心的中点;平台值离最近的档位太远,判为"不确定",不触发。
- 基线会在空闲且平稳时自动更新;慢漂移会被缓慢跟随;信号突变后长时间平稳会被当作新基线。

## 标定流程

1. Chrome 打开 `index.html`,点"连接串口"。
2. 手不碰,看基线稳定后点"重新取基线"。
3. 按住第 1 档的位置不松手,点那一行的"记录",等 2 秒。四档依次做完。
4. 看"可分性检查":相邻两档的间距至少要是波动的 3 倍才算分得开。
5. 点"导出 JSON"保存标定文件。四个方向可以在下拉框里改。

## 接线

```
XIAO D2 ──[ 1 MΩ 电阻 ]── 导电物体(铝箔 / 金属片) ── XIAO D3
```

按板子丝印上的 D2、D3 接。1 MΩ 适合直接触摸,4.7 到 10 MΩ 可以做几厘米的接近感应。

## 烧录固件

用 Arduino IDE 打开 `sketch_xiao_30.ino`,开发板选 **XIAO_ESP32S3**(C3 板子选 XIAO_ESP32C3),上传即可。
固件以 115200 波特率输出 `原始值,平滑值`,接线断开时输出 `#` 开头的提示行;发 `r` 可重置平滑滤波。

## 页面

在线版本:**https://yolaaandaw-dotcom.github.io/xiao-touch-sensor/**

用 Chrome / Edge 打开上面的网址(或者本地双击 `index.html`),点"连接串口"选择板子即可。
不需要安装任何东西。Safari、Firefox 和手机浏览器不支持 Web Serial。

## 旧的 Python 监视器(可选)

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
| `index.html` | 标定 + 档位识别页面(浏览器直连串口) |
| `touch_monitor.py` + `touch_monitor.html` | Python 串口转网页的监视界面 |

## 许可

固件改编自 CapacitiveSensorR4(Takanori Fujiwara 和 S. Sandra Bae),其源自 Paul Badger 的 CapacitiveSensor 库,许可为 CC BY-NC-SA 4.0。
