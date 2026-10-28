# 阶段2A：电池ADC与统一状态快照

日期2026-10-28。仅2A；用户已明确2B暂不开发。未烧录、未Push。第一阶段用户报告10项中8项通过；运行时限和nFAULT保护实机验收仍待完成。本次不改变已验证阀门控制，不恢复任何固定运行超时，不做堵转试验。

## 基线、文件和提交状态

开始时Git为`No commits yet on master`，工程全未跟踪。原67项模拟通过。修改前1257个源码/配置/测试/文档文件保存在`../../tmp/phase2a-baseline-20261028/source.zip`及`sha256.json`；未覆盖此前基线。阀门控制、驱动端口、数字消抖、恢复防误开及RX接收模块与该快照的哈希一致。

新增产品文件：Core/Inc/battery_monitor.h、battery_adc_port.h、app_status.h；Core/Src/battery_monitor.c、battery_adc_port.c、app_status.c。

修改产品/配置文件：Core/Src/main.c、Core/Src/adc.c、STM32_PROJECT.ioc、CMakeLists.txt、STM32CubeIDE/.project。根CubeIDE工程会发现Core源文件；子工程的显式链接源文件已补齐。

新增测试：tests/test_battery.c、test_battery_port.c、test_app_status.c。修改测试：tests/mocks/adc.h、tests/test_valve_app.c、tests/run_phase1.py。已有test_digital_control.py入口继续运行完整套件。

新增交付文件：docs/phase2a-plan.md、本报告、docs/phase2a-changes.json、tools/commit_phase2a.py。Git提交受当前环境`.git`只读权限阻挡：默认git add无法创建index.lock；两次授权执行因自动审批超时未启动。不能将它说成已提交。提交辅助脚本会先从不可变快照建立第一阶段基线提交，再只提交本次2A文件，检查文件哈希和现有暂存区/HEAD，任何冲突则停止；无Push命令。

## 换算与硬件核对

原换算正确：`VBAT=ADC_RAW×VDDA×2180/(4095×680)`，阻值单位均为kΩ，整数结果四舍五入到mV。原代码固定假定VDDA=3300mV；该假定误差会同比进入电池结果。

现在实际采集VREFINT并读取STM32U0出厂校准值，`VDDA_mV=round(VREFINT_CAL×3000/VREFINT_RAW)`。厂测电压是3000mV，不是3300mV；地址0x1FFF6EA4及校准条件来自工程中的ST官方`Drivers/STM32U0xx_HAL_Driver/Inc/stm32u0xx_ll_adc.h`。电池换算以64位整数合并运算，避免VDDA中间取整再次影响结果。7.2V对应采样点约2.246V，在VDDA3.3V时原始码约2787。工厂校准0/0xFFFF、VREFINT零/满量程、BAT零/满量程、超出12位的MOTOR码均不发布有效值；不静默退回假定3.3V。估计VDDA需在1500～4000mV宽范围内，作为本3.3V板的异常读数筛查，不是芯片工作额定范围，也不参与阀门保护。

据提供原理图`../../阀体驱动板.pdf`：电池采样分压R17=1.5MΩ、R19=680kΩ，C20=100nF。Rth约467.9kΩ，RC时间常数46.8ms；500ms约10.7个时间常数。原理图不代替实装阻容确认。原ADC为16MHz/1、79.5周期采样（4.969µs），现改为16MHz/4=4MHz、160.5周期（40.125µs），增加建立时间约8倍。第一轮再等待500ms，已有驱动初始化的100ms等待不变；所有新增等待均用后台状态机计时。

每个完整序列为rank1 PA0/IN4 BAT，rank2 PA7/IN14 MOTOR，rank3内部IN12 VREFINT；12位右对齐、AutoWait、软件触发、非连续、无DMA。HAL在配置内部通道时开启VREFINT路径，首次采样前的等待远大于LL头文件的12µs参考稳定时间。每轮完成后间隔250ms，约4Hz。4点滑动均值作用于经过VDDA补偿的电池mV；初次有样本即可有效，之后逐步填满窗口；失败后清空滤波窗口，恢复不混入故障前旧样本。滤波有延迟，不将该慢速电压用于过流或欠压动作。

高阻网络的C20提供采样电荷储备，延长采样能减少通道切换影响，但不能保证消除输入漏电、板面污染、焊接缺件、分压误差和机械负载噪声。ST外网资料工具本次连接失败；采样保持电容、输入漏电及源阻抗最坏值尚需对照完整芯片数据手册和板测闭环，不能宣称绝对精度已验收。

特别注意仪表加载：10MΩ输入仪表并在PA0节点时，理想直流读数约降低4.47%（Rth与仪表分压）。不要依据这种测量直接修改换算系数；电池电压对比应在BAT_SYS源端测量，节点波形应使用已知输入阻抗并考虑加载的探头。

ADC1_MOTOR按原理图直接连接DRV8874 IPROPI，R15=3.3kΩ对地、C18=10nF并联，PA7读取。保留原始码，不计算安培、不设阈值、不改变驱动器VREF_EN。实装版本、阻值公差和IPROPI电流比例尚需实机/芯片资料核对。

## 有效性、更新时间和快照

Battery_Init/Battery_Service/Battery_Get统一管理ADC，不再由STATUS发起或等待采样。每次服务只做一次启动、一次就绪转换读取，或一次停止；没有EOC时立即返回。完整三通道均成功且停止成功后才整体发布BAT/MOTOR/VREF、mV及更新时间，部分成功不发布。

- BAT_HAS：是否曾有完整有效结果。
- BAT_VALID：最近完成的尝试成功，且上次有效值年龄不超过1000ms。
- BAT_UPDATED：最近一次有效结果发布的上电毫秒数；失败不更新。
- BAT_AGE：上述结果年龄；从未有有效结果时为UINT32_MAX（4294967295），结合BAT_HAS=0识别。
- BAT_SEQ：成功发布序号；查询、失败不会使其增加。
- ADC_RESULT：NOT_READY、OK、CAL_ERROR、START_ERROR、TIMEOUT、READ_ERROR、STOP_ERROR或RANGE_ERROR。
- ADC_FAILURES：完整尝试失败次数；启动即无有效校准时直接不可用，不伪造尝试次数。
- VDDA、VREF_RAW：最近有效序列的估算参考电压mV及原始参考码；不是PA5的VREF_EN。
- BAT_SYS：滤波后的最近有效mV；ADC=ERR/BAT_VALID=0时仍可显示此旧值，必须结合有效性与更新时间使用。
- ADC_RESULT=OK但BAT_VALID=0可表示最后一次尝试成功、但数据已过期，不代表本次查询获得了新样本。

序列20ms仍未完成就进入清理并报告ADC采样超时，这不是电机行程超时。失败保留最近有效值及其时间，同时清除有效性；下一周期允许恢复。停止/禁用硬件失败则保持ADC不可用至重初始化，不每轮重复阻塞清理。原HAL在SysTick正常条件下Enable子程序2ms超时（含tick量化约≤3ms），Stop包含停止和禁用两个2ms子程序（含量化约≤6ms）；故障等待时GPIO断线中断仍能抢占。ADC绝不主动改变阀门故障或驱动状态。最坏循环延迟和SysTick正常性需板测，不能把软件时基超时当作CPU卡死的独立保护。

AppStatus_Publish在主循环安全处理及ADC服务之后，把阀门状态、输入、当前/锁存/历史故障、驱动输出及完整电池元数据复制到统一快照。AppStatus_Get返回副本，只计算该副本数据年龄，不读GPIO、不消费断线事件、不触发ADC、不修改安全状态。两个接口仅限主循环调用，IRQ不读写快照；通信没有阀门内部状态的可写接口，已有控制命令继续经过第一阶段安全仲裁。

快照保留原始L4和状态机的当前断线位，两者可因恢复稳定时间而暂时不同。SNAP_MS为快照捕获的上电毫秒数；传输期间状态可能变化，该行表示当时的快照，不承诺传完时仍处于同一状态。STATUS保留既有ASCII字段并追加上述诊断字段，没有更改正式协议或增加CRC。前述67项第一阶段保护全部回归；未恢复行程超时。

## 行程安全超时的最小恢复建议（只建议，未实施）

现有run_tick仍用于100ms点动，TIMEOUT诊断位0x02已保留，但常规开/关没有最大时间判断。最小方案是在valve_config.h增加独立开/关上限，0明确为禁用；每次PWM实际启用时开始计时，在保护收集处检查已启用的上限，再处理到位，避免后到的限位覆盖已超时事实。超时停机并锁存TIMEOUT；安全关未到位时还记录SAFE_INCOMPLETE，排障/显式清除后才可重试。

先测量正常完整开/关行程在已允许供电范围、负载、温度条件下的最坏时间，再结合机械允许连续运行时间决定余量和最终值。不能只取平均时间，也不为测保护而堵转。超时逻辑可在主机模拟器中用虚拟时间验证；板上失限位测试先断开电机或机械脱载。须用户确认并实测后再启用，当前固件仍无限时。

## 已执行验证

主机模拟执行真实产品C代码：battery17、battery_port7、snapshot5、RX9、controller21、valve_port14、application26，总99场景通过。覆盖VDDA变化、异常参考、滤波、初次无数据、失败保留值/时间、完整序列提交、停止失败、数据过期、时间回绕、快照副本隔离、连续查询、ADC卡住时断线抢占与安全关查询。原应用STATUS采样行为已由缓存语义回归替换。核心与快照先观察桩实现失败，集成先观察旧STATUS缺少有效性字段失败，再实现通过；HAL适配器以实际封装及模拟HAL单独验证。

在新目录build/phase2a-clean完成独立CMake/Ninja Debug干净配置与40步构建，无编译/链接告警：FLASH38180 B/64KB（58.26%），RAM2856 B/12KB（23.24%）。RAM非实测最坏栈水位；STATUS缓冲640字节，尚需板上栈水位确认。CubeMX重新生成和CubeIDE GUI构建未执行；两个IDE源文件入口/.ioc已同步，须板上使用CubeIDE刷新Clean/Build再下载。

运行方式与第一阶段相同：`python tests/test_digital_control.py`或`python tests/run_phase1.py`，需要ARM GCC和Unicorn2.1.4。ADC端口测试对真实封装执行EOC/OVR检查；芯片模拟电气性能未模拟。独立只读审查确认原六个阀门/RX源文件完全未变，并独立执行此前95项通过；随后增加4项边界/集成回归，最终99项由主代理重跑。

## 电池与通信实机验收步骤（全部待验收）

1. 先断开电机，记录旧固件同电压STATUS和万用表BAT_SYS电压；确认实装R17/R19/C20及供电允许范围。刷新CubeIDE，Clean/Build，确认battery_monitor.c、battery_adc_port.c、app_status.c均被编译，下载运行。
2. 上电早期可能显示ADC=ERR、ADC_RESULT=NOT_READY、BAT_HAS=0；约500ms后应获得有效序列。上电1秒后查询应见BAT_VALID=1、VDDA接近实测VDDA、BAT_SEQ递增、BAT_AGE通常小于一个采样周期附近。不要把上电暂未采样当作电池故障。
3. 先在标称7.2V测试，再在硬件已确认允许的范围内选至少另外两点；不根据7.2V推断电池化学类型、满充/放空电压。每点稳定至少2秒，记录10次STATUS的BAT_SYS/VDDA/BAT_RAW/VREF_RAW/AGE及万用表，计算均值、极差和偏差。当前没有约定精度验收阈值，记录后按电阻公差、校准精度及用途确认。
4. 改变供电后观察原始电压、滤波值及序号，允许RC+四点均值带来的约1秒量级响应；不应出现长期明显偏低或无意义跳变。使用已知阻抗探头核对PA0滤波，注意10MΩ加载效应。最终允许波动与精度需实测确定，不在软件中擅自校正比例。
5. 接回电机，重复正常开/关、断线关阀和恢复防误开；在静止/启动/运行阶段记录电池读数与VDDA，确认ADC_FAIL无持续增加，限位停机和断线响应不受影响。当前nFAULT实机保护仍属于第一阶段待验收项；不通过堵转或短路验证。
6. 连续查询：115200/8N1，发送PING和STATUS，收到完整回复再发下一条，连续至少100轮。应不缺行、不截断，正常无RX硬件/缓存溢出。BAT_SEQ按后台约4Hz递增，不能每查询一次就伪造新样本；电机运行中查询不得停机。若上位机无间隔发送超过缓冲容量，属于需验证的错误恢复场景，不承诺无限吞吐。
7. 断线查询：开向运行时触发真实L4断线，持续查询STATUS；观察CLOSING/SAFE_PENDING=1及电池有效性，恢复L4但保持L5高不能误开，关到位后SAFE_PENDING=0。ADC读数异常也不能取消安全关。
8. 有效性故障注入已在模拟器验证；板上可在电机断开的调试条件下受控暂停ADC服务或禁用ADC转换，恢复运行后查询。应保留最近BAT_SYS但BAT_VALID=0，BAT_UPDATED/BAT_SEQ不增加，BAT_AGE增加；不能以断开电池、让系统掉电的方式验证旧值保留。硬件STOP失败需重新初始化/复位；恢复成功时第一新值不混入故障前滤波窗口。
9. CRC错误测试：本阶段明确不适用，ASCII没有CRC，不能声称已验收CRC。可发送未知命令/超长行/受控ORE验证错误回复或重同步，再发PING恢复；正确CRC算法、帧格式与错误帧策略待2B另行定义。
10. 记录最坏循环/断线中断响应、STATUS完整发送时间、栈水位、实际ADC分频与VREFINT读数。无需也不得为本阶段执行危险堵转。

交付后停止在2A；2B、正式CRC协议、电池百分比、低功耗STOP均未开发。
