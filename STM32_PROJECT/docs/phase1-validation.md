# 第一阶段软件验证记录

最新串口修复验证：2026-10-28，RX队列9、控制器21、端口14、应用23，共67项通过；CMake Debug构建通过，FLASH35904 B、RAM2672 B。详情见 `rs485-rx-fix-20261028.md`。以下为此前历史验证记录，板上PING仍需重新烧录验收。

## 2026-10-28更新：取消运行超时

按用户要求移除开、关及断线安全关阀最大运行时间。修改前文件与上次交付哈希一致；本次修改前快照为 `../../tmp/limit-only-before-20261028.zip`。先添加不限时运行回归，观察原代码断言失败，再移除30秒配置、非零时限断言及超时判断。

最终执行 `tests/run_phase1.py`：控制器21、端口14、应用23，共58项通过。测试覆盖正常开、正常关、安全关在模拟运行超过1小时后继续驱动、对应限位到达后停止；故障恢复用真实DRV保护场景验证，100 ms点动保留。Debug构建成功：FLASH 38320 B，RAM 2520 B。未烧录，实机仍待验收。以下56项、38356 B及超时回归记录为2026-10-27历史结果，不代表当前配置。

验证日期：2026-10-27。无硬件连接、无烧录、无机械/电气实测。

## 已执行结果

1. 修改前 Git：`No commits yet on master`，全工程未跟踪；保留 `../../tmp/phase1-baseline-20261027/source.zip`、`sha256.json`、`git-status.txt`。没有创建提交或改写 Git 历史。
2. 控制器测试在桩实现上首先以“默认数字开阀未执行”断言失败；端口桩以“参考输出未使能”断言失败；实际实现后通过。运行时限与到位同时发生、无锁存时反复 FAULTCLR 两个回归均先观察到失败，再修复通过。
3. 最终执行原有入口 `tests/test_digital_control.py`，它运行新完整套件：controller 19、port 14、application 23，共 **56 场景通过**。编译采用 `-Wall -Wextra -Werror`，最终测试无编译告警。
4. 新建 `build/phase1-clean` 配置 CMake/Ninja Debug 成功；最后又以 `cmake --build ... --clean-first` 清理并重新完成全部 36 个编译/链接步骤，退出码 0，无编译/链接告警。
5. 链接内存：FLASH **38356 B / 64 KB（58.53%）**；RAM **2520 B / 12 KB（20.51%）**。RAM 数字包括链接器保留空间，不等于实测最坏栈使用；未做板上栈水位测量。
6. 独立只读审查指出/确认了超时优先于到位的修复；最终重新检查无锁存清除保护，并独立运行相同的 19+14+23 场景，无新增实质问题。

控制器覆盖：默认控制、初始化等待、端点停止与反向离开、限位释放不重启、断线抢占/脉冲/持续高、恢复后重新低电平授权、安全关不可撤销、三个启动阶段断线、双限位与DRV同时故障、开关最大运行时限、关失败锁存、历史与人工清除、点动、最终端口守卫拒绝、时基回绕、已有关到位记忆后的再次安全关、运行中手动请求拒绝、超时与到位同时发生、无故障清除不拖延安全关。

端口覆盖：真实PH/VREF/nSLEEP/PWM写入及CCR=800、中断快速去使能、断线不打断关向、原始高/pending/粘滞事件三重检查、VREF/nSLEEP/CCR写入边界高电平与短脉冲注入、保护/目标限位拒绝、HAL与时基无关的异常关断。

应用覆盖：实际main.c中的PING、STATUS、ADC成功与超时、回复期间断线和到位、全部旧手动/直控测试命令在安全关期间的行为、接收损坏/丢弃恢复、发送失败、驱动故障及FAULTCLR历史保留。

## 工具与命令

实际 Python：`D:/chen/DeepSeek Harness/resources/runtime/primary-runtime/dependencies/python/python.exe`。

ARM GCC 14.3.1：`D:/chen/STM32CubeIDE_2.2.0/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740/tools/bin`。

CMake/Ninja 来自同一 IDE 的 externaltools 插件。安装目录未改动。测试专用 Unicorn 2.1.4 wheel 通过 PyPI 提供的 SHA-256 验证，解包在工程外 `../../tmp/phase1-testdeps/`，没有写入固件依赖。

从工作区根目录执行的最终测试命令：

```powershell
& 'D:/chen/DeepSeek Harness/resources/runtime/primary-runtime/dependencies/python/python.exe' STM32_PROJECT/tests/test_digital_control.py
```

构建可复现命令见 `phase1-acceptance.md`。最终原始日志在 `../../tmp/phase1-final-tests.txt`、`../../tmp/phase1-final-build.txt`，最终 Git 状态在 `../../tmp/phase1-final-git-status.txt`。源文件与基线的差异清单见 `phase1-changes.json`。

## 未验证与限制

未运行 CubeIDE GUI 构建、Release 构建、硬件在环测试或烧录。没有把模拟通过解释为实机通过。暂定开/关最大运行时间均30秒，必须标定；断线恢复100 ms和数字消抖50 ms也需对照现场信号验收。56个场景是在模拟硬件上执行真实C代码，未模拟GPIO电气毛刺宽度、真实中断延迟、电源跌落、PWM模拟时序、DRV模拟特性或机械动作。

本阶段代码交付后停止；继续开发需用户明确安排。
