## Script Readme

**对于生产环境为 CentOS 7.9.2009 (x86)的交叉编译**
使用Dockerfile.centos7 生成sysroot (含GLIBC 和 libstdc++)，然后指定参数给`CxxInterpreter`生成对方平台的可执行文件。
针对低版本Linux编译的程序，可以在高版本运行，如以当前sysroot编译的程序，可以在RHEL 10上直接运行。
当前Dockerfile包含了lldb-11和gcc-9用于简易调试，实际编译没用到那么多。（如系统采用Docker而非Podman，请自行修改sh脚本）
注意：由于系统过老旧，VSCode的 ms-vscode-remote.remote-containers 插件无法在其容器内运行。

1. 运行 `sudo ./make_c79dbg_image.sh` 生成image (建议以root运行 以方便之后挂载到文件夹)
2. 运行 `sudo ./mount_c7dbg.sh <dir>`挂载当前镜像 (例如 sudo ./mount_c7dbg.sh c7root)
3. 以挂载至`/mnt/c7root`为例，修改`cgn_setup.cgn.cc`额外指定两个配置即可交叉编译
```cpp
cfg["cxx_sysroot"] = "/mnt/c7root";
cfg["cxx_gcctoolchain"] = "/mnt/c7root/opt/rh/devtoolset-9/root/usr";
```

**对于生产环境为 Freebsd**
同上，解压freebsd系统至`/mnt/freebsd15-root`后，修改`cfg["cxx_sysroot"] = "/mnt/freebsd15-root";`即可。

**推荐: 制作安全的隔离和开发容器**
以HostOS 为 RHEL 10.1为例，修改Dockerfile.rhel10 内容以满足实际要求（包括基础系统、预装包）
* 运行`r10.sh build` 重新生成image并存储于当前用户podman环境(使用 `podman images` 查看)
* 运行`r10.sh create` 创建一个vscode可以直接attach的container
* 在VSCode运行container插件，选择刚刚制作的container，右键选 `Attach Visual Studio Code`即可进入之
* 运行`r10.sh bash` 可以打开container内部的bash

用户应自行修改r10.sh和Dockerfile.rhel10以适应当前操作系统 修改后依次执行 build->create 然后用vscode重新打开即可

**(VS官方文档)制作安全的隔离和开发容器**
假设当前HostOS 为 RHEL 10.1 修改Dockerfile.rhel10 内容以满足实际要求（包括基础系统、预装包）
* 运行`r10.sh build` 重新生成image并存储于当前用户podman环境(使用 `podman images` 查看)
* 参考 devcontainer.js.example 编写`<dir>/.devcontainer/devcontainer.js` 确保文件夹映射和宿主机一致
* 使用VSCode先打开`<dir>`文件夹(to let VSCode known which devcontainer.js loaded)，在命令输入框（最上方搜索框）运行`> Dev Containers: Open workspace in container.` 然后选择想进入container的.code-workspace文件
* VSCode会自动创建新的容器 并映射文件夹，用户可以像使用本地一样
* 运行`r10.sh bash` 打开临时本地shell
