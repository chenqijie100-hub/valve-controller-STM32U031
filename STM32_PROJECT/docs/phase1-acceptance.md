# 第一阶段：状态机与断线安全控制

2026-10-28串口修复补充：板上复现PING接收溢出后，接收改为USART1 RXNE/错误中断加128字节环形缓存，主循环解析ASCII。发送及阀门控制不变。最新67项验证及重新烧录步骤见 `rs485-rx-fix-20261028.md`；下文58项/历史测试记录应结合该补充阅读。

更新日期：2026-10-28。按用户要求取消开、关及安全关阀最大运行时间，持续驱动直到对应限位或其他保护/有效控制停止。100 ms点动保留。软件主机模拟及交叉编译验证完成；全部板级行为待实机验收。

## 基线与修改范围

开始前使用 `D:/Espressif/tools/git/cmd/git.exe status --short --branch`，结果为 `No commits yet on master`，工程全部未跟踪，没有可引用的已验证提交。没有执行 reset、checkout、add、commit。修改前 1231 个源码、配置、库文件的 ZIP、SHA-256 清单和 Git 状态保存在工程外的 `../../tmp/phase1-baseline-20261027/`。源文件快照排除了 Debug/build/Release 产物；现有用户 CMake HAL 源列表已经完整，本次未覆盖它。历史 `.bak`、原硬件 PDF 保留。

新增产品代码：

- `Core/Inc/valve_config.h`：集中时序与临时行程时限。
- `Core/Inc/valve_control.h`、`Core/Src/valve_control.c`：状态、调度、仲裁、当前/锁存/历史故障、恢复条件。
- `Core/Inc/valve_port.h`、`Core/Src/valve_port.c`：实际 GPIO/PWM、最终写入安全门、中断快停、异常关断。

修改产品/构建代码：

- `Core/Src/main.c`：上电 GPIO 顺序、默认数字控制、统一命令入口、诊断调度。
- `Core/Src/stm32u0xx_it.c`：LIMIT4 上升沿回调；NMI/HardFault 安全输出。
- `CMakeLists.txt`：加入两个业务源文件，放在用户维护区。
- `STM32CubeIDE/.project`：给使用链接源文件的子工程加入两个源文件；根 CubeIDE 工程本来会发现 Core，无需更改。

新增验证与文档：

- `tests/run_phase1.py`、`tests/test_valve_control.c`、`tests/test_valve_port.c`、`tests/test_valve_app.c`、`tests/test_runtime.c`、`tests/requirements-phase1.txt`。
- `tests/mocks/main.h`、`tim.h`、`adc.h`、`gpio.h`、`rtc.h`、`usart.h`。
- `docs/phase1-plan.md`、本文件。

修改验证入口：`tests/test_digital_control.py`，保留旧入口名，改为执行新测试。旧测试的“上电 DIN 禁用”前提不再符合需求，其消抖、到位、换向、禁止重试、时基回绕等有效场景迁入新测试。

辅助产物不属于固件：`../../tmp/phase1-baseline-20261027/` 基线、`../../tmp/phase1-testdeps/` 主机模拟器依赖、独立 `build/phase1-clean/` 及最终验证日志。未修改 `.ioc`、HAL/CMSIS、ADC/TIM/USART 初始化、电压换算、低功耗功能或正式通信协议。

## 状态与动作

| 状态 | 含义 |
|---|---|
| UNKNOWN | 初始/中途停止/等待换向，尚无明确端点结论 |
| OPENING | 开向启动时序或开向运行 |
| OPENED | 单独开限位有效时确认开到位 |
| CLOSING | 关向启动时序或关向运行，是否安全关阀另看 SAFE_PENDING |
| CLOSED | 单独关限位有效时确认关到位 |
| FAULT | 保护故障锁存，禁止驱动，不能用 CLOSED 替代 |

LIMIT1=PA15、LIMIT2=PB3，高有效；LIMIT4=PA3，高为断线；LIMIT5=PB0，高开低关。PB1 不参与业务。PA2 保持低，使限位电路开启。到位后保持本次到位结论，避免开关抖动/释放引起自动重启；需要新的有效命令变化才开始下一动作，因此状态表示上次确认的阀位，原始 L1/L2 仍独立回报。

上电首先 HAL_Init、GPIO 安全低输出，再配置系统时钟和其他外设；ADC 校准、原有 100 ms RC 等待完成后才初始化端口、控制器并启用控制。上电自动模式打开，无需 DINON；LIMIT5 需稳定 50 ms，驱动还需满足停止后 2000 ms 的间隔。电机启动不依赖串口发送成功。ADC 校准失败会在 STATUS 中报告；本阶段没有 ADC 保护，校准失败不会独立禁止阀门控制。

保留已验证参数：PH 低开高关，TIM1_CH4 20 kHz；RUN/自动控制为 100%，JOG 为 50%/100 ms；100% CCR=800。停止依次拉低 nSLEEP/VREF、清 CCR；nSLEEP 优先关断，因为 CCR 预装载不能保证比较值立即生效。下次动作至少等待 2000 ms，然后设 PH、VREF 高并等待至少 20 ms，再 nSLEEP 高等待至少 2 ms，最后施加 PWM。原有阻塞等待改为状态机计时，时序顺序和最小等待保留。

**开阀、关阀和断线安全关阀均无最大运行时间。** 原30秒常量、非零时限断言和超时停机判断已移除。正常运动由目标限位、DRV故障、双限位或有效控制命令停止；断线安全关不接受撤销命令，保持到独立关到位或其他保护故障。限位失效时不会再按运行时间停机。100 ms点动仍按明确点动时长结束。

## 断线优先和恢复

1. LIMIT4 上升沿只记录粘滞事件，并立即关闭开向或开向准备中的 nSLEEP/VREF/PWM；不在中断内延时、发串口或换向。
2. 主循环将事件或原始高电平转成 WIRE 当前故障，锁住开阀许可，建立 SAFE_PENDING。正在关阀时保持关向，不被断线事件反复停机或重置运行时限。
3. 正在开阀时先停止，经过完整停止/换向间隔后关阀。启动阶段同样可抢占。所有可能使能输出的写入前后都在很短的临界区检查原始 PA3、EXTI 上升沿 pending 和粘滞事件；外部信号在屏蔽中断期间变化也会被检查到。
4. SAFE_PENDING 只在“关限位高且开限位低”时完成；LIMIT4 提前恢复、LIMIT5 改变、STOP/DINOFF/SLEEP、通信异常都不能撤销它。
5. LIMIT4 连续低 100 ms 后，WIRE 当前位自动解除，历史保留，SAFE_PENDING 不变。
6. 从解除 WIRE 之后重新计时，必须观察到 PB0 连续低 50 ms 才解除开向闭锁。上一次断线前或断线期间的低电平资格不能借用。随后数字高电平才能开阀；明确的手动 RUNA/JOGA 同样必须先满足该低电平授权，但可作为数字高请求的人工替代。
7. 若DRV故障、双限位或驱动启动失败使关阀无法继续，停止并锁存 SAFE_INCOMPLETE，状态 FAULT，不谎报 CLOSED。明确排障并清除保护后，可以继续仍保持的安全关阀请求；不会自动反复重试保护故障。没有运行时长导致的停机。

## 故障与 ASCII 调试

| 位 | 名称 | 当前/锁存/恢复 |
|---|---|---|
| 0x01 | WIRE | 当前跟随立即断线/100 ms 稳定恢复；历史保留，不作为人工清除的保护锁存 |
| 0x02 | TIMEOUT | 保留诊断位编号以免其他故障位变化；当前代码不产生运行超时故障 |
| 0x04 | DRIVER | 唤醒稳定后或运行中 nFAULT 低，保护锁存；不把睡眠期间电平当作新驱动故障 |
| 0x08 | BOTH_LIMITS | 两限位同时高立即锁存、禁止驱动；原始矛盾解除后仍需人工清除 |
| 0x10 | SAFETY_INCOMPLETE | 安全关阀因保护无法完成，保护锁存并保留历史 |
| 0x20 | START | PWM/端口使能失败，保护锁存 |

CURRENT 表示实时输入故障及尚未确认的启动/未完成事件；LATCHED 是禁止驱动的保护集合；HISTORY 为本次上电以来发生过的位集合。驱动停止后 nFAULT 的有效性与芯片睡眠状态有关，所以 DRIVER 当前位只在适配层认为驱动唤醒时读取，停止后仍以 LATCHED/HISTORY 追踪故障。历史保存在 RAM，不跨复位/掉电；本阶段不写 Flash。

FAULTCLR 条件：必须确有保护锁存、驱动已停、WIRE 已稳定恢复、无新断线事件、原始 nFAULT 为高、没有双限位。成功清锁存但保留历史，重新要求 PB0 低电平授权，并保留未完成的关阀请求。没有保护锁存时拒绝 FAULTCLR，避免重复清除命令延后安全关阀。若实机睡眠时 nFAULT 不能回高，则不能满足清除条件，需确认驱动器/上拉行为并排障，不能绕过该条件强行重启。

保持 115200/8N1、LF/CRLF ASCII 和 PB4 半双工方向控制。STATUS/PING 不停机。STATUS 保留已有电压和原始 GPIO 字段，追加 STATE、CURRENT、LATCHED、HISTORY、SAFE_PENDING、SAFE_INCOMPLETE、OPEN_OK。它是采样时快照，发送过程中状态仍可变化。发送每字节之间、转向等待和 ADC 等待期间均继续处理安全控制；单字节发送最长 2 ms 超时，总发送预算 100 ms。接收异常丢弃到下一个 LF，不改阀门安全请求。主机仍需一问一答，避免回复期间发送下一命令导致接收溢出。

RUNA/RUNB/JOGA/JOGB 只登记受保护请求。已经在启动/运行时的新手动动作请求拒绝；正常 STOP/DINOFF/SLEEP 停止并关闭自动模式，DINON 恢复自动模式。安全关阀期间这些取消/改向命令拒绝。PWM0/25/50/75、VREFON、WAKE 原始直控调试命令在本阶段全部拒绝，避免存在第二个电机输出所有者。

## 可执行验证

主机模拟编译的是真实 C 控制器、硬件适配层和 main.c 的实际解析、STATUS、ADC、TX 路径；仅硬件寄存器/API 用模拟实现。ARM GCC 编译，Unicorn 在主机执行 Cortex-M0+ 指令；不能替代电气/机械测试。

依赖：Python 3、`arm-none-eabi-gcc`/`arm-none-eabi-nm`、`unicorn==2.1.4`。可用 `python -m pip install -r tests/requirements-phase1.txt` 安装主机依赖；本次使用工程外 `../tmp/phase1-testdeps` 中 SHA-256 验证过的 wheel。固件无新增第三方依赖。

```powershell
# 在 STM32_PROJECT 中执行；工具在 PATH 时无需设置 ARM_CC。
$env:ARM_CC = '<ARM GCC tools>/bin/arm-none-eabi-gcc.exe'
python tests/run_phase1.py
# 旧入口也执行相同完整套件：
python tests/test_digital_control.py
```

本机实际 Python：`D:/chen/DeepSeek Harness/resources/runtime/primary-runtime/dependencies/python/python.exe`。ARM GCC 来自 STM32CubeIDE 2.2.0 的 `plugins/*gnu-tools*/tools/bin`；runner 包含本机工具自动发现，也可通过 ARM_CC/ARM_NM 显式指定。

干净构建使用一个原先不存在的 `build/phase1-clean` 目录，保留旧 Debug/build/Debug 产物。工具来自同一 IDE 的 CMake、Ninja 和 GNU 插件：

```powershell
# 在 STM32_PROJECT 中执行，cmake/ninja/ARM GCC 已加入 PATH。
cmake -S . -B build/phase1-clean -G Ninja '-DCMAKE_TOOLCHAIN_FILE=cmake/gcc-arm-none-eabi.cmake' '-DCMAKE_BUILD_TYPE=Debug'
cmake --build build/phase1-clean
```

CubeIDE 的两个工程入口均补齐了源文件关系；本次实际执行的是 CMake Debug 交叉构建，没有运行 IDE GUI 构建或下载。最终测试数量、内存占用、源码哈希及构建输出见随附验证记录。

## 实机测试清单（全部待验收）

测试前先使用限流电源、示波器/逻辑分析仪，记录 PA3、PA10、PA11、PA9、PA5、PB3，确认机械行程允许临时最大时间。涉及限位失效的测试先用脱离机械负载的模拟条件，避免直接堵转；本阶段没有新增过流保护。

| 编号 | 操作 | 必须观察到的结果 |
|---|---|---|
| B01 | PB0 高/低分别上电；同时观察初始化阶段输出 | 初始化/100 ms RC 等待前不驱动；之后默认数字控制，稳定输入且间隔满足后动作 |
| B02 | 启动时已开到位且请求开、已关到位且请求关 | 无相应方向输出；状态端点正确 |
| B03 | 正常低→高→低，分别触发开/关到位 | PH 方向正确，到位停机；20 kHz、50% 点动及100%连续符合原实测 |
| B04 | PB0 <50 ms 抖动，运行中持续反向请求 | 短抖动不换向；有效变化先停，停满2 s，再走VREF20 ms/nSLEEP2 ms时序 |
| B05 | 到位后限位抖动或释放、PB0不变 | 不反复重启；L1/L2原始值可变，端点记忆可追踪 |
| W01 | PB0高、阀门开向运行时PA3拉高 | 测量开向nSLEEP立即拉低的中断响应；停满2 s后PH换关向，持续至关到位 |
| W02 | 分别在VREF准备、nSLEEP拉高前后、首个PWM附近注入PA3高或短脉冲 | 不出现持续开向驱动；脉冲不丢失，最终安全关阀；记录最坏电气响应时间 |
| W03 | 关向运行时断线、断线持续高、PB0反复变动 | 不中断/反复重启关向，不重置行程时限，关到位后停止 |
| W04 | 断线上电，分别位于中途/开到位/关到位 | 禁止开；必要时安全关；已独立关到位不再驱动 |
| W05 | 安全关过程中PA3恢复低，PB0始终高 | WIRE低稳定100 ms后清当前位；仍关至到位，之后不自动开 |
| W06 | PA3恢复中有短高脉冲，PB0低只在断线前/期间有效 | 恢复计时重新开始；先前低电平不解除开向闭锁 |
| W07 | 恢复后PB0低保持<50 ms/≥50 ms，再高 | 短低不授权；有效低后高才允许开，仍不得取消尚未完成的安全关 |
| W08 | 断线后已关到位，PB0持续高；发送RUNA/JOGA/WAKE等 | 未完成低电平授权时拒绝开；原始驱动测试始终拒绝 |
| F01 | 上电/静止/运动时两限位同时高 | 记录BOTH_LIMITS并停机，状态FAULT，无危险动作；恢复单限位后不自动重试 |
| F02 | 唤醒稳定后/运动中nFAULT拉低，特别在安全关中 | 立即由循环停机，DRV锁存；安全关未到位时SAFE_INCOMPLETE=1，不报告CLOSED |
| F03 | 受控脱载条件下分别运行开、关、安全关超过原30秒，再触发目标限位 | 不因运行时长停机，不产生TIMEOUT；触发对应限位后停止 |
| F04 | 长时间运行后同时触发到位、nFAULT或双限位 | 其他保护仍优先并保留故障，不用到位覆盖DRV/双限位故障 |
| F05 | 故障后恢复电平但不发FAULTCLR，再按条件发FAULTCLR | 先保持停机；清除后历史保留、重新低电平授权，待处理安全关可以继续 |
| F06 | 断线/双限位/nFAULT低/运动时反复FAULTCLR；无故障时反复清除 | 条件不满足则拒绝；无故障清除不重置换向等待，不拖延安全关 |
| C01 | 开/关运行中连续PING/STATUS | 返回合法ASCII；不中断电机，到位及其他保护仍及时处理，PB4正确释放 |
| C02 | STATUS发送或ADC等待过程中触发断线/限位/nFAULT | 安全响应仍执行；测量最坏循环延迟，断线中断快停不依赖发送完成 |
| C03 | 安全关期间注入乱码、超长行、串口ORE/FE/NE、发送失败 | 安全关继续；丢弃坏帧到LF后通信可恢复 |
| C04 | 安全关期间发送STOP/SLEEP/DINOFF/RUNA/RUNB/JOG/PWM/VREFON/WAKE | 不撤销安全关；被拒绝命令无输出副作用 |
| X01 | 受控条件下调试器触发Error_Handler/HardFault/NMI | PA9/PA5/PA11关闭、PWM停，异常处理不等待HAL tick；不将此视为机械安全关成功 |
| X02 | CPU停机/复位/欠压/失电的硬件测试 | 记录实际输出与机械位置，确认外部默认电平、电源余量和硬件保护能否满足系统安全要求 |

## 限制与本阶段停止点

没有硬件连接，所有上述实机项目均待验收，尤其是PA3电气极性、短脉冲宽度、最坏ISR延迟、机械换向间隔、DRV睡眠/故障电平和行程时限。GPIO/EXTI不是独立于CPU的硬件联锁；很短的临界区和外设传播延迟仍存在，不能承诺零微秒或捕获任意窄脉冲。CPU停止、总线失效、关中断后卡死、时基停止、掉电、机械堵塞都不能靠本状态机保证关到位。

Error_Handler/HardFault/NMI尽力直接关断输出，不发串口、不等待时基，也不能执行完整机械关阀或可靠地持久保存故障。没有新增看门狗、独立硬件关断或储能方案。没有实现软件电流阈值；DRV硬件保护能力取决于既有电路。本阶段不继续开发正式RS485协议、STOP低功耗、电池百分比或RTOS。
