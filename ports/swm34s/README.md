MicroPython port to Synwit SWM34S (Cortex-M33 MCU)
==========================================

&nbsp;
## Pin
### 输出
``` python
import machine
from machine import Pin

led = machine.Pin('PA5', Pin.OUT)
led.low()
led.high()
led.toggle()
led.value(1-led.value())
```

&nbsp;
### 输入
``` python
key = machine.Pin('PA6', Pin.IN, pull=Pin.PULL_UP)
key.value()
```

&nbsp;
### 中断
``` python
led = machine.Pin('PA5', Pin.OUT)
​
key = machine.Pin('PA6', Pin.IN, pull=Pin.PULL_UP, irq=Pin.FALL_EDGE, callback=lambda key: led.toggle())
```

&nbsp;
### API
``` python
Pin(id, dir=Pin.IN, *, af=0, pull=Pin.PULL_NONE, irq=0, callback=None, priority=3)
```
|参数|含义|可取值|
|---|---|----|
|id|引脚名称|PA0--15、PB0--15、PC0--15、PD0--15、PE0--15、PM0--15、PN0--15|
|dir|GPIO方向|IN、OUT、OUT_OD|
|af|引脚功能，0表示GPIO| |
|pull|上下拉控制|PULL_UP、PULL_DOWN、PULL_NONE|
|irq|中断触发边沿|0、RISE_EDGE、FALL_EDGE、BOTH_EDGE、LOW_LEVEL、HIGH_LEVEL|
|callback|中断回调函数| |
|priority|中断优先级|0--7，数值越小优先级越高|


&nbsp;
## Timer
### 定时器
``` python
import machine
from machine import Pin, Timer
​
led = machine.Pin('PA5', Pin.OUT)
​
tmr = machine.Timer(0, 100000, callback=lambda tmr: led.toggle())   # 100000us
tmr.start()
```
定时器模式下单位为us，最大取值为 0xFFFFFF = 16777215us（约16秒）

&nbsp;
### 计数器
``` python​
ctr = machine.Timer(1, 3, mode=Timer.COUNTER, pin='PB0', callback=lambda ctr: led.toggle())
ctr.start()
```
计数器模式下单位为上升沿个数，最大取值为 0xFFFFFF

&nbsp;
**如何查询计数器输入在哪个引脚上？**  
``` python
for name in dir(machine.Pin):
    if 'TIMR1' in name:
        print(name)
```

&nbsp;
### API
``` python
Timer(id, period, *, mode=Timer.TIMER, pin=None, irq=Timer.IRQ_TIMEOUT, callback=None, priority=3)
```
|参数|含义|可取值|
|---|---|----|
|id| |0-4、10-21|
|period|周期| |
|mode|工作模式|TIMER、COUNTER（仅TIMR0-4，TIMR10-21无计数器功能）|
|pin|计数引脚| |
|irq|中断触发源|0、IRQ_TIMEOUT|
|callback|中断回调函数| |
|priority|中断优先级|0--7，数值越小优先级越高|

&nbsp;
``` python
period([int])
value()
```
获取或设置周期值，定时器模式下单位为us，计数器模式下单位为上升沿个数  
获取计数器当前值，定时器模式下单位为us，计数器模式下单位为上升沿个数

&nbsp;
``` python
irq_enable(irq)
irq_disable(irq)
irq_flags(): irq
```
irq取值为 IRQ_TIMEOUT

&nbsp;
## UART
``` python
import machine
from machine import UART
​
ser = machine.UART(2, 115200, txd='PM7', rxd='PB13')
​
ser.write('Hi from MicroPy\n')

while ser.any() < 7: pass
ser.read()
```
注意：UART0用于 MicroPy REPL，用户程序中不要使用

&nbsp;
### 中断接收
``` python
led = machine.Pin('PA5', Pin.OUT)

def on_ser_rx(ser):
    if ser.irq_flags() & ser.IRQ_RXAny:
        while ser.any():
            char = ser.readchar()
            os.dupterm().writechar(char)    # do something on received char

    if ser.irq_flags() & ser.IRQ_TIMEOUT:
        led.toggle()                        # do somethin when no data in specified time

ser = machine.UART(2, 57600, txd='PM7', rxd='PB13', irq=UART.IRQ_RXAny, callback=on_ser_rx)
```
注意1：中断服务程序中GC被锁，不能动态分配内存，因此callback函数中不能使用read和readline，也不能用print  
注意2：readchar底层利用了SysTick延时功能，因此UART1的中断优先级不能高于SysTick，SysTick的中断优先级为3

&nbsp;
### 中断发送
``` python
class tx_helper:
    @classmethod
    def send(cls, ser, msg):
        cls.Inx = 0
        cls.Msg = msg
        ser.irq_enable(ser.IRQ_TXEmpty)   # when want send message, enable irq

    @classmethod
    def on_tx(cls, ser):
        if ser.irq_flags() & ser.IRQ_TXEmpty:
            while not ser.full():
                if cls.Inx < len(cls.Msg):
                    ser.writechar(cls.Msg[cls.Inx])
                    cls.Inx += 1
                else:
                    ser.irq_disable(ser.IRQ_TXEmpty)   # when message sent, disable irq
                    break

ser = machine.UART(2, 115200, txd='PM7', rxd='PB13', irq=UART.IRQ_TXEmpty, callback=lambda ser: tx_helper.on_tx(ser))
tx_helper.send(ser, b'Hi from MicroPy on Synwit SWM34S\n')
```
注意：由于str的索引会导致动态内存分配，因此只能发送bytes

&nbsp;
### API
``` python
UART(id, baudrate, *, bits=8, parity=None, stop=1, rxd=None, txd=None,
                      irq=0, callback=None, priority=4, timeout=10)
```
|参数|含义|可取值|
|---|---|----|
|id| |1、2、3|
|baudrate|串口波特率| |
|bits|数据位宽|5、6、7、8|
|parity|校验位|'None'、'Odd'、'Even'、'One'、'Zero'|
|stop|停止位宽|1、2|
|rxd|UART_RXD引脚| |
|txd|UART_TXD引脚| |
|irq|中断触发源|0、IRQ_RXAny、IRQ_TXEmpty 及其或运算|
|callback|中断回调函数| |
|priority|中断优先级|0--7，数值越小优先级越高|
|timeout|timeout个字符时间未收到新数据触发超时|0--255|

&nbsp;
``` python
baudrate([int])
```
获取或设置波特率

&nbsp;
``` python
read([nbytes])
readinto(buf[, nbytes])
readline()
```
若不指定读取个数，read和readinto读取所有已接收到的内容；readline一直读取直到遇到换行符

&nbsp;
``` python
write(buf)
```
buf为实现buffer协议的对象（如str、bytearray）

&nbsp;
``` python
irq_enable(irq)
irq_disable(irq)
irq_flags(): irq
```
irq取值为 IRQ_RXAny、IRQ_TXEmpty 及其或运算


&nbsp;
## SPI
### 读写闪存
``` python
import machine, time
from machine import Pin, SPI
from array import array

spi = machine.SPI(1, 1000000, mosi='PC6', miso='PC5', sck='PC7')
spi_ss = machine.Pin('PC4', Pin.OUT)
spi_ss.high()

GD25Q21_CMD_READ_CHIPID	  =	0x90
GD25Q21_CMD_READ_DATA	  = 0x03
GD25Q21_CMD_WRITE_PAGE	  = 0x02
GD25Q21_CMD_ERASE_SECTOR  = 0x20
GD25Q21_CMD_WRITE_ENABLE  = 0x06
GD25Q21_CMD_WRITE_DISABLE = 0x04

def flash_identify():
    spi_ss.low()
    spi.write(array('B', [GD25Q21_CMD_READ_CHIPID,  0x00, 0x00, 0x00]))
    memview = spi.read(2)
    spi_ss.high()

    return array('B', memview)

def flash_erase(addr):
    spi_ss.low()
    spi.write(array('B', [GD25Q21_CMD_WRITE_ENABLE]))
    spi_ss.high()

    spi_ss.low()
    spi.write(array('B', [GD25Q21_CMD_ERASE_SECTOR, (addr >> 16) & 0xFF, (addr >>  8) & 0xFF, (addr >>  0) & 0xFF]))
    spi_ss.high()

def flash_write(addr, buf):
    spi_ss.low()
    spi.write(array('B', [GD25Q21_CMD_WRITE_ENABLE]))
    spi_ss.high()

    spi_ss.low()
    spi.write(array('B', [GD25Q21_CMD_WRITE_PAGE,   (addr >> 16) & 0xFF, (addr >>  8) & 0xFF, (addr >>  0) & 0xFF]))
    spi.write(buf)
    spi_ss.high()

def flash_read(addr, cnt):
    spi_ss.low()
    spi.write(array('B', [GD25Q21_CMD_READ_DATA,    (addr >> 16) & 0xFF, (addr >>  8) & 0xFF, (addr >>  0) & 0xFF]))
    memview = spi.read(cnt)
    spi_ss.high()

    return array('B', memview)

flash_identify()
flash_erase(0x1000)
time.sleep(2)
flash_read(0x1000, 32)
flash_write(0x1000, array('B', [i for i in range(32)]))
time.sleep(1)
flash_read(0x1000, 32)
```

&nbsp;
### API
``` python
SPI(id, baudrate, *, polarity=SPI.IDLE_LOW, phase=SPI.EDGE_FIRST, slave=False,
                     bits=8, mosi=None, miso=None, sck=None, ss=None)
```
|参数|含义|可取值|
|---|---|----|
|id| |0、1|
|baudrate|时钟速率| |
|polarity|时钟极性|IDLE_LOW、IDLE_HIGH|
|phase|时钟相位|EDGE_FIRST、EDGE_SECOND|
|slave|是否为从机| |
|bits|数据位宽|取值4--16|
|mosi|SPI_MOSI引脚| |
|miso|SPI_MOSI引脚| |
|sck|SPI_SCK引脚| |
|ss|SPI_SS引脚，从机模式下使用| |

&nbsp;
``` python
baudrate([int])
```
获取或设置波特率

&nbsp;
``` python
write(data): count
```
只写；data可以是一个整数，或实现buffer协议的对象（如str、bytearray、array.array）；当数据位宽小于等于8时使用str和bytearray，数据位宽大于8时使用array.array

&nbsp;
``` python
read(count): int or buf
readinto(buf): count
```
只读；读取一个数据时返回一个整数，读取多个数据时返回一个memoryview

&nbsp;
``` python
write_read(data): int or buf
write_readinto(wbuf, rbuf)
```
同时读写；参数同'write'，返回值同'read'


&nbsp;
## I2C
### 通用读写
``` python
import machine
from machine import I2C
from array import array

i2c = machine.I2C(0, 100000, scl='PC5', sda='PC4')
​i2c.scan()

SLV_ADDR = 0x50   # AT24C02的地址
i2c.writeto(SLV_ADDR, array('B', [0x00, 0x37, 0x55, 0xAA, 0x78]))
time.sleep(1)
i2c.writeto(SLV_ADDR, array('B', [0x00]))   # dummy write, set read address
i2c.readfrom(SLV_ADDR, 4)
```
注意：端口A、C上的引脚有内部上拉，外部可不接上拉电阻；端口B上的引脚必须外接上拉电阻

&nbsp;
### 存储读写
``` python
i2c.mem_writeto(SLV_ADDR, 0x10, array('B', [0x37, 0x55, 0xAA, 0x78]))
time.sleep(1)
i2c.mem_readfrom(SLV_ADDR, 0x10, 4)
```

&nbsp;
### API
``` python
I2C(id, baudrate, *, slave=False, slave_addr=None, scl=None, sda=None)
```
|参数|含义|可取值|
|---|---|----|
|id| |0、1|
|baudrate|时钟速率| |
|slave|是否为从机|True、False|
|slave_addr|从机地址，从机模式下使用| |
|scl|I2C_SCL引脚| |
|sda|I2C_SDA引脚| |

&nbsp;
``` python
baudrate([int])
```
获取或设置波特率

&nbsp;
``` python
scan(start=0x10, end=0x7F): list
```
扫描总线上的外设

&nbsp;
``` python
writeto(addr, data): count
```
data可以是一个整数，或实现buffer协议的对象（如str、bytearray、array.array）

&nbsp;
``` python
readfrom(addr, count): str
readfrom_into(addr, buf): count
```
读取

&nbsp;
``` python
mem_writeto(addr, memaddr, data, memaddr_len=1): count
```
data可以是一个整数，或实现buffer协议的对象（如str、bytearray、array.array）  
memaddr_len是memaddr的长度，单位为byte

&nbsp;
``` python
mem_readfrom(addr, memaddr, count, memaddr_len=1): str
mem_readfrom_into(addr, memaddr, buf, memaddr_len=1): count
```
从指定内存地址读取


&nbsp;
## ADC
### 软件启动
``` python
import machine
from machine import ADC

adc = machine.ADC(0)
​adc.seq_config(0, [0, 1, 2])

adc.start(0)
for i in range(3):
    while not adc.any(): pass
    chn, value = adc.read(0)
    print(f'chn {chn}: {value}')
```
ADC0：CH0-6 on PC6-0, CH7-10 on PA15-12, CH11 on PA10
ADC1：CH0-1 on PD1-0, CH2-6 on PC13-9

&nbsp;
### 定时启动
``` python
adc = machine.ADC(0)
adc.seq_config(0, [0], trigger=ADC.TRIG_TIM2)

tmr = machine.Timer(2, 1000)   # 1000us
tmr.start()

buf = [0] * 500
for i in range(500):
    while not adc.any(4): pass
    chn, buf[i] = adc.read(4)
```

&nbsp;
### API
``` python
ADC(id)
```
|参数|含义|可取值|
|---|---|----|
|id| |0、1|

&nbsp;
``` python
seq_config(seq, chns, *, trigger=ADC.TRIG_SW, sampleTime=1, convertTimes=1)
```
|参数|含义|可取值|
|---|---|----|
|seq |转换序列|0、1、2、3|
|chns|转换通道| 数组或元组指定 |
|trigger|启动触发|TRIG_SW、TRIG_TIMER2-3、TRIG_EXT0-1、TRIG_PWM0-4|
|sampleTime|采样时间|1、2、4、8、16、32、64、128|
|convertTimes|单次启动后的转换次数|1-256|

&nbsp;
``` python
start(seq)
any(seq)
read(seq)
```
软件启动模式下，start()用来启动序列；any()检测指定序列是否有转换结果可读


&nbsp;
## PWM
### 基本使用
``` python
import machine
from machine import PWM
​
pwm = machine.PWM(1, machine.freq()/1000000, 10000, dutyA=2500, pinA='PM3')
pwm.start()
```
时钟分频为 SYS_CLK / 1000000，因此 PwM 计数单位为 1us，此时周期 10000 对应频率 100Hz

&nbsp;
### API
``` python
PWM(id, clkdiv, period, *, mode=PWM.EDGE_ALIGNED, dutyA=2500, dutyB=7500, deadzoneA=0, deadzoneB=0, trigger=0, pinA=None, pinAN=None, pinB=None, pinBN=None)
```
|参数|含义|可取值|
|---|---|----|
|id| |0、1、2、3、4|
|clkdiv|时钟分频|1-1024|
|period|周期时长|1-65535|
|mode  |工作模式|EDGE_ALIGNED、CENTER_ALIGNED|
|dutyA|A通道高电平时长|1-65535|
|dutyB|B通道高电平时长|1-65535|
|deadzoneA|A通道死区时长|0-1023|
|deadzoneB|B通道死区时长|0-1023|
|pinA|A通道输出引脚| |
|pinAN|A通道互补输出引脚| |
|pinB|B通道输出引脚| |
|pinBN|B通道互补输出引脚| |

&nbsp;
``` python
start()
stop()
```
启动、停止PWM

&nbsp;
``` python
period()
duty(ch)
deadzone(ch)
```
获取或设置周期时长、高电平时长、死区时长，ch取值CH_A、CH_B


&nbsp;
## CAN
### 自测
``` python
import machine
from machine import CAN

can = machine.CAN(0, 100000, mode=CAN.SELFTEST, rxd='PN5', txd='PN4')
can.send(0x137, b'\x11\x22\x33\x44\x55\x66\x77\x88')
while not can.any(): pass
can.read()
```
将PN4和PN5短接，执行上面的自测代码

&nbsp;
### 接收（标准帧）
``` python
can = machine.CAN(0, 100000, rxd='PN5', txd='PN4')
can.filter(0, (CAN.FRAME_STD, (0x137, 0x7FE), (0x154, 0x7FF)))
while not can.any(): pass
can.read()
```
当消息满足“ID & mask == check & mask”时被接收，因此上面的设置接收ID等于0x137、0x136和0x154的消息

&nbsp;
### 接收（扩展帧）
``` python
can = machine.CAN(0, 100000, rxd='PN5', txd='PN4')
can.filter(0, (CAN.FRAME_EXT, (0x1137, 0x1FFFFFFC)))
while not can.any(): pass
can.read()
```
接收ID等于0x1137、0x1136、0x1135和0x1134的消息

&nbsp;
### API
``` python
CAN(id, baudrate, *, mode=CAN.NORMAL, rxd=None, txd=None, irq=0, callback=None, priority=4)
```
|参数|含义|可取值|
|---|---|----|
|id| |0、1|
|baudrate|位速率|1000000、800000、500000、400000、250000、200000、125000、100000|
|mode|工作模式|NORMAL、LISTEN、SELFTEST|
|rxd|接收引脚| |
|txd|发送引脚| |
|irq|中断触发源| |
|callback|中断回调函数| |
|priority|中断优先级|0--7，数值越小优先级越高|

&nbsp;
``` python
baudrate([int])
```
获取或设置波特率

&nbsp;
``` python
full()
send(id, data, *, format=CAN.FRAME_STD, remoteReq=False, retry=True)
done()
```
full()返回False表示可以发送，send()用于发送数据，参数data可以是一个整数或buffer  
done()用于发送后查询发送是否完成，返回False表示发送还未完成，返回TX_SUCC表示发送成功，返回TX_FAIL表示发送失败

&nbsp;
``` python
any()
read()
```
any()返回True表示有数据可读，read()用于读出接收到的数据，返回值格式为（format, id, bytes)，如果接收到的是一个远程帧则返回值为（format, id, True)

&nbsp;
``` python
state()
```
查询模块当前状态，可取值及其解释如下：
|状态|含义|
|---|---|
|STAT_EACT|ERROR ACTIVE,  1 error_counter <   96|
|STAT_EWARN|ERROR WARNING, 1 error_counter >=  96|
|STAT_EPASS|ERROR PASSIVE, 1 error_counter >= 127|
|STAT_BUSOFF|BUS OFF,       1 error_counter >= 255|

&nbsp;
``` python
filter(filter_id, [filter])
```
读取或设置接收过滤器，过滤器根据位数不同有下面两种格式（filter_id 取值 0-15）：
- 扩展帧过滤器：(CAN.FRAME_EXT, (check29b, mask29b))
- 标准帧过滤器：(CAN.FRAME_STD, (check11b, mask11b), (check11b, mask11b))


&nbsp;
## RTC
``` python
rtc = machine.RTC()
rtc.datetime([2020, 5, 11, 0, 14, 34, 00])
rtc.datetime()
```
datetime()用于查询或设置当前时间，参数依次为：年、月、日、DayOfWeek、时、秒，设置时参数中DayOfWeek可以是任意值，因为代码会自动计算它的正确值

