"""Execute real C tests on a host Cortex-M0+ emulator (no board required).

Set ARM_CC and ARM_NM if not on PATH. Install unicorn==2.1.4 with pip,
or set PHASE1_TEST_DEPS to a directory containing that package.
Build outputs go to a fresh temporary directory. No firmware is flashed.
"""
from pathlib import Path
import os
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
deps = os.environ.get('PHASE1_TEST_DEPS', str(ROOT.parent / 'tmp/phase1-testdeps'))
sys.path.insert(0, deps)
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS
from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0, UC_ARM_REG_PC

def compiler():
    found = os.environ.get('ARM_CC') or shutil.which('arm-none-eabi-gcc')
    if not found:
        candidates = list(Path('D:/chen/STM32CubeIDE_2.2.0').glob(
            'STM32CubeIDE/plugins/*gnu-tools*/tools/bin/arm-none-eabi-gcc.exe'))
        found = str(candidates[0]) if candidates else None
    if not found:
        raise SystemExit('Set ARM_CC to arm-none-eabi-gcc')
    return found

def execute(name, sources, include_dirs=(), defines=(), libs=('-lgcc',)):
    cc = compiler()
    nm = os.environ.get('ARM_NM') or str(Path(cc).with_name('arm-none-eabi-nm.exe' if os.name == 'nt' else 'arm-none-eabi-nm'))
    with tempfile.TemporaryDirectory(prefix='valve_phase1_') as temp:
        elf = Path(temp) / (name + '.elf')
        cmd = [cc, '-mcpu=cortex-m0plus', '-mthumb', '-std=c11', '-O1',
               '-g', '-Wall', '-Wextra', '-Werror', '-ffreestanding', '-fno-builtin',
               '-ffunction-sections', '-fdata-sections', '-nostdlib',
               '-Wl,-Ttext=0x10000', '-Wl,--section-start=.data=0x20000000',
               '-Wl,--section-start=.bss=0x20001000', '-Wl,--gc-sections',
               '-Wl,-e,run_tests']
        cmd += [f'-I{ROOT / p}' for p in include_dirs]
        cmd += [f'-D{d}' for d in defines]
        cmd += [str(ROOT / p) for p in sources]
        subprocess.run(cmd + list(libs) + ['-o', str(elf)], check=True)
        symbols = {}
        for line in subprocess.check_output([nm, str(elf)], text=True).splitlines():
            parts = line.split()
            if len(parts) == 3:
                symbols[parts[2]] = int(parts[0], 16)
        data = elf.read_bytes()
        header = struct.unpack_from('<16sHHIIIIIHHHHHH', data)
        cpu = Uc(UC_ARCH_ARM, UC_MODE_THUMB | UC_MODE_MCLASS)
        cpu.mem_map(0x10000, 0x80000)
        cpu.mem_map(0x20000000, 0x40000)
        for i in range(header[10]):
            kind, offset, address, _, size, _, _, _ = struct.unpack_from(
                '<IIIIIIII', data, header[5] + i * header[9])
            if kind == 1 and size:
                cpu.mem_write(address, data[offset:offset + size])
        sentinel = 0x8FFF0
        cpu.reg_write(UC_ARM_REG_SP, 0x2003FFF0)
        cpu.reg_write(UC_ARM_REG_LR, sentinel | 1)
        cpu.emu_start(symbols['run_tests'] | 1, sentinel, count=20000000)
        result = cpu.reg_read(UC_ARM_REG_R0)
        if cpu.reg_read(UC_ARM_REG_PC) != sentinel:
            raise AssertionError(f'{name}: did not return (instruction limit)')
        if result:
            if 'response' in symbols:
                print('Captured response:', bytes(cpu.mem_read(symbols['response'], 1024)).split(b'\0')[0].decode(errors='replace'))
            raise AssertionError(f'{name}: C assertion failed at source line {result}')
        cases = struct.unpack('<I', cpu.mem_read(symbols['tests_passed'], 4))[0]
        print(f'PASS {name}: {cases} scenarios')

if __name__ == '__main__':
    execute('power_manager_low', ['Core/Src/power_manager.c','tests/test_power_manager.c'], ['Core/Inc'], ['POWER_LOW_POWER_MODE=1'])
    execute('power_manager_debug', ['Core/Src/power_manager.c','tests/test_power_manager.c'], ['Core/Inc'])
    execute('power_port', ['Core/Src/power_port.c','tests/test_power_port.c'], ['tests/mocks','Core/Inc'], ['POWER_LOW_POWER_MODE=1'])
    execute('battery', ['Core/Src/battery_monitor.c', 'tests/test_battery.c'], ['Core/Inc'])
    execute('battery_port', ['Core/Src/battery_adc_port.c', 'tests/test_battery_port.c'], ['tests/mocks','Core/Inc'])
    execute('snapshot', ['Core/Src/app_status.c','tests/test_app_status.c'], ['Core/Inc'])
    execute('rs485_rx', ['Core/Src/rs485_rx.c', 'tests/test_rs485_rx.c'], ['tests/mocks', 'Core/Inc'])
    execute('controller', ['Core/Src/valve_control.c', 'tests/test_valve_control.c'], ['Core/Inc'])
    execute('port', ['Core/Src/valve_port.c', 'tests/test_valve_port.c'], ['tests/mocks', 'Core/Inc'])
    app_sources=['Core/Src/power_manager.c','Core/Src/battery_monitor.c','Core/Src/battery_adc_port.c',
                 'Core/Src/app_status.c','Core/Src/rs485_rx.c','Core/Src/valve_control.c',
                 'Core/Src/valve_port.c','tests/test_valve_app.c','tests/test_runtime.c']
    execute('application_debug', app_sources, ['tests/mocks','Core/Inc'], libs=('-lc','-lgcc'))
    execute('application_low', app_sources, ['tests/mocks','Core/Inc'], ['POWER_LOW_POWER_MODE=1'], libs=('-lc','-lgcc'))
