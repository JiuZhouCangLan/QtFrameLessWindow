## Qt无边框窗口 for Windows

***原项目: https://github.com/Bringer-of-Light/Qt-Nice-Frameless-Window***

由上述仓库修改而来, 仅保留了Windows平台部分实现, 修正了多屏情况下, 窗口存在的各种问题.

### 特点:

- 保留了窗口阴影
- 保留所有原生拖动, 最大化, 最小化效果
- 跨屏拖动不会导致错位或撕裂

### 使用:  
Qt6.3.0以下版本  
1. 获得Qt源代码
2. 对Qt源代码应用Patch文件夹下的.diff补丁 补丁来自[[QTBUG-84466]](https://bugreports.qt.io/browse/QTBUG-84466), 感谢@Viktor Arvidsson  
3. 编译Qt源代码  
4. 编译本项目  
5. 使用修改后的`platforms/qwindows.dll`及`platforms/qdirect2d.dll`以及所有qml相关dll(及其debug版本)替换掉Qt原本的dll  

Qt6.3.0以上版本  
直接使用