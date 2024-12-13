WS63FLASH(1) -- 海思 WS63 芯片刷写工具

(English version available at file `README.en')

ws63flash 是通过反向海思官方烧录工具 (BurnTool) 实现的海思 WS63 烧写工具。

# 安装

在 Microsoft Windows 上，需要先安装 MSYS2 并在其 MSYS 环境 (MSYS2 MSYS) 下运行：

  # pacman -S autoconf automake gcc make

在 GNU/Linux 上，执行以下命令：

  # apt install autoconf automake gcc make

构建并安装：

  $ autoreconf -fi # 如果使用 Release 源代码，可跳过。
  $ ./configure
  $ make
  $ sudo make install

# 使用方法

在兼容 POSIX 的系统 (GNU/Linux, BSD 等) 上刷写 fwpkg：

  $ ws63flash --flash /dev/ttyUSB0 /path/to/fwpkg

在 MichaelSoft Binbows 上需要使用 COM 口刷写：

  # ws63flash --flash COM3 \path\to\fwpkg

以更快的波特率 (推荐, 921600) 刷写：

  # ws63flash -b921600 --flash PORT /path/to/fwpkg

选择性刷写，只刷写 `ws63-liteos-app.bin' 和刷写所需的必要文件：

  # ws63flash --flash PORT /path/to/fwpkg ws63-liteos-app.bin

二进制刷写，直接刷写未打包到 fwpkg 的二进制文件：

  # ws63flash --write PORT root_loaderboot_sign.bin \
            ws63-liteos-app-sign.bin@0x230000 \
	    flashboot_sign.bin@0x220000

擦除板上内存：

  # ws63flash --erase PORT /path/to/fwpkg

更多信息请参见 `ws63flash --help' 以及 ws63flash(1) 手册页。

# 参考资源

  * https://gitee.com/HiSpark/fbb_ws63/tree/master/src/bootloader

    Contains useful headers for return values, error values and data structures.

  * https://gitee.com/HiSpark/fbb_ws63/blob/master/tools/README.md

     Contains instruction for setting up HiSpark Studio. The `libburn.dll' in
     HiSpark Studio contains important routines and data structures for flashing.

  * https://gitee.com/hihope_iot/near-link/raw/master/firmware/WS63/ws63-liteos-app_all.fwpkg

    A reference firmware for fwpkg parsing and flashing. But this can't be
    flashed on Hisilicon WS63E MCU. And, `src/fwpkg.h' also documented it.
