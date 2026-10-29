# 3B ADC模拟电路关断修复（2026-10-29）

用户授权：先补齐Stop1前ADC内部稳压器/VREFINT关断；限位4.7k电阻由用户实测。基线35d910c，用户未提交的power_config.h模式2必须保留且不包含在本修复提交中。不Push。

设计与实施计划：
- 复用battery_adc_port：仅ADC完全禁用且无转换/校准/停止请求时保存ADVREGEN及CCR内部通道位；关闭内部路径和稳压器，保留分频/扫描/校准，不复位ADC。
- power_port所有正常返回路径（含RTC读失败及晚到事件取消WFI）都成对恢复；先恢复时钟与TIM2，再恢复ADC模拟供电。故障停机路径允许保持ADC关闭，不尝试在未知时钟中运行。
- 恢复后的稳定等待采用主循环非阻塞资格判断，2个HAL毫秒tick间隔（实际至少约1ms，覆盖本库20us稳压器、12usVREFINT稳定要求）。期间安全状态机和IRQ照常执行，采样尝试/20ms转换超时尚不启动，不制造START_ERROR或旧数据更新。
- ADC恢复只影响后台检测，不改变DRV8874的PA5 VREF_EN、nSLEEP、LIMIT_EN、阀门状态机或数字量定义。仅配置2实际调用关断/恢复；0/1行为保留。
- 先增加失败测试：禁用条件、WFI时模拟电路确实关闭、取消路径恢复、原关闭状态保留、稳定等待和tick回绕、等待期间安全关阀优先。再实现并运行全部测试及三配置干净构建。

依据：[RM0503](https://www.st.com/resource/en/reference_manual/rm0503-stm32u0-series-advanced-armbased-32bit-mcus-stmicroelectronics.pdf)14.4.2及14.10；本地U0 HAL1.0.0的HAL_ADC_Stop只禁用ADC，本地LL_ADC_DELAY_INTERNAL_REGUL_STAB_US=20、LL_ADC_DELAY_VREFINT_STAB_US=12。稳压器关闭时内部校准保持，无需复位或每次重新校准。

## 完成结果

- 主机模拟295项全部通过（原275项，新增20项）：ADC端口18、电池25、Stop端口25、应用Stop34，其他套件保持通过。覆盖ADEN/转换/校准/停止忙时拒绝、保存与恢复无关位和校准、重复挂起拒绝、原已关闭状态不被意外开启、tick回绕、稳态等待不记采样失败、RTC失败/取消WFI恢复、模拟电路稳定期间断线安全优先。日志：工作区`tmp/phase3b-adc-final-tests.txt`。
- 独立只读复核通过，独立重跑295项通过。
- 三个全新目录CMake/Ninja干净构建，各43步，无编译/链接警告。debug FLASH39420B/RAM2888B；Sleep FLASH41184B/RAM2920B；Stop1 FLASH43732B/RAM2936B。构建目录`build/phase3b-adc-debug`、`build/phase3b-adc-sleep`、`build/phase3b-adc-stop`，ELF均为STM32_PROJECT.elf。
- 默认测试配置显式固定为0，防止用户将头文件改成2后把标为debug的用例也隐式编成Stop；固件模式配置不受此测试修正影响。
- 未修改阀门状态机/驱动、GPIO/输入、ADC分压换算或滤波算法、RTC/UART协议，也没有修改外部PA5 VREF_EN。用户原有power_config.h模式2保留在工作区，独立提交不包含它。

## 全部修改文件

1. Core/Inc/battery_adc_port.h：增加挂起/恢复/稳定资格接口。
2. Core/Src/battery_adc_port.c：保存并关闭ADC内部路径和稳压器，恢复并非阻塞稳定等待，保护所有采样启动。
3. Core/Src/battery_monitor.c：稳定前不开始采样尝试/转换超时，不伪报失败。
4. Core/Src/power_port.c：仅Stop1挂接成对关闭/恢复，包括RTC失败和WFI取消路径。
5. tests/mocks/main.h：ADC公共寄存器及模拟供电位。
6. tests/run_phase1.py：测试模式显式固定。
7. tests/test_battery.c：模拟电路未稳定时不计失败，稳定后继续采样。
8. tests/test_battery_port.c：实际ADC适配器关断/恢复/保持/等待/回绕测试。
9. tests/test_power_stop.c：验证WFI前关闭及每条正常退出恢复路径。
10. tests/test_valve_app.c：完整应用唤醒后ADC稳定及断线优先回归。
11. docs/phase3b-adc-fix.md：本说明（新增）。

## 实机复验

1. 保持用户模式2，CubeIDE Refresh、Clean、Build，下载此次ELF。先确认正常开/关到位、静止断线唤醒关完、断线提前恢复关完、恢复后高请求不误开。
2. GPIO唤醒后的活动期间先发LF再STATUS，检查BAT_VALID、BAT_SEQ、BAT_UPDATED和ADC_FAILURES；重复至少20次唤醒，电压合理、序号更新，不能持续ADC错误/超时。稳定窗口不是新的ADC失败原因；瞬时BAT_VALID=0可能因很快完成新采样而未被串口捕获。
3. 无ST-LINK、无周期UART查询，同一供电、同一阀位/输入/外部接线，稳定Stop后清零统计，记录60秒平均/最低/峰值，与此前约424uA基准比较。构建通过不代表实际节能量，不能预先承诺降低多少。
4. 对照代码可查看进入Stop调用前ADC_CR.ADVREGEN=0、ADC_CCR.VREFEN/TSEN/VBATEN=0；醒来恢复此前值。软件调试断点会影响功耗，仅作寄存器检查，不作为正式电流结果。普通STATUS没有新增这几个寄存器字段。
5. 4.7k电阻测量由用户独立进行，记录电阻编号、开/关阀位、两端实际压降；不要为测功耗直接关闭LIMIT_EN。

本补丁尚未烧录，ADC恢复精度、物理关断状态、唤醒最坏延迟和电流变化待实机验收。完成后停止，不自动开发其他低功耗功能或Push。
