# M5stackGPSLogger

m5stackで作るGPSロガー（GPX) & ラップタイマー

## Hardware

* M5Stack Gray（MPU6886 + BMM150）
  * https://www.switch-science.com/catalog/3648/
* M5Stack用GPSモジュール V2（NEO-M8N）
  * https://www.switch-science.com/catalog/3861/
* （M5Stack用電池モジュール）
  * https://www.switch-science.com/catalog/3653/
* micro SD

## Software

* Arduino 1.8.13
* M5Stack 0.3.1
* TinyGPS++ 1.0.2b
  * http://arduiniana.org/libraries/tinygpsplus/
* u-center 20.10
  * https://www.u-blox.com/en/product/u-center?lang=ja

## GPS configuration

あらかじめ以下のURLなどを参考にu center でrate(10Hz)、boud rate(115200)などを変更する
sample -> m5stack -> module -> gpsraw を起動しておき、u-centerを起動し、設定値を変更する。
Sendボタンを押した後は必ずReceiver→Action→Save Configも押すこと。

参考URL
* http://www.denshi.e.kaiyodai.ac.jp/gnss_tutor/pdf/st_190310_1.pdf
* https://www.u-blox.com/sites/default/files/u-center_Userguide_%28UBX-13005250%29.pdf
* https://akizukidenshi.com/catalog/faq/goodsfaq.aspx?goods=M-12905
* https://www.aitoya.com/contents/cont001-001.html
* https://twitter.com/pass810/status/1186430550325653504
