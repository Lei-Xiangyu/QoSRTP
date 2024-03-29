# QoSRTP
## 简介
带有qos功能的rtp-rtcp传输库。
## 功能
* 基本的RTP和RTCP协议功能：
  * rtp以及rtcp各种包的格式；
  * rtcp传输的调度；
* RTX重传功能：通过nack通知丢失包实现基于rtx的重传，以提高数据传输的可靠性和容错性。
* FEC功能，支持ulp有区分的对不同的包进行不同程度和分组方法的保护；
## 平台
目前仅支持windows64 vs2022编译环境。
