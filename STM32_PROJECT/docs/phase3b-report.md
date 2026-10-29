# 阶段3B：Stop 1 与 GPIO 唤醒交付及实机验收

日期：2026-10-29。基线：`0622cc4`（3A）。本次只做3B，未烧录、未Push；Stop2、Standby、RTC定时唤醒、USART/RS485 Stop接收唤醒、LIMIT_EN关断均未实现。功能和电流均须实机验收后才能判定通过。

## 1. 已核对的依据与适用范围

- 本工程 `Drivers/STM32U0xx_HAL_Driver/Src/stm32u0xx_hal.c` 的版本为1.0.0；同库 `stm32u0xx_hal_pwr_ex.c:411` 实现 `HAL_PWREx_EnterSTOP1Mode(PWR_STOPENTRY_WFI)`，设置LPMS=001，设置/退出时清除SLEEPDEEP；注释明确GPIO、SRAM和寄存器保持。
- [ST RM0503 Rev4](https://www.st.com/resource/en/reference_manual/rm0503-stm32u0-series-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)：4.3.7及表27定义Stop1与EXTI/NVIC唤醒，5.4.3的STOPWUCK选择HSI16，31.3.10要求旁路影子时双读日历确认一致。Stop入口有pending时不应强行清除事件以入睡。
- [ST DS14581 Rev2](https://www.st.com/resource/en/datasheet/stm32u031c8.pdf)：STM32U031支持Stop1、RTC及GPIO功能，3.6.5和6.3.6给出芯片条件。手册芯片电流/唤醒数值不作为本板验收保证。
- [ST ES0603 Rev2](https://www.st.com/resource/en/errata_sheet/es0603-stm32u031x468-device-errata-stmicroelectronics.pdf)：核查了LPTIM Stop中断、RTC调试alarm和USART噪声等限制。本次不使用LPTIM/RTC alarm；现有ADC AutoWait保留。仍需记录实物REV_ID及测试批次，不能用主机测试证明所有硅片条件。
- [TI DRV8874 SLVSF66A](https://www.ti.com/lit/ds/symlink/drv8874.pdf)：6.5、7.4.2用于下文nSLEEP实测判据。

ST的大型RM在线阅读器有大小限制，已通过ST官方索引核对上述具体章节；未声称完整下载/逐页审阅整个RM。没有依据其他STM32系列实现Stop流程。

## 2. 配置、构建与烧录

`Core/Inc/power_config.h` 的 `POWER_LOW_POWER_MODE`：

|值|工作方式|空闲串口|后台电池采样|
|---|---|---|---|
|0，默认|调试，不自动休眠|保留原ASCII查询|约250ms周期|
|1|保留3A普通Sleep|普通IRQ可唤醒，原ASCII查询|约250ms定时醒来采样|
|2|3B Stop1，GPIO唤醒|不保证收帧，不支持UART唤醒|Stop期间暂停，GPIO醒来后重新采样|

CubeIDE：打开现有工程，确认包含新文件`power_rtc.c`；头文件设2，或预处理定义`POWER_LOW_POWER_MODE=2`，两处不能冲突。Refresh、Project/Clean、Build，再下载此次构建的ELF。确认启动配置没有仍指向旧的Debug ELF。回到0即可保留原ASCII调试，回到1用于3A电流对比。不要让CubeMX重复初始化TIM2。

CMake：`-DVALVE_STOP1=ON`选择2；它优先于`VALVE_LOW_POWER`。Stop OFF、LOW_POWER ON为1；两者OFF为0。构建目录：`build/phase3b-debug`、`build/phase3b-sleep`、`build/phase3b-stop`，均生成`STM32_PROJECT.elf`。

本次三配置均干净构建43步，无编译/链接警告：

|配置|FLASH /64KB|RAM /12KB|
|---|---:|---:|
|0 Debug|39312B|2880B|
|1 Sleep|41076B|2912B|
|2 Stop1|43324B|2920B|

RAM并非最坏栈水位。符号检查：0使用原弱HAL_GetTick，1/2使用power_port强覆盖；只有2的最终ELF包含Stop1与RTC读时入口。CubeIDE GUI编译/下载尚未在本机执行，实际验收仍需核对其构建日志。

## 3. 关键路径与安全边界

1. 主循环 `Valve_Service → RS485_ProcessInput → Power_Idle` 不变。Stop权限仍由原`power_manager.c`统一决定：电机准备/运行、安全关阀、换向/消抖/恢复授权、ADC忙、UART缓冲/半行和事件未处理时均不入睡。新增首次上电/每次Stop后必须完成一次ADC尝试，避免刚上电尚未采样就一直Stop。
2. `PowerManager_TrySleep`保持原PRIMASK、事件代数、GPIO签名和输出复核；随后`power_port.c:stop1`再检查全部EXTI pending、NVIC pending、UART TC/接收活动、ADC转换/停止/校准/禁用活动、PWM和驱动输出。
3. 进入前暂停TIM2与SysTick，关闭USART1 UE/UESM。不会修改PH、nSLEEP、VREF_EN、LIMIT_EN或输入上下拉。最后复核GPIO及pending后调用U0 HAL Stop1 API；没有任何清除GPIO EXTI pending的代码。
4. PRIMASK跨越最后检查、WFI、恢复。晚到的已使能GPIO中断使WFI结束；事件保持pending，恢复完才允许ISR执行。PA3上升沿沿用原快停/粘滞事件，即使唤醒前已恢复低，短脉冲也可通过已锁存事件请求安全关阀。不能保证捕获任意窄脉冲。
5. 恢复时首先核对HSIRDY、HSI16 SYSCLK、AHB/APB÷1、TIM1=PCLK、ADC=SYSCLK、USART1=HSI、Flash延迟1。STOPWUCK预先设HSI，因此使用与运行前相同的硬件唤醒时钟；无需重启PLL/LSE或重新调用整套SystemClock_Config。
6. 恢复检查只有有限次数寄存器读取，不使用冻结的HAL时间等待。RTC经过时间补到HAL时基，启动TIM2后再更新SystemCoreClock、重建SysTick、恢复UART、最后解锁IRQ。下轮先重新读GPIO并运行原安全状态机，再处理ADC/UART。
7. 恢复后时钟/RTC/时基异常进入原Error_Handler，驱动安全关闭，保留诊断，不继续在未知时钟下运行电机。此时无法保证完成关阀，需复位/检查硬件；不会软件制造一次关到位记录。硬件失效或MCU无法执行代码时的关阀能力不由此软件保证。

受保护且与3A提交一致：`valve_control.c/h`、`valve_port.c/h`、`valve_config.h`、`gpio.c`、`adc.c`、`tim.c`、`usart.c`、`rtc.c`、`battery_adc_port.c/h`、`app_status.c/h`、中断源文件和`.ioc`。没有恢复固定30秒超时、没有新增过流阈值。

## 4. 时间、ADC、UART行为

RTC沿用原LSE和127/255分频，不设alarm或wake timer；Stop2/Standby等均未调用。仅在配置2开启RTC BYPSHAD，最多4次双读SSR/TR/DR，校验BCD日期/闰年。原日历值不重设；2000—2099日历周期，倒退/非法值拒绝使用。

运行时仍是TIM2的1ms计数；Stop前冻结TIM2，用RTC的1/256秒增量补偿整个停表区间，并保留不足1ms的余数。恢复后TIM2继续计时，HAL_GetTick保持32位毫秒自然回绕；`uwTick`本身不代表Stop经过时间。RTC量化约3.906ms，切换边界还有寄存器读取的微小开销；不是精密计时承诺，必须实测重复唤醒误差。活跃的50/100ms资格和2s换向等待本来就禁止Stop，不会在Stop中跨过这些尚未处理的任务。不得在调试器冻结RTC/TIM2时验收时间。

现有HAL_Delay(100)仍在PM启用前；原串口1/2ms等待与逐字节TX期间不调用Idle。恢复前不调用依赖HAL tick的阻塞RCC/RTC API；恢复后HAL等待使用已运行的TIM2。没有新增HAL_UART_Receive轮询。

Stop期间没有周期采样，也没有主动欠压监控。GPIO醒来先执行安全判断，再将旧电池值标记NOT_READY，保留最后有效mV与updated_ms，清空跨Stop的平均窗口并立即安排新采样。未完成/失败时不得将旧值报告为新值。ADC时钟与配置保留，沿用HAL_ADC_Start/Stop；实际醒来后的精度、参考建立及多次采样仍需板测。

USART1在Stop期间被禁用，不能靠PING唤醒。醒来恢复同一CR1、BRR及HSI时钟，并丢弃输入直到下一LF（包括临近WFI取消的路径），避免命令尾部误执行。若需醒后查询，应在电机正在运行、程序保持活动期间先发送一个单独换行，再发完整PING/STATUS。空闲时可能已再次Stop；日常调试应选0或1。只等待重同步LF不阻止下一次Stop；已有真正半行命令仍按原策略阻止休眠。

PA15/PB3没有新增EXTI：Stop中单独改变到位信号不会主动唤醒，只在下次GPIO唤醒后重读。运动期间始终活动，到位停止保持原轮询方式。

## 5. 调试观察项

原有 `power_sleep_entries / power_sleep_aborts / power_last_blockers` 仍可用；配置2中sleep_entries包括Stop API返回次数，不能代表实际驻留时间。

|变量|说明|
|---|---|
|power_stop_entries|Stop API返回且恢复成功的次数，正在Stop时不会继续增长|
|power_stop_last_ms|最近一次停表区间的RTC补偿毫秒数|
|power_stop_wake_exti|退出时RPR1与FPR1的或值；bit3对应PA3，bit0对应PB0，只作线索|
|power_stop_clock_ok|初始配置或唤醒恢复检查完成为1；恢复检查中/异常为0|
|power_stop_faults|锁存诊断位：1时钟配置不支持；2 RTC配置/读数异常；4唤醒时钟异常；8软件时基恢复异常|

`power_last_blockers`沿用3A位定义：0x1调试、0x2电机、0x4安全关未完成、0x8阀门任务、0x10 ADC、0x20 UART、0x80事件/临近休眠取消、0x100输出不安全、0x200端口初始化失败、0x400上下文不允许。端口内部取消可能只显示0x80，结合stop_faults和硬件寄存器定位。

可在冷上电后、完成一次GPIO唤醒时检查 `PWR->SR1` 的STOPF[11:9]是否为101，以辅助确认硬件曾进入Stop1；标志有历史保持特性，不能证明每次成功或驻留时长。以实际电流波形为最终依据。调试连接可能改变/阻止Stop或失联，不能拿“暂停/断点后电流”作为正式结果。

## 6. 可执行主机测试

运行 `tests/run_phase1.py`（或既有test_digital_control入口），使用arm-none-eabi-gcc和Unicorn Cortex-M0+模拟器，临时目录输出，不烧录。最终275项全部通过，-Wall/-Wextra/-Werror：PM Sleep20、Debug1、Stop20；Sleep端口16、Stop端口20、RTC19；电池23、ADC端口7、快照5、RX14、阀门26、驱动14、应用Debug28/Sleep30/Stop32。完整日志保存在工作区`tmp/phase3b-final-tests.txt`。

测试覆盖三个配置的统一许可、服务期间/最后检查/WFI时到达的GPIO事件、安全关期间绝不睡、UART半行、Stop期间UE关闭、ADC硬件忙、时钟拒绝及恢复失败、长时间RTC补偿、毫秒余数累积、运行TIM2与停表RTC合计、闰年/跨年及非法日期、双读期间寄存器变化的重试/耗尽、旧ADC失效与首次采样、串口尾帧丢弃再同步。真实端口HAL/寄存器由桩模拟，不能验证硅片实际Stop/WFI、电气边沿、时钟启动延迟、噪声或功耗。

独立只读复核未发现实质问题，独立运行273项通过；随后新增ADC禁用忙和混合计时测试，由主代理执行最终回归。

## 7. 实机验收顺序（全部待验收）

1. 配置0先回归PING/STATUS、ADC、正常开关、断线关完与恢复防误开，确认旧行为。配置1用于复测此前约0.99mA基准，同接线/同供电/同阀位。
2. 电机动力线暂时断开，保持限位/控制接线。配置2重新Clean/Build/下载，L4低、L5低、独立关到位L2高/L1低，运行超过1秒使首次ADC完成。确认PWM无脉冲、PA5低、PA2低；测nSLEEP。然后脱离ST-LINK并独立供电、冷上电复测。
3. 保持输入10秒、1分钟、10分钟：应长期Stop，不能每250ms自行醒来采样。此时PING不回复符合本阶段定义。用GPIO唤醒后观察last_ms应近似实际停留时间；entries在Stop驻留中不增长正常。读取诊断宜在唤醒后仍活动时进行，不要依靠调试器强制暂停来证明唤醒。
4. 接回电机并使用可承受启动电流的测量链路。关到位Stop→L5高，应消抖后开；开到位停止并再次Stop；L5低应醒来关阀。运行/准备/换向期间entries不增长，任一方向到位必须停，之后才允许再次Stop。
5. 开到位且稳定Stop→L4高，必须醒来禁止开阀并安全关阀。关阀途中把L4恢复低仍须关完。L5持续高不得误开；先低至少1秒再高才允许开。断线关到位前不允许Stop；保护故障使关不完时仍不得睡、不得报告已关。nFAULT注入仅使用已确认安全夹具，不做堵转测试。
6. 分别从Stop触发PB0升/降沿及PA3断线，至少100次累计唤醒（可先电机断开、按真实位置独立模拟限位，避免无意义机械磨损），记录漏唤醒/误开/恢复失败数必须为0。包含上电第一次、开到位/关到位、ADC刚完成、GPIO快速相邻变化及断线短脉冲。切换设备的触点抖动也要记录，不能声称任意窄脉冲必被捕获。
7. Stop保持数秒后醒来，验证50ms消抖、100ms断线恢复、低授权和2s换向等待仍正常；计时不能冻结。记录RTC与PC间隔差，跨分钟/午夜及重复100次后误差。检查旧BAT_VALID先为0且updated不伪更新，随后新序号/时间戳和mV更新；重复验证醒后的ADC正常。
8. 唤醒后的活动期间核对SystemCoreClock/HCLK/PCLK均16MHz，TIM1 PSC=0、ARR=799，实际PWM频率与3A一致；ADC配置/读数一致。发送单独LF，再PING/STATUS，串口115200 8N1无乱码且不干扰运动；重复唤醒后再测。静止Stop里发命令不作为通信故障。
9. 构造双限位/原有已锁存故障时验证仍禁止危险动作；新输入在临近Stop时不得丢失。时钟/RTC失效的主机测试已覆盖，现场不通过危险短路或停振来验证。出现时基错误、误开或漏安全关，停止电机测试，保留诊断，切回0。

## 8. PA9 / DRV8874实测

先断电，根据原理图和导通档确认PA9网络到驱动nSLEEP的连接；优先选择同网测试点/串阻焊盘。实物若为TI PWP16封装，nSLEEP为3脚，仍需确认方向。不要在通电密脚间滑动表笔；需要时断电焊细测试线再上电。

万用表黑笔板地、红笔nSLEEP网络：稳定静止时应接近0V。VM约7.2V时，TI规定逻辑低上限0.8V，但正常软件低输出应明显接近0V；不要把0.79V当作理想设计值。示波器10×探头观察PA9/nSLEEP、PA5和PWM，进入Stop/整个驻留期间不能出现意外高脉冲；合法开/关时仍沿用原VREF/唤醒/PWM顺序。TI规定拉低并经过tSLEEP后关闭桥和内部电路；不要仅用nFAULT高来证明休眠。

结合nSLEEP持续低、无驱动脉冲和VM支路电流验证驱动器状态；芯片睡眠电流不能由整板电流直接推算。输出高阻时OUT1/OUT2对地电压可能漂移，不能要求两个输出都测得0V。测电机两端波形使用差分探头或两路对板地测量再相减，不能把接地夹夹到桥输出上。

## 9. 最坏响应与电流测量

示波器CH1接MCU侧PA3，CH2接nSLEEP，其他通道接VREF_EN/PWM；PA3升沿触发。先等待开到位、最后一次运动已过去超过2秒，再在稳定Stop中断线，测PA3到安全关方向唤醒/PWM的延迟；这是包含软件恢复、原有驱动启动等待的应用安全响应，不等于芯片裸唤醒时间。另测刚到位后立即断线的端到端最大延迟，标出原换向保护等待；不为缩短波形而改掉它。若需单独测WFI返回/ISR入口，需后续经确认选用独立测试点加测量标记，本次未占用LIMIT3或修改控制引脚作标记。对100次以上、不同ADC边界和供电电压采集，保存最大值及波形；不把平均值当最坏值。

正式测流脱离ST-LINK，关闭串口定时发送，避免USB适配器额外供电路径；保持同一电源、阀位和限位输入，分别测配置0/1/2。上电动作结束、稳定Stop后清零统计，至少记录60秒平均/最低/峰值及输入电压，明确电机动力线、RS485是否连接。Stop不应再有3A约4Hz采样唤醒；若仍有周期性活动，先查调试保持、pending、UART半行、外部噪声及是否确实下载配置2。

记录整板和可测的3.3V/DRV VM支路；PA2保持低，限位电阻、收发器和稳压器静态耗电仍在，不能承诺整板达到MCU微安典型值。不要用微安量程测电机启动，不做堵转。原3A的nSLEEP实测、最坏响应和剩余静态耗电来源继续保留待验收，不因代码测试通过而关闭这些问题。

## 10. 交付文件与停止点

新增：`Core/Inc/power_rtc.h`、`Core/Src/power_rtc.c`、`tests/test_power_rtc.c`、`tests/test_power_stop.c`、`docs/phase3b-plan.md`、`docs/phase3b-report.md`、`docs/phase3b-changes.json`。

修改：`Core/Inc/power_config.h`、`power_port.h`、`battery_monitor.h`、`rs485_rx.h`；`Core/Src/power_port.c`、`main.c`、`battery_monitor.c`、`rs485_rx.c`；`CMakeLists.txt`、`STM32CubeIDE/.project`；`tests/mocks/main.h`、`run_phase1.py`、`test_battery.c`、`test_rs485_rx.c`、`test_valve_app.c`。

完整文件及SHA256见`phase3b-changes.json`。基线及用户未跟踪构建目录、硬件PDF均保留。完成本次3B后停止，等待代码审查和实机验收；不自动开发USART唤醒、Stop2或下一阶段。
