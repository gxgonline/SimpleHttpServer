# Simple Http Server

## 原始网址：https://github.com/InsZVA/SimpleHttpServer
> 改进说明（20200713）Email：19784420@qq.com
* 增加了64位编译选项
* 所有数字997替换为宏：WSA_IO_PENDING。
* 修改了错误fp = CreateFileA 返回值 fp == NUUL判断改为fp == INVALID_HANDLE_VALUE
* 修改CreateFileMapping参数传入错误，应该是高32位，低32位。使用GetFileSizeEx支持大于4G的文件。
* 解决CPU占用太大的问题。
* 创建服务模式启动。

## 说明

IOCP模型作为高性能IO模型，线程本地内存池，文件缓存，分析模块。

## 进度

1. IOCP模型:

[x] AcceptEx接收数据

[x] DisconnectEx复用Sokcet

[x] 消息循环 

2. 内存池

[x] 16Bytes * [1-32] 的小块内存分配

[] 512Bytes * [1 - 8]的中等内存分配

[x] 整页内存分配

3. HTTP请求解析

[x] HTTP请求状态机

[x] 长请求的处理

[] Keep-Alive处理

[] SOCKET哈希到线程处理？

4. HTTP响应书写

[] 与HTTP请求关联

5. HTTP文件传输

[] TransmitFile

6. 统计模块

[x] 基础