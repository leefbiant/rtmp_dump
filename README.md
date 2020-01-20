# rtmp_dump
recv rtmp dump h264 / aac / opus / mp4

接收RTMP流 dump文件
支持dump h264裸流，AAC裸流，AAC转opus封装ogg, 音视频合并成MP4文件。

SPS、PPS解析参考了其他项目

1 重采样放到编码函数之中去
2 增加opus解码和AAC编码
