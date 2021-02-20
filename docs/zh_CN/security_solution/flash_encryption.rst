Flash 加密
****************

概述
~~~~~~~~

-  使能 flash encryption 后，使用物理手段（如串口）从 SPI flash
   中读取的数据都是经过加密的，大部分数据无法恢复出真实数据。
-  flash encryption 使用 256-bit AES key 加密 flash 数据，key
   保存在芯片的 efuse 中，生成之后变成软件读写保护。
-  用户烧写 flash 时烧写的是数据明文，第一次 boot 时，软件 bootloader
   会对 flash 中的数据在原处加密。
-  一般使用情况下一共有4次机会通过串口烧写 flash ，通过 OTA 更新 flash数据没有次数限制。开发阶段可以在 menuconfig中设置无烧写次数限制，但不要在产品中这么做。

使用步骤
~~~~~~~~

1. make menuconfig 中选择 "Security features"->"Enable flash encryption on boot"
2. 按通常操作编译出 bootloader, partition table 和 app image 并烧写到 flash 中
3. 第一次 boot 时 flash 中被指定加密的数据被加密（大的 partition加密过程可能需要花费超过1分钟） ，之后就可以正常使用被加密的flash数据。

加密过程（第一次 boot 时进行）
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1. bootloader 读取到 efuse 中的 FLASH\_CRYPT\_CNT 为0，于是利用硬件随机数生成器产生加密用的 key ，此 key 被保存在 efuse 中，对于软件是读写保护的。
2. bootloader 对所有需要被加密的 partition 在 flash 中原处加密
3. 默认情况下 efuse 中的 DISABLE\_DL\_ENCRYPT, DISABLE\_DL\_DECRYPT 和 DISABLE\_DL\_CACHE 会被烧写为1，这样 UART bootloader 时就不能读取到解密后的 flash 数据
4. efuse 中的 FLASH\_CRYPT\_CONFIG 被烧写成 0xf，此标志用于决定加密 key 的多少位被用于计算每一个 flash 块（32字节）对应的秘钥，设置为 0xf 时使用所有256位
5. efuse 中的 FLASH\_CRYPT\_CNT 被烧写成 0x01，此标志用于 flash 烧写次数限制以及加密控制，详见“FLASH\_CRYPT\_CNT”一节
6. bootloader 将自己重启，从加密的 flash 执行软件 bootloader

串口重烧 flash （3次重烧机会）
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-  串口重烧 flash 过程

   1. make menuconfig 中选择 "Security features"->"Enable flash encryption on boot"
   2. 编译工程，将所有之前加密的 images （包括 bootloader）烧写到 flash 中。
   3. 在 esp-idf 的 components/esptool\_py/esptool 路径下使用命令 ``espefuse.py burn\_efuse FLASH\_CRYPT\_CNT`` 烧写 efuse 中的 FLASH\_CRYPT\_CNT
   4. 重启设备，bootloader 根据 FLASH\_CRYPT\_CNT 的值重新加密 flash 数据。

-  若用户确定不再需要通过串口重烧 flash，可以在 esp-idf 的 ``components/esptool\_py/esptool`` 路径下使用命令 ``espefuse.py --port PORT write\_protect\_efuse FLASH\_CRYPT\_CNT 将 FLASH\_CRYPT\_CNT``
   设置为读写保护（注意此步骤必须在 bootloader 已经完成对 flash 加密后进行）

FLASH\_CRYPT\_CNT
~~~~~~~~~~~~~~~~~

-  FLASH\_CRYPT\_CNT 是 flash 加密方案中非常重要的控制标志，它是 8-bit 的值，它的值一方面决定 flash 中的值是否马上需要加密，另一方面控制 flash 烧写次数限制。
-  当 FLASH\_CRYPT\_CNT 有（0,2,4,6,8）位被烧写为1时，bootloader 会对 flash 中的内容进行加密。
-  当 FLASH\_CRYPT\_CNT 有（1,3,5,7）位被烧写为1时，bootloader 知道 flash 的内容已经过加密，直接读取 flash 中的数据解密后使用。
-  FLASH\_CRYPT\_CNT 的变化过程：

   1.  没有使能 flash 加密时，永远是0
   2.  使能了 flash 加密，在第一次 boot 时 bootloader 发现它的值是
       0x00，于是知道 flash 中的数据还未加密，利用硬件随机数生成器产生
       key，然后加密 flash，最后将它的最低位置1（取值为0x01）
   3.  后续 boot 时，bootloader 发现它的值是 0x01，知道 flash
       中的数据已加密，可以解密后直接使用
   4.  用户需要串口重烧 flash ，于是使用命令行手动烧写
       FLASH\_CRYPT\_CNT，此时2个 bit 被置为 1（取值为0x03）
   5.  重启设备，bootloader 发现 FLASH\_CRYPT\_CNT 的值是 0x03（2 bit
       1），于是重新加密 flash 数据，加密完成后 bootloader 将
       FLASH\_CRYPT\_CNT 烧写为0x07（3 bit 1），flash 加密正常使用
   6.  用户需要串口重烧 flash ，于是使用命令行手动烧写
       FLASH\_CRYPT\_CNT，此时4个 bit 被置为 1（取值为0x0f）
   7.  重启设备，bootloader 发现 FLASH\_CRYPT\_CNT 的值是 0x0f（4 bit
       1），于是重新加密 flash 数据，加密完成后 bootloader 将
       FLASH\_CRYPT\_CNT 烧写为0x1f（5 bit 1），flash 加密正常使用
   8.  用户需要串口重烧 flash ，于是使用命令行手动烧写
       FLASH\_CRYPT\_CNT，此时6个 bit 被置为 1（取值为0x3f）
   9.  重启设备，bootloader 发现 FLASH\_CRYPT\_CNT 的值是 0x4f（6 bit
       1），于是重新加密 flash 数据，加密完成后 bootloader 将
       FLASH\_CRYPT\_CNT 烧写为0x7f（7 bit 1），flash 加密正常使用
   10. 注意！此时不能再使用命令行烧写 FLASH\_CRYPT\_CNT，bootloader 读到
       FLASH\_CRYPT\_CNT 为 0xff（8 bit 1）时，会停止后续的 boot。

被加密的数据
~~~~~~~~~~~~

-  Bootloader
-  Secure boot bootloader digest（若 Secure Boot 被使能， flash
   中会多出这一项， 具体查看“Secure Boot”中“执行过程”的步骤3）
-  Partition table
-  Partition table 中指向的所有 Type 域标记为“app”的部分
-  Partition table 中指向的所有 Flags
   域标记为“encrypted”的部分（用于非易失性存储（NVS）部分的 flash
   在任何情况下都不会被加密）

哪些方式读到解密后的数据（真实数据）
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-  通过内存管理单元的 flash 缓存读取的 flash
   数据都是经过解密后的数据，包括：

   -  flash 中的可执行应用程序代码
   -  存储在 flash 中的只读数据
   -  任何通过 ``API esp\_spi\_flash\_mmap()`` 读取的数据
   -  由 ROM bootloader 读取的软件 bootloader image 数据

-  如果调用 API ``esp\_partition\_read()`` 读取被加密区域的数据，则读取的
   flash 数据是经过解密后的数据

哪些方式读到不解密的数据（无法使用的脏数据）
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-  通过 API esp\_spi\_flash\_read() 读取的数据
-  ROM 中的函数 SPIRead() 读取的数据

软件写入加密数据
~~~~~~~~~~~~~~~~

-  调用 API ``esp\_partition\_write()`` 时，只有写到被加密的 partition
   的数据才会被加密
-  函数 ``esp\_spi\_flash\_write()`` 根据参数 ``write\_encrypted`` 是否被设为
   true 决定是否对数据加密
-  ROM 函数 ``esp\_rom\_spiflash\_write\_encrypted()`` 将加密后的数据写入
   flash 中，而 SPIWrite() 将不加密的数据写入到 flash 中