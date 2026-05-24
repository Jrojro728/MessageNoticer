

# MessageNoticer

A c++ short messages system

<!-- PROJECT SHIELDS -->

[![Contributors][contributors-shield]][contributors-url]
[![Forks][forks-shield]][forks-url]
[![Stargazers][stars-shield]][stars-url]
[![Issues][issues-shield]][issues-url]
[![MIT License][license-shield]][license-url]

<!-- PROJECT LOGO -->
<br />

<p align="center">
  <a href="https://github.com/jrojro728/MessageNoticer">
    <img src="MessageNoticerServer/icon1.ico" alt="Logo" width="128" height="128">
  </a>

  <h3 align="center">MessageNoticer</h3>
  <p align="center">
    一个简单的c++短消息系统实现
    <br />
    <!-- <a href="https://github.com/jrojro728/MessageNoticer"><strong>探索本项目的文档 »</strong></a> -->
    <a href="https://github.com/jrojro728/MessageNoticer">查看Demo</a>
    ·
    <a href="https://github.com/jrojro728/MessageNoticer/issues">报告Bug</a>
    ·
    <a href="https://github.com/jrojro728/MessageNoticer/issues">提出新特性</a>
  </p>

</p>

 
## 目录

- [上手指南](#上手指南)
  - [开发前的配置要求](#开发前的配置要求)
  - [安装步骤](#安装步骤)
- [文件目录说明](#文件目录说明)
- [开发的架构](#开发的架构)
- [部署](#部署)
- [使用到的框架](#使用到的框架)
- [如何参与开源项目](#如何参与开源项目)
- [作者](#作者)
- [鸣谢](#鸣谢)

### 上手指南

使用git clone下载本仓库并使用你喜欢的构建方式编译

(可选cmake 和 msbuild)


###### 开发前的配置要求

1. 可用的vcpkg
2. 一个可以使用c++20标准的编译器
3. CMake版本 >= 3.16

###### **安装步骤**

1. git clone

```sh
git clone https://github.com/jrojro728/MessageNoticer.git
```
2. 编译
3. enjoy

### 文件目录说明

```
filetree 
├── LICENSE.txt
├── README.md
├── vcpkg.json ----------------------------Vcpkg依赖项定义
├── /MessageNoticerServer/  ---------------服务器实现
├── /MessageNoticerServer/  ---------------客户端实现
├── MessageNoticer.sln --------------------vs解决方案
———— CMakeLists.txt
```





### 开发的架构 

经典CS架构

### 部署

暂无，等待实现

### 使用到的框架

- [log4cplus](https://github.com/log4cplus/log4cplus)
- [boost.uuid](https://www.boost.org/doc/libs/latest/libs/uuid/doc/html/uuid.html)
- [jsoncpp](https://github.com/open-source-parsers/jsoncpp)
- [argh](https://github.com/adishavit/argh)

<!-- ### 贡献者

请阅读**CONTRIBUTING.md** 查阅为该项目做出贡献的开发者。 -->

#### 如何参与开源项目

贡献使开源社区成为一个学习、激励和创造的绝佳场所。你所作的任何贡献都是**非常感谢**的。

1. Fork the Project
2. Create your Feature Branch
3. Commit your Changes
4. Push to the Branch
5. Open a Pull Request



### 版本控制

您可以在repository参看当前可用版本。

### 作者

Jrojro728

### 版权说明

该项目签署了MIT 授权许可，详情请参阅 [LICENSE.txt](https://github.com/shaojintian/Best_README_template/blob/master/LICENSE.txt)

### 鸣谢

- [Img Shields](https://shields.io)

<!-- links -->
[your-project-path]:jrojro728/MessageNoticer
[contributors-shield]: https://img.shields.io/github/contributors/jrojro728/MessageNoticer.svg?style=flat-square
[contributors-url]: https://github.com/jrojro728/MessageNoticer/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/jrojro728/MessageNoticer.svg?style=flat-square
[forks-url]: https://github.com/shaojintian/Best_README_template/network/members
[stars-shield]: https://img.shields.io/github/stars/jrojro728/MessageNoticer.svg?style=flat-square
[stars-url]: https://github.com/jrojro728/MessageNoticer/stargazers
[issues-shield]: https://img.shields.io/github/issues/jrojro728/MessageNoticer.svg?style=flat-square
[issues-url]: https://img.shields.io/github/issues/jrojro728/MessageNoticer.svg
[license-shield]: https://img.shields.io/github/license/jrojro728/MessageNoticer.svg?style=flat-square
[license-url]: https://github.com/jrojro728/MessageNoticer/blob/master/LICENSE.txt




