# 阶段3A：普通Sleep管理与GPIO唤醒

日期2026-10-28。只实施Sleep；Stop1/Stop2/Standby、RTC周期唤醒、USART低功耗接收唤醒、LIMIT_EN关断均未实施。正式485协议不变，电机不限时运行的既有决定不变。没有新增过流/欠压动作或RTOS。未烧录，所有电气/电流结果待实机验收。

## 基线与本地提交

开始时Git仍是未提交master、全工程未跟踪。修改前1270个源码/配置/测试/文档文件和SHA256保存在`../../tmp/phase3a-baseline-20261028/`，原99项场景通过。本次已创建本地基线提交`d21c5cd`，保存此前2A实机使用的源码；3A另作独立功能提交，没有Push。权限提升后的Git身份与目录所有者不同，采用每条命令仅针对`D:/ChatGPT/机阀驱动`的safe.directory参数，没有更改全局配置或文件所有权。

完整文件及哈希见phase3a-changes.json。新增产品：Core/Inc/power_config.h、power_manager.h、power_port.h；Core/Src/power_manager.c、power_port.c。新测试：tests/test_power_manager.c、test_power_port.c。新增文档：本文件、phase3a-plan.md、phase3a-changes.json。

修改：Core/Src/main.c（初始化/空闲钩子/已处理GPIO代数）；Core/Src/stm32u0xx_it.c与Core/Inc/stm32u0xx_it.h（GPIO双边沿通知、TIM2比较IRQ）；Core/Src/valve_control.c与对应头（只读休眠许可、缓存两个原始输入，不改变原状态转换）；Core/Src/valve_port.c与头（只读粘滞事件查询）；Core/Src/battery_monitor.c与头（只读下次工作预算）；Core/Src/rs485_rx.c与头（只读缓冲忙查询）；CMakeLists.txt、STM32CubeIDE/.project（模式和两个新源文件）；tests/mocks/main.h、tests/run_phase1.py、test_valve_app.c、test_battery.c、test_rs485_rx.c、test_valve_control.c（真实模块许可与集成回归）。ADC换算/滤波、PWM配置、普通GPIO/EXTI配置、USART配置、RTC初始化不变。

## 模式选择

默认`POWER_LOW_POWER_MODE=0`，完整保留ASCII调试，不执行自动Sleep，也不占用TIM2或替换HAL时基。它独立于DEBUG和优化等级，不能以CubeIDE配置名称判断休眠是否启用。

CubeIDE：在Core/Inc/power_config.h将默认值改为1，或者在编译器预处理宏中定义`POWER_LOW_POWER_MODE=1`，刷新、Clean、Build、下载即可测试普通Sleep；回到0后重新构建恢复原调试模式。根工程自动发现Core；子工程的显式链接已补齐。不要在CubeMX中再初始化/占用TIM2。

CMake：`-DVALVE_LOW_POWER=OFF`为调试、`ON`为低功耗，默认OFF；CMake编译宏覆盖头文件默认值。已分别在build/phase3a-debug、build/phase3a-low生成ELF，可直接按现有流程选择对应文件下载。二者均是Debug编译优化配置，区别是明确的电源模式宏。

## 休眠许可

主循环先安全服务，再处理UART，最后Power_Idle。只有以下条件全部满足才尝试Sleep：

- 电机不在准备、唤醒、运行阶段，输出占空比0，nSLEEP/VREF均低。
- 没有安全关阀请求；即使保护故障已使驱动停止，只要SAFE_PENDING=1仍禁止休眠，可能持续保持CPU活动，这是安全优先的取舍。
- 没有手动待执行动作、数字控制待到位动作或换向/冷却后待启动动作。
- LIMIT5初次资格、50ms消抖、断线低电平100ms恢复、恢复后低电平50ms重新授权均已完成，或正稳定等待下一次外部变化。未授权且L5高可睡；L5下降唤醒后会继续完成低电平授权。
- ADC未转换/清理，且下次ADC工作截止时间尚未到达。
- UART没有队列、错误待处理、半行命令或未结束的坏帧；没有发送方向保持/硬件接收活动。半行若永远不补LF，会一直阻止休眠，不擅自丢弃命令。
- 没有尚未被主循环安全采样处理的GPIO事件或其他待处理中断。

Valve_CanSleep仅观察内部状态；原Valve_Step各控制分支条件、PH方向、PWM参数、到位停止、换向间隔、保护及断线防误开逻辑原样保留。新增两个缓存字节用于只读许可；不会通过休眠清故障、解除闭锁或撤销动作。

## WFI竞态与唤醒顺序

PA3、PB0继续使用原双边沿EXTI。PA3上升沿首先调用原快速开向关断，再记录GPIO事件；下降沿与PB0双边沿只记录事件。中断不做串口、延时、换向或完整状态处理。

每次Valve_Service在读GPIO前获取事件代数，完成安全判断及ADC服务后仅把该代数标记已处理。期间或随后到达的事件会造成代数不匹配。Sleep前PRIMASK短临界区再次核对：代数、真实PA3/PB0/PA15/PB3、电机输出、驱动粘滞断线事件、EXTI升降沿pending、NVIC pending、UART硬件/缓冲活动。任何不符直接退出。

保持PRIMASK跨过最终检查和`HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON,PWR_SLEEPENTRY_WFI)`，利用Cortex-M“已使能IRQ成为pending可完成WFI”的机制避免中断已处理后又睡过去。信号在最后检查之后到达，会使WFI立即完成；SysTick恢复和定时比较清理后才恢复原PRIMASK，GPIO ISR随后执行。主循环下一步重新读取实际GPIO并先做安全服务，再处理UART。原始PA3持续高也由轮询重新识别，不依赖单一事件位。

短脉冲能否被EXTI锁存及最坏唤醒/IRQ响应仍受实际脉宽和硬件延迟限制，主机模拟不能替代板测。PA15/PB3未增加EXTI；静止时其变化最迟在下一次后台唤醒重读，运动期间不睡且维持原轮询到位停止。

## 时间、ADC和HAL调用

低功耗模式专用TIM2是32位内部计数器，1kHz、ARR=0xFFFFFFFF，不使用引脚。其32位能力由工程STM32U031器件头确认。当前16MHz APB÷1产生PSC=15999；频率不整除、APB非÷1或TIM2已被别的模块占用时拒绝启用，保持原SysTick时基和CPU活动，不冒险使用错误时间。

启动PowerManager之前HAL_GetTick仍返回uwTick。TIM2初始化后，以`启动时uwTick+TIM2->CNT`作为HAL_GetTick，保持1ms单位及约49.7天自然回绕；TIM2的Sleep时钟保持使能。HAL_IncTick仍正常存在，但低功耗配置中的应用/HAL超时一律通过HAL_GetTick获得真实经过时间，不能直接使用原始uwTick判断睡眠时长。普通Sleep不会切系统时钟，本阶段没有恢复SystemClock_Config动作。

只有确认可以睡时才HAL_SuspendTick，清除旧SysTick pending，以TIM2比较中断安排到下一ADC截止时间的唤醒，最多250ms。比较旧标志在写新截止值之前清除；如果布置过程中已过截止时间则不执行WFI。任何进入后退出/放弃路径都HAL_ResumeTick，再恢复PRIMASK。此计时器唤醒不是RTC周期唤醒，也没有启用Stop UART唤醒API。

Sleep期间不执行HAL_Delay或UART发送；现有100ms启动等待发生在PM启用之前，现有2ms/1ms串口转向等待及逐字节发送期间只进行安全/ADC服务，不调用Idle。ADC启动/停止的既有有界HAL等待不与Sleep重叠，ADC转换/清理阶段许可为0。唤醒到期后继续约4Hz电池采样，BAT_AGE包含Sleep经过时间，不冻结读数年龄或消抖/换向计时。

保留普通USART接收中断。普通Sleep保持UART时钟时它可以自然唤醒CPU并缓冲数据，GPIO安全中断仍优先；这不等于实现了Stop模式串口接收，不能据此推断未来Stop1首字节可靠。

TIM2是3A专用资源，由power_port直接管理，未加入CubeMX自动初始化；请勿重复配置或由调试器冻结它后用软件计时结果验收。该时基不能直接用于Stop1，见后文3B分析。

## 诊断与构建验证

不新增ASCII命令或正式协议字段。调试器可以观察：power_sleep_entries（Sleep调用返回次数，并非实际驻留时间）、power_sleep_aborts（进入临界区后的取消次数）、power_last_blockers（位组合）。

| 阻止位 | 原因 |
|---|---|
|0x001|调试模式，禁止自动睡眠|
|0x002|电机准备/运行或驱动仍唤醒|
|0x004|安全关阀任务未完成|
|0x008|阀门任务/消抖/恢复/重新授权未完成|
|0x010|ADC忙或后台工作已到期|
|0x020|UART缓冲/半行/错误待处理|
|0x040|显式输入忙（框架预留；当前由阀门忙覆盖）|
|0x080|GPIO/IRQ事件、原始电平变化或截止时间竞态|
|0x100|实际PWM/nSLEEP/VREF未安全关闭|
|0x200|TIM2端口初始化失败，回退活动模式|
|0x400|已屏蔽IRQ或正在IRQ上下文，不允许睡|

power_last_blockers=0只表示上一次调用满足检查并返回，不证明CPU实际睡了多久。频繁串口/调试连接/噪声也能造成短睡或立即唤醒。

新增主机测试先观察桩实现及许可失败，再实现通过。最终178项：PM低功耗20、PM调试1、实际端口16、电池20、ADC端口7、快照5、RX11、阀门26、驱动14、应用调试28、应用低功耗30。包括事件在服务前/中/最后检查/WFI处到达、截止时间已过、GPIO事件不被清理吞掉、tick恢复、计数回绕、时钟/资源拒绝、UART半行、ADC到期及断线后绝不睡。测试编译-Wall/-Wextra/-Werror。

独立只读审查确认时基、竞态、资格查询和HAL集成无实质问题，并独立运行此前175项通过；后续新增3项端口竞态回归后由主代理重跑。主机的WFI/HAL/寄存器是模拟，不能证明实际芯片电气唤醒延迟与电流。

两个全新目录完成CMake/Ninja Debug干净构建，各42步，无编译/链接告警：

|电源配置|FLASH|RAM|
|---|---:|---:|
|调试默认|39212B /64KB|2880B /12KB|
|普通Sleep|40920B /64KB|2888B /12KB|

RAM数字不是实测最坏栈水位。默认镜像没有强HAL_GetTick覆盖；低功耗镜像强符号来自power_port，构建需包含该源文件。CubeIDE GUI及硬件未运行，下载时必须选对模式。

## 实机操作与验收（全部待验收）

1. 先保持电机断开，下载调试默认镜像或保持POWER_LOW_POWER_MODE=0。PING/STATUS、ADC与既有保护回归；power_sleep_entries应为0，Idle检查时阻止位包含0x1。确认新代码没有改变原控制。
2. 下载build/phase3a-low/STM32_PROJECT.elf，或CubeIDE明确设宏1并Clean/Build。先置L4正常、L5低、实际/模拟独立关到位，驱动输出应PWM0/nSLEEP0/VREF0、PA2始终低。输入稳定、ADC完成、串口无半行后，power_sleep_entries应增加。电池约每250ms后台更新，不再1ms无意义SysTick唤醒；频繁STATUS本身会唤醒CPU，不能用于测静态电流。
3. 功能观察可用调试器，电流验收必须脱离调试连接并独立供电。复位正常运行后，安静保持输入，记录至少10s的电流波形/均值/峰值，比较同供电、同阀位、同UART接线的调试模式。确认有约4Hz后台采样活动而不是1kHz唤醒活动；实际驻留时间需电流波形或专用跟踪确认，计数器不能代替。
4. 接回电机，用能承受启动电流的供电和电流测量链路。L5低→高唤醒开阀：先消抖，再按原停止/换向时序运行；OPENING及CLOSING期间power_sleep_entries不增长，到位后才允许继续增长。运动中查询仍正常。不要使用微安量程串联测电机启动电流。
5. 开到位且已Sleep时使L4高：记录PA3到唤醒/驱动动作的最坏延迟，必须建立安全关任务，关到位前休眠计数不能继续增加。L4提前恢复也必须关完；继续执行恢复后高电平不误开、低稳定后再高才开。
6. Sleep时给PA3高/低短脉冲、给PB0双边沿，至少重复20次并记录GPIO与驱动。含CPU刚准备睡、ADC刚结束和后台定时唤醒边界；不得丢失安全关。不要承诺捕获任意窄脉冲，记录实测可靠脉宽和最坏响应。普通开/关运行时触发限位仍立即按原轮询停止。
7. L5变化后处于50ms消抖、反向请求后2s等待、L4恢复100ms、低授权50ms期间观察计数，不能睡；有待处理安全关但双限位/DRV故障令其无法继续时，也不能睡。故障清除行为必须与第一阶段相同，nFAULT实机注入仍按安全夹具方案，不做堵转。
8. 让系统Sleep保持数秒后改变L5，测实际50ms消抖和2s换向，不应冻结或延长为“累计CPU醒着的时间”。电池BAT_AGE与PC经过时间对应，后台定时采样能持续。检查上电首次采样约500ms额外稳定等待，不因Sleep永远无法开始。
9. 分别发送半行PING不带LF、补LF、完整STATUS、受控坏帧。半行及未重同步坏帧应保持活动；补齐处理后才能睡。完整PING/STATUS在普通Sleep中仍应收到回复；普通UART IRQ不是Stop唤醒方案。发送/ADC转换中不睡，Sleep唤醒后HAL等待不死锁。暂停调试器时不要发送，避免人为ORE。
10. 测Sleep前后PA2、PA9、PA5、PA11电平，确认LIMIT_EN保持使能，PWM无毛刺、DRV未被误唤醒。复位/错误处理的既有安全关闭不变。TIM2被其他模块占用或时钟不支持时应回退活动，不能默默使用错误时间。

若出现唤醒丢失、误开、睡在安全关中、HAL计时停止或输出毛刺，先切断电机电源，回到宏0默认调试模式并保存快照/波形。硬件精度、GPIO毛刺、中断延迟、时基及电流尚未验收，本交付不能称为整板低功耗已通过。

## 需要分别测量的电流

在相同供电和输入下记录：CPU活动静止、普通Sleep静止、ADC周期采样峰值及占空比、UART接收/发送、开关运行与停止、断线关完成后、保护故障、接/不接ST-LINK、接/不接RS485适配器；同时记录BAT_SYS和3.3V。Sleep只降低CPU动态部分，TIM1/TIM2、内部参考、RTC/LSE、收发器、电源芯片与板上电阻网络仍可能耗电，不能拿MCU手册典型值作为整板目标。

据既有原理图的理论电阻电流（不是实测）：7.2V电池分压约3.30µA；VREF_EN高时82k+4.7k支路约38µA，本阶段睡前关闭；nFAULT拉低时10k上拉约330µA；LIMIT5被拉低时100k上拉约33µA；LIMIT4低时220k+1k约14.9µA（实际外部线路等效电路需确认）。限位使能电路的4.7k上拉遇到开关闭合时每支路约0.70mA，开关断开时4.7k与47k约63.8µA，每个端点状态可能不同。这些静态支路可能远大于MCU可节省部分。

## 3B仅设计预留：Stop1与LIMIT_EN

本地ST HAL提供HAL_PWREx_EnterSTOP1Mode，文档说明GPIO保持，STOPWUCK选择HSI或MSI作为唤醒系统时钟；必须结合目标芯片参考手册确认实际配置，不能直接把Sleep端口替换成Stop调用。3B需重新设计时基与后台ADC策略：TIM2普通APB计数在Stop停止，现有HAL_GetTick方案不能计量Stop时间，甚至会影响带超时的时钟恢复。须先确定Stop可运行的时间来源或明确暂停哪些时间任务，再设计时钟恢复、SystemCoreClock/外设分频、SysTick恢复顺序，任何等待都不能依赖尚未恢复的时基。

EXTI PA3/PB0唤醒保留同一代数/原始电平复核策略；Stop前确认nSLEEP/VREF/PWM安全，唤醒先处理断线，再考虑UART/ADC。需要验证实际GPIO保持、唤醒延迟和电机电源域默认电平。当前USART1 ASCII是否可用、首字节是否丢失，以及SWD调试保持Stop造成的额外耗电，应单独定义与实测，不能沿用Sleep验收结论。

LIMIT_EN的Q3低有效控制两路4.7k上拉。理论上关闭可减少上述静态电流，但原理图两输入还有47k下拉且经U5反相；关掉上拉可能让两个MCU限位同时读高，形成假双限位故障，并失去真实限位判断。不能简单在Sleep/Stop前拉高PA2。后续方案至少需区分检测电路断电与真实双限位、确认是否影响外部唤醒、先禁止驱动，唤醒后重新使能并等待原RC建立，再读取限位并做安全判断；这些条件未确认前维持PA2低。3A没有实施任何限位检测电路关断。

完成本阶段后停止，等待实机确认再开发Stop1。
