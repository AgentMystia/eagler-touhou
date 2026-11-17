#!/usr/bin/env python3
"""Unicorn 整局 oracle：在原版 th16.exe 代码上走 GameManager 创建（0x42d700 → 0x42cb60，创建全部游戏管理器），
再按原版更新链（0x401460）逐帧推进整个游戏逻辑（Player / Enemy+ECL / Bullet / Item / Gui / Effect / Replay …），
输入由脚本喂进 DAT_004a50b0；导出各管理器的内存快照供移植逐字段比对。

用法：
    game-oracle.py <th16.exe> <dat 解包目录> <frames> <out.json> [--stage N] [--shot N] [--season N] [--difficulty N]
                   [--input "f:held,f:held,..."] [--seed N] [--every N] [--debug]

执行链（原版代码；桩同 anm-oracle.py，另加）：
    0x46d020  AnmManager::load_file(slot, name) → 按需同步装载（0x46cf80 / 0x46d1c0）
    0x401300 / 0x4013b0 不再桩掉：链节点进入 DAT_004a6d94 链管理器，每帧由 0x401460 原样执行
    0x43c5b0  装载线程创建 → 无操作（0x42cb60 由 oracle 同步调用）
    IDirect3DDevice9（DAT_004c10d8）→ 伪 vtable（全部返回 S_OK）；导入表其余项 → 记录并抛错（按需补桩）
"""
import argparse, importlib.util, json, math, pathlib, re, struct, sys

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('anm_oracle', HERE / 'anm-oracle.py')
anm_oracle = importlib.util.module_from_spec(spec)
spec.loader.exec_module(anm_oracle)
Oracle = anm_oracle.Oracle
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EIP

STUB2 = 0x00720000
STUB2_SIZE = 0x4000
PAUSE = 0x00730000

GAME_CREATE, GAME_LOAD, CHAIN_RUN_UPDATE, SCORE_CTOR = 0x42D700, 0x42CB60, 0x401460, 0x4497E0
VM_CTOR = 0x4093F0
ASCII_CTOR, ASCII_INIT, FRONT_INIT, CAMERA_INIT = 0x407C10, 0x407CB0, 0x43AD20, 0x43CB10
DAT_UI_CAMERA, DAT_OVERLAY, DAT_TEXT_BANK = 0x4C10D0, 0x4A6DC8, 0x4C17F8
ANM_LOAD_FILE = 0x46D020
THREAD_CREATE = 0x43C5B0
IAT_VEC3_NORMALIZE = 0x48B264

DAT_CHAIN_MANAGER = 0x4A6D94
DAT_SCORE = 0x4A6F0C
DAT_STAGE_TABLE = 0x4A6F18
DAT_STAGE_NUMBER, DAT_STAGE_COPY = 0x4A5790, 0x4A5794
DAT_SHOT, DAT_SHOT_AUX, DAT_SEASON, DAT_DIFFICULTY = 0x4A57A4, 0x4A57A8, 0x4A57AC, 0x4A57B4
DAT_GAME_FLAGS = 0x4A5BEC
DAT_D3D_DEVICE = 0x4C10D8
DAT_LOADING_VMS = 0x4C128C          # 4 个 VM 指针（0x43b520）
DAT_INPUT_HELD = 0x4A50B0
DAT_GAME_INPUT = 0x4A52C8
DAT_CAMERA_TEMPLATE = 0x4C168C
DAT_BG_COLOR = 0x4C1B0C
DAT_GAME_MANAGER = 0x4A6DD4
DAT_PLAYER = 0x4A6EF8
DAT_STAGE_MANAGER = 0x4A6DA0
DAT_ENEMY_MANAGER = 0x4A6DC0
DAT_GAME_SPEED = 0x4A5788
STAGE_TABLE_BASE, STAGE_TABLE_STRIDE = 0x4A22D0, 0xD4
BANK_ARRAY = 0x184F4F0

# IDirect3DDevice9 vtable：每项 stdcall 栈字节数（含 this）
D3D_DEVICE_METHODS = [
    12, 4, 4, 4, 4, 4, 8, 8, 12, 8, 16, 16, 8, 12, 12, 4, 8, 20, 20, 12, 8, 16, 12, 36, 40, 32, 28, 28, 36, 36,
    20, 12, 12, 12, 24, 16, 28, 12, 12, 8, 8, 4, 4, 28, 12, 12, 12, 8, 8, 8, 8, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 4, 8, 8, 8, 12, 12, 16, 16, 16, 16, 8, 12, 12, 8, 8, 8, 8, 8, 4, 8, 4, 16, 28, 20, 36, 28, 12, 8, 8, 8,
    8, 12, 8, 8, 16, 16, 16, 16, 16, 16, 20, 20, 12, 12, 8, 8, 12, 8, 8, 16, 16, 16, 16, 16, 16, 16, 16, 8, 12,
]


def f32(v):
    return struct.unpack('<f', struct.pack('<f', v))[0]


class GameOracle(Oracle):
    def __init__(self, exe, dat_dir, stage=1, shot=0, season=0, difficulty=1, seed=0, debug=False):
        dat_dir = pathlib.Path(dat_dir)
        super().__init__(exe, 'text.anm', (dat_dir / 'text.anm').read_bytes())
        self.dat_dir = dat_dir
        self.stage, self.shot, self.season, self.difficulty, self.seed = stage, shot, season, difficulty, seed
        self.debug = debug
        self.import_log = []
        self.uc.mem_map(STUB2, STUB2_SIZE)
        self.stub2_count = 0
        # 暂停页：桩需要在模拟器外做“嵌套调用”（如同步装载 ANM）时跳到这里并 emu_stop，由 call() 循环接手
        self.uc.mem_map(PAUSE, 0x1000)
        self.uc.mem_write(PAUSE, b'\xeb\xfe')
        self.pending = None
        self.depth = 0
        # 链登记走原版代码
        del self.replaced[0x401300]
        del self.replaced[0x4013B0]
        self.hooks[ANM_LOAD_FILE] = self.on_anm_load_file
        self.replaced[THREAD_CREATE] = (0, lambda o: self.uc.reg_write(UC_X86_REG_EAX, 0))
        self.replaced[0x479A65] = (0, lambda o: self.uc.reg_write(UC_X86_REG_EAX, 0))  # CRT _chdir（成绩/回放目录切换）
        for addr in (0x477FB2, 0x47BCB2, 0x475150):                                     # CRT free（oracle 堆只增不减）
            self.replaced[addr] = (0, lambda o: None)
        self.replaced[0x45ED00] = (0x0C, lambda o: self.uc.reg_write(UC_X86_REG_EAX, 0))  # 声音线程请求（BGM 装载/播放）thiscall 3 args
        self.replaced[0x4792A8] = (0, self.on_vsprintf)                                  # __stdio_common_vsprintf（CRT 需 FLS/线程数据）
        self.replaced[0x459240] = (0x20, lambda o: None)                                 # GDI 文字 → 纹理像素（无逻辑副作用）
        self.replaced[0x47BCEC] = (0, self.on_malloc)                                    # __malloc_base
        self.replaced[0x47BD3A] = (0, self.on_calloc)                                    # __calloc_base
        self.replaced[0x483CB9] = (0, self.on_realloc)                                   # __realloc_base
        self.setup_imports()
        self.setup_device()
        self.narrow_hooks()
        if debug:
            from unicorn import UC_HOOK_BLOCK
            self.trace = []
            def on_block(uc, address, size, user_data):
                self.trace.append(address)
                if len(self.trace) > 64:
                    del self.trace[:32]
            self.uc.hook_add(UC_HOOK_BLOCK, on_block)

    def call(self, fn, ecx=0, edx=0, args=(), limit=200_000_000):
        """同 Oracle.call，但支持桩在 PAUSE 处挂起：先在模拟器外执行 self.pending（可再嵌套 call），再替桩完成 stdcall 返回。"""
        uc = self.uc
        top = anm_oracle.STACK + anm_oracle.STACK_SIZE - 0x1000 - self.depth * 0x40000
        esp = top - 4 * len(args)
        uc.mem_write(esp, struct.pack('<I', anm_oracle.SENTINEL) + b''.join(struct.pack('<I', a & 0xFFFFFFFF) for a in args))
        uc.reg_write(UC_X86_REG_ESP, esp)
        uc.reg_write(UC_X86_REG_ECX, ecx)
        uc.reg_write(UC_X86_REG_EDX, edx)
        self.errors = [e for e in self.errors if e.startswith('note')]
        eip = fn
        while True:
            try:
                uc.emu_start(eip, anm_oracle.SENTINEL, count=limit)
            except Exception as exc:
                if self.debug:
                    print('trace: ' + ' '.join('%#x' % a for a in self.trace[-24:]), file=sys.stderr)
                raise RuntimeError('call %#x faulted: %s eip=%#x errors=%s' % (
                    fn, exc, uc.reg_read(UC_X86_REG_EIP), self.errors[-4:])) from None
            eip = uc.reg_read(UC_X86_REG_EIP)
            if eip == anm_oracle.SENTINEL:
                return uc.reg_read(UC_X86_REG_EAX)
            if eip == PAUSE and self.pending is not None:
                action, self.pending = self.pending, None
                ctx = uc.context_save()
                self.depth += 1
                try:
                    cleanup, value = action()
                finally:
                    self.depth -= 1
                    uc.context_restore(ctx)
                esp = uc.reg_read(UC_X86_REG_ESP)
                ret = struct.unpack('<I', uc.mem_read(esp, 4))[0]
                uc.reg_write(UC_X86_REG_ESP, esp + 4 + cleanup)
                uc.reg_write(UC_X86_REG_EAX, value)
                eip = ret
                continue
            if self.debug:
                print('trace: ' + ' '.join('%#x' % a for a in self.trace[-24:]), file=sys.stderr)
            raise RuntimeError('call %#x did not return: eip=%#x errors=%s' % (fn, eip, self.errors))

    def on_vsprintf(self, o):
        # __stdio_common_vsprintf(options u64, buf, count, fmt, locale, va_list) cdecl → 写入 buf，返回长度
        buf, count, fmt_p, va = self.arg(2), self.arg(3), self.arg(4), self.arg(6)
        fmt = self.cstr(fmt_p)
        out = []
        i = 0
        while i < len(fmt):
            ch = fmt[i]
            if ch != '%':
                out.append(ch)
                i += 1
                continue
            m = re.match(r'%([-+ 0#]*)(\d*)(?:\.(\d+))?(hh|h|ll|l|L)?([diouxXcsfeEgG%])', fmt[i:])
            if not m:
                out.append(ch)
                i += 1
                continue
            flags, width, prec, length, conv = m.groups()
            i += m.end()
            if conv == '%':
                out.append('%')
                continue
            spec = '%' + flags + width + ('.' + prec if prec is not None else '')
            if conv in 'fFeEgG':
                value = struct.unpack('<d', self.uc.mem_read(va, 8))[0]
                va += 8
                out.append((spec + conv) % value)
            elif conv == 's':
                p = self.u32(va)
                va += 4
                out.append((spec + 's') % (self.cstr(p) if p else '(null)'))
            elif conv == 'c':
                out.append((spec + 'c') % chr(self.u32(va) & 0xFF))
                va += 4
            else:
                if length == 'll':
                    raw = self.uc.mem_read(va, 8)
                    va += 8
                    value = struct.unpack('<q' if conv in 'di' else '<Q', raw)[0]
                else:
                    raw = self.uc.mem_read(va, 4)
                    va += 4
                    value = struct.unpack('<i' if conv in 'di' else '<I', raw)[0]
                out.append((spec + (conv if conv != 'u' else 'd')) % value)
        text = ''.join(out).encode('cp932', errors='replace')
        if count and len(text) >= count:
            text = text[:count - 1]
        self.uc.mem_write(buf, text + b'\0')
        self.uc.reg_write(UC_X86_REG_EAX, len(text))

    def on_calloc(self, o):
        n = self.arg(0) * self.arg(1)
        p = self.alloc(max(n, 1))
        self.uc.mem_write(p, b'\0' * n)
        self.uc.reg_write(UC_X86_REG_EAX, p)

    def on_realloc(self, o):
        old, size = self.arg(0), self.arg(1)
        p = self.alloc(max(size, 1))
        if old:
            # 旧块大小未知：拷贝到新块大小为止（oracle 堆块连续，越界读仍在映射内）
            self.uc.mem_write(p, bytes(self.uc.mem_read(old, size)))
        self.uc.reg_write(UC_X86_REG_EAX, p)

    def cstring(self, text):
        p = self.alloc(len(text) + 1)
        self.uc.mem_write(p, text.encode('ascii') + b'\0')
        return p

    def pause(self, action):
        self.pending = action
        self.uc.reg_write(UC_X86_REG_EIP, PAUSE)
        self.uc.emu_stop()

    def narrow_hooks(self):
        """整局模拟指令量大，逐指令 Python 钩子不可接受：把镜像内的替换函数入口 / 钩子入口改写为 JMP 到桩区，
        跳过点改写为 JMP，代码钩子只覆盖桩区（镜像代码以原生 JIT 速度执行）。"""
        uc = self.uc
        uc.hook_del(self.code_hook)
        def jmp(src, dst):
            uc.mem_write(src, b'\xe9' + struct.pack('<i', dst - (src + 5)))
        for addr, (cleanup, handler) in list(self.replaced.items()):
            def run(o, cleanup=cleanup, handler=handler):
                handler(o)
                esp = uc.reg_read(UC_X86_REG_ESP)
                ret = struct.unpack('<I', uc.mem_read(esp, 4))[0]
                uc.reg_write(UC_X86_REG_ESP, esp + 4 + cleanup)
                uc.reg_write(UC_X86_REG_EIP, ret)
            jmp(addr, self.make_stub2(0, run))
        for addr, fn in list(self.hooks.items()):
            if addr < anm_oracle.STUB:
                jmp(addr, self.make_stub2(0, fn))
        for src, dst in self.skips.items():
            jmp(src, dst)
        self.replaced = {}
        self.skips = {}
        for base, size in ((anm_oracle.STUB, 0x1000), (STUB2, STUB2_SIZE)):
            uc.hook_add(UC_HOOK_CODE, self.on_code, begin=base, end=base + size - 1)

    # ---- 桩区 2 ----
    def make_stub2(self, ret_bytes, fn):
        addr = STUB2 + self.stub2_count * 16
        self.stub2_count += 1
        if self.stub2_count * 16 > STUB2_SIZE:
            raise RuntimeError('stub2 exhausted')
        self.uc.mem_write(addr, bytes([0xC2]) + struct.pack('<H', ret_bytes) if ret_bytes else bytes([0xC3]))
        self.hooks[addr] = fn
        return addr

    def setup_imports(self):
        imports = json.loads((HERE.parent.parent.parent.parent / 'artifacts/th16/decomp/jp/imports.json').read_text())
        known = {
            # name: (stack bytes, handler)
            'Sleep': (4, lambda o: None),
            'EnterCriticalSection': (4, lambda o: None),
            'LeaveCriticalSection': (4, lambda o: None),
            'timeGetTime': (0, lambda o: self.uc.reg_write(UC_X86_REG_EAX, 0x1000)),
            'GetTickCount': (0, lambda o: self.uc.reg_write(UC_X86_REG_EAX, 0x1000)),
            'SetCurrentDirectoryA': (4, lambda o: self.uc.reg_write(UC_X86_REG_EAX, 1)),
            'GetCurrentDirectoryA': (8, lambda o: self.uc.reg_write(UC_X86_REG_EAX, 0)),
            'D3DXVec3Normalize': (8, self.on_vec3_normalize),
            'D3DXVec2Normalize': (8, self.on_vec2_normalize),
            'D3DXMatrixLookAtLH': (16, self.on_matrix_look_at),
            'D3DXMatrixPerspectiveFovLH': (20, self.on_matrix_perspective),
            'D3DXMatrixRotationX': (8, lambda o: self.on_matrix_rotation(0)),
            'D3DXMatrixRotationY': (8, lambda o: self.on_matrix_rotation(1)),
            'D3DXMatrixRotationZ': (8, lambda o: self.on_matrix_rotation(2)),
            'D3DXMatrixTranslation': (16, self.on_matrix_translation),
            'D3DXMatrixMultiply': (12, self.on_matrix_multiply),
            'D3DXVec3Transform': (12, self.on_vec3_transform),
            'D3DXVec3Project': (24, self.on_vec3_project),
        }
        for imp in imports:
            name = imp['name']
            slot = int(imp['references'][0], 16)
            if name in known:
                n, fn = known[name]
                addr = self.make_stub2(n, fn)
            else:
                addr = self.make_stub2(0, (lambda nm: (lambda o: self.on_unknown_import(nm)))(name))
            self.uc.mem_write(slot, struct.pack('<I', addr))

    def on_unknown_import(self, name):
        eip_ret = struct.unpack('<I', self.uc.mem_read(self.uc.reg_read(UC_X86_REG_ESP), 4))[0]
        raise RuntimeError('unstubbed import %s called from %#x' % (name, eip_ret))

    def setup_device(self):
        vt = self.alloc(len(D3D_DEVICE_METHODS) * 4)
        for i, n in enumerate(D3D_DEVICE_METHODS):
            addr = self.make_stub2(n, (lambda idx: (lambda o: self.on_device_method(idx)))(i))
            self.uc.mem_write(vt + i * 4, struct.pack('<I', addr))
        dev = self.alloc(0x10)
        self.uc.mem_write(dev, struct.pack('<I', vt))
        self.uc.mem_write(DAT_D3D_DEVICE, struct.pack('<I', dev))
        self.device_calls = {}

    def on_device_method(self, idx):
        self.device_calls[idx] = self.device_calls.get(idx, 0) + 1
        self.uc.reg_write(UC_X86_REG_EAX, 0)

    # ---- D3DX 数学（float32 口径；仅供绘制矩阵，不参与游戏逻辑状态）----
    def read_f(self, addr, n):
        return list(struct.unpack('<%df' % n, self.uc.mem_read(addr, 4 * n)))

    def write_f(self, addr, values):
        self.uc.mem_write(addr, struct.pack('<%df' % len(values), *values))

    def on_vec3_normalize(self, o):
        out, src = self.arg(0), self.arg(1)
        x, y, z = self.read_f(src, 3)
        length = f32(math.sqrt(f32(f32(f32(x * x) + f32(y * y)) + f32(z * z))))
        if length != 0.0:
            x, y, z = f32(x / length), f32(y / length), f32(z / length)
        self.write_f(out, [x, y, z])
        self.uc.reg_write(UC_X86_REG_EAX, out)

    def on_vec2_normalize(self, o):
        out, src = self.arg(0), self.arg(1)
        x, y = self.read_f(src, 2)
        length = f32(math.sqrt(f32(f32(x * x) + f32(y * y))))
        if length != 0.0:
            x, y = f32(x / length), f32(y / length)
        self.write_f(out, [x, y])
        self.uc.reg_write(UC_X86_REG_EAX, out)

    @staticmethod
    def _norm3(v):
        l = f32(math.sqrt(f32(f32(f32(v[0] * v[0]) + f32(v[1] * v[1])) + f32(v[2] * v[2]))))
        return [f32(c / l) for c in v] if l else list(v)

    @staticmethod
    def _cross(a, b):
        return [f32(f32(a[1] * b[2]) - f32(a[2] * b[1])), f32(f32(a[2] * b[0]) - f32(a[0] * b[2])), f32(f32(a[0] * b[1]) - f32(a[1] * b[0]))]

    @staticmethod
    def _dot(a, b):
        return f32(f32(f32(a[0] * b[0]) + f32(a[1] * b[1])) + f32(a[2] * b[2]))

    def on_matrix_look_at(self, o):
        out, eye_p, at_p, up_p = self.arg(0), self.arg(1), self.arg(2), self.arg(3)
        eye, at, up = self.read_f(eye_p, 3), self.read_f(at_p, 3), self.read_f(up_p, 3)
        z = self._norm3([f32(at[i] - eye[i]) for i in range(3)])
        x = self._norm3(self._cross(up, z))
        y = self._cross(z, x)
        m = [x[0], y[0], z[0], 0.0, x[1], y[1], z[1], 0.0, x[2], y[2], z[2], 0.0,
             f32(-self._dot(x, eye)), f32(-self._dot(y, eye)), f32(-self._dot(z, eye)), 1.0]
        self.write_f(out, m)
        self.uc.reg_write(UC_X86_REG_EAX, out)

    def on_matrix_perspective(self, o):
        out = self.arg(0)
        fovy, aspect, zn, zf = (struct.unpack('<f', struct.pack('<I', self.arg(i)))[0] for i in range(1, 5))
        ys = f32(1.0 / math.tan(fovy * 0.5))
        xs = f32(ys / aspect)
        q = f32(zf / f32(zf - zn))
        m = [xs, 0, 0, 0, 0, ys, 0, 0, 0, 0, q, 1.0, 0, 0, f32(-zn * q), 0]
        self.write_f(out, m)
        self.uc.reg_write(UC_X86_REG_EAX, out)

    def on_matrix_rotation(self, axis):
        out = self.arg(0)
        a = struct.unpack('<f', struct.pack('<I', self.arg(1)))[0]
        c, s = f32(math.cos(a)), f32(math.sin(a))
        m = [1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0]
        if axis == 0:
            m[5], m[6], m[9], m[10] = c, s, -s, c
        elif axis == 1:
            m[0], m[2], m[8], m[10] = c, -s, s, c
        else:
            m[0], m[1], m[4], m[5] = c, s, -s, c
        self.write_f(out, m)
        self.uc.reg_write(UC_X86_REG_EAX, out)

    def on_matrix_translation(self, o):
        out = self.arg(0)
        x, y, z = (struct.unpack('<f', struct.pack('<I', self.arg(i)))[0] for i in range(1, 4))
        self.write_f(out, [1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0, 0, x, y, z, 1.0])
        self.uc.reg_write(UC_X86_REG_EAX, out)

    def on_matrix_multiply(self, o):
        out, a_p, b_p = self.arg(0), self.arg(1), self.arg(2)
        a, b = self.read_f(a_p, 16), self.read_f(b_p, 16)
        m = []
        for r in range(4):
            for c in range(4):
                acc = 0.0
                for k in range(4):
                    acc = f32(acc + f32(a[r * 4 + k] * b[k * 4 + c]))
                m.append(acc)
        self.write_f(out, m)
        self.uc.reg_write(UC_X86_REG_EAX, out)

    def on_vec3_transform(self, o):
        out, v_p, m_p = self.arg(0), self.arg(1), self.arg(2)
        v, m = self.read_f(v_p, 3) + [1.0], self.read_f(m_p, 16)
        r = []
        for c in range(4):
            acc = 0.0
            for k in range(4):
                acc = f32(acc + f32(v[k] * m[k * 4 + c]))
            r.append(acc)
        self.write_f(out, r)
        self.uc.reg_write(UC_X86_REG_EAX, out)

    def on_vec3_project(self, o):
        out, v_p, vp_p, proj_p, view_p, world_p = (self.arg(i) for i in range(6))
        v = self.read_f(v_p, 3) + [1.0]
        mats = [self.read_f(p, 16) for p in (world_p, view_p, proj_p) if p]
        for m in mats:
            r = []
            for c in range(4):
                acc = 0.0
                for k in range(4):
                    acc = f32(acc + f32(v[k] * m[k * 4 + c]))
                r.append(acc)
            v = r
        w = v[3] if v[3] != 0.0 else 1.0
        x, y, z = f32(v[0] / w), f32(v[1] / w), f32(v[2] / w)
        vx, vy, vw, vh = struct.unpack('<4I', self.uc.mem_read(vp_p, 16))
        minz, maxz = struct.unpack('<2f', self.uc.mem_read(vp_p + 16, 8))
        sx = f32(f32(f32(x + 1.0) * 0.5) * vw + vx)
        sy = f32(f32(f32(1.0 - y) * 0.5) * vh + vy)
        sz = f32(f32(z * f32(maxz - minz)) + minz)
        self.write_f(out, [sx, sy, sz])
        self.uc.reg_write(UC_X86_REG_EAX, out)

    # ---- 文件 ----
    def on_read_file(self, o):
        name_ptr = self.uc.reg_read(UC_X86_REG_ECX)
        size_ptr = self.uc.reg_read(UC_X86_REG_EDX)
        name = self.cstr(name_ptr)
        self.read_calls += 1
        if name not in self.files:
            path = self.dat_dir / name
            if path.exists():
                self.files[name] = path.read_bytes()
            else:
                self.errors.append('note: read of %s → not found' % name)
                if size_ptr:
                    self.uc.mem_write(size_ptr, struct.pack('<I', 0))
                self.uc.reg_write(UC_X86_REG_EAX, 0)
                return
        data = self.files[name]
        p = self.alloc(len(data) + 16)
        self.uc.mem_write(p, data)
        if size_ptr:
            self.uc.mem_write(size_ptr, struct.pack('<I', len(data)))
        self.uc.reg_write(UC_X86_REG_EAX, p)

    def on_anm_load_file(self, o):
        # 0x46d020 入口钩子：槽已装 → 直接返回；否则挂起，在模拟器外同步跑装载线程工作步（0x46cf80 / 0x46d1c0）
        uc = self.uc
        slot = self.arg(0)
        name = self.cstr(self.arg(1))
        current = self.u32(self.manager + BANK_ARRAY + slot * 4)
        if current:
            esp = uc.reg_read(UC_X86_REG_ESP)
            ret = struct.unpack('<I', uc.mem_read(esp, 4))[0]
            uc.reg_write(UC_X86_REG_ESP, esp + 4 + 8)
            uc.reg_write(UC_X86_REG_EAX, current)
            uc.reg_write(UC_X86_REG_EIP, ret)
            return
        def load():
            if self.debug:
                print('anm load slot %d ← %s' % (slot, name), file=sys.stderr)
            if name not in self.files:
                path = self.dat_dir / name
                if not path.exists():
                    raise RuntimeError('ANM %s missing from dat dir' % name)
                self.files[name] = path.read_bytes()
            self.load_file(slot, name)
            self.loaded_banks[slot] = name
            return 8, self.u32(self.manager + BANK_ARRAY + slot * 4)
        self.pause(load)

    # ---- 启动 ----
    def boot(self):
        uc = self.uc
        self.loaded_banks = {}
        # 链管理器 DAT_004a6d94（0x401300 以 +0x14 为更新链哨兵、+0x18 首节点；0x401460 以 +0x50/+0x54 为游标/暂停）
        self.chain_manager = self.alloc(0x100)
        uc.mem_write(self.chain_manager, b'\0' * 0x100)
        uc.mem_write(DAT_CHAIN_MANAGER, struct.pack('<I', self.chain_manager))
        self.manager = self.alloc(0x1C7FD90)
        self.call(anm_oracle.MANAGER_CTOR, ecx=self.manager)
        uc.mem_write(anm_oracle.DAT_MANAGER, struct.pack('<I', self.manager))
        uc.mem_write(anm_oracle.DAT_BACKBUFFER_W, struct.pack('<IIf', 640, 480, 1.0))
        # 0x43b520（Supervisor 初始化）：清色、相机 0x43cb10、DAT_004a6dc8（0x88）、4 个装载幕 VM
        uc.mem_write(DAT_GAME_SPEED, struct.pack('<f', 1.0))
        uc.mem_write(DAT_BG_COLOR, struct.pack('<I', 0xFF000000))
        self.call(CAMERA_INIT, ecx=DAT_UI_CAMERA)
        overlay = self.alloc(0x88)
        uc.mem_write(overlay, b'\0' * 0x88)
        uc.mem_write(overlay, struct.pack('<I', 2))
        uc.mem_write(DAT_OVERLAY, struct.pack('<I', overlay))
        for i in range(4):
            vm = self.alloc(anm_oracle.VM_SIZE)
            self.call(VM_CTOR, ecx=vm)
            uc.mem_write(DAT_LOADING_VMS + i * 4, struct.pack('<I', vm))
        # 0x43adc0：AsciiManager（0x407c10 / 0x407cb0 → ascii.anm 槽 2）、text.anm 槽 0、0x43ad20（front.anm 槽 5 +
        # EffectManager 0x418a30 → effect.anm 槽 8 / bullet.anm 槽 7）、成绩数据 0x4497e0（scoreth16.dat 缺失 → 默认值）
        self.ascii_manager = self.alloc(0x19254)
        self.call(ASCII_CTOR, ecx=self.ascii_manager)
        self.call(ASCII_INIT, ecx=self.ascii_manager)
        uc.mem_write(DAT_TEXT_BANK, struct.pack('<I', self.call(ANM_LOAD_FILE, args=(0, self.cstring('text.anm')))))
        if self.call(FRONT_INIT) != 0:
            raise RuntimeError('0x43ad20 failed: %s' % self.errors[-4:])
        self.score = self.alloc(0x1A3AC)
        self.call(SCORE_CTOR, ecx=self.score)
        uc.mem_write(DAT_SCORE, struct.pack('<I', self.score))
        # 标题 → 游戏的全局（0x43ce10 / 标题菜单）
        uc.mem_write(DAT_STAGE_NUMBER, struct.pack('<I', self.stage))
        uc.mem_write(DAT_STAGE_COPY, struct.pack('<I', self.stage))
        uc.mem_write(DAT_SHOT, struct.pack('<I', self.shot))
        uc.mem_write(DAT_SHOT_AUX, struct.pack('<I', 0))
        uc.mem_write(DAT_SEASON, struct.pack('<I', self.season))
        uc.mem_write(DAT_DIFFICULTY, struct.pack('<I', self.difficulty))
        uc.mem_write(DAT_STAGE_TABLE, struct.pack('<I', STAGE_TABLE_BASE + self.stage * STAGE_TABLE_STRIDE))
        uc.mem_write(DAT_GAME_FLAGS, struct.pack('<I', 0))
        # 0x43ce10 case 7（标题 → 游戏）：Supervisor(0x4c10d0)+0x700 = 1（新开局：0x42cb60 据此创建 Player）
        uc.mem_write(DAT_UI_CAMERA + 0x700, struct.pack('<I', 1))
        # GameManager 创建 + 装载（同步）
        self.game_manager = self.call(GAME_CREATE, ecx=0)
        if self.u32(DAT_GAME_MANAGER) != self.game_manager:
            raise RuntimeError('DAT_004a6dd4 not set')
        rc = self.call(GAME_LOAD)
        if rc != 0:
            raise RuntimeError('0x42cb60 failed rc=%#x errors=%s' % (rc, self.errors[-6:]))
        self.frame = 0

    # ---- 帧 ----
    def set_input(self, held):
        prev = self.u32(DAT_INPUT_HELD)
        self.uc.mem_write(DAT_INPUT_HELD, struct.pack('<II', held, prev))

    def tick(self):
        # 更新链含 AnmManager 自身的世界 tick（优先级 9，0x46e740），无需另行调用 0x46e340
        self.call(CHAIN_RUN_UPDATE, ecx=self.chain_manager)
        self.frame += 1

    def chain_list(self):
        out = []
        node = self.u32(self.chain_manager + 0x18)
        while node:
            item = self.u32(node)
            out.append((struct.unpack('<i', self.uc.mem_read(item, 4))[0], self.u32(item + 8), self.u32(item + 0x24)))
            node = self.u32(node + 4)
        return out

    def i32(self, addr):
        return struct.unpack('<i', self.uc.mem_read(addr, 4))[0]

    def snapshot(self):
        player = self.u32(DAT_PLAYER)
        sm = self.u32(DAT_STAGE_MANAGER)
        snap = {
            'frame': self.frame,
            'game_frame': self.i32(0x4A579C),
            'stage_frame': self.i32(0x4A57A0),
            'input': [self.u32(DAT_GAME_INPUT), self.u32(DAT_GAME_INPUT + 0xC)],
            'rng': self.u32(0x4A52E8) if False else None,
        }
        if player:
            snap['player'] = {
                'pos': [self.u32(player + 0x610 + 4 * i) for i in range(3)],
                'pos_fixed': [self.i32(player + 0x61C), self.i32(player + 0x620)],
                'state': self.i32(player + 0x165A8),
                'timer': self.i32(player + 0x628),
                'focus': self.i32(player + 0x165C8),
                'dir': self.i32(player + 0x2C780),
                'vm': bytes(self.uc.mem_read(player + 0x14, 0x568)).hex(),
            }
        if sm:
            snap['stage'] = {'frame': self.i32(sm + 0x66E0), 'ins': self.i32(sm + 0x20), 'flags': self.u32(sm + 0x66C4)}
        return snap


def parse_input(spec):
    events = {}
    if spec:
        for part in spec.split(','):
            f, v = part.split(':')
            events[int(f)] = int(v, 0)
    return events


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('dat_dir')
    ap.add_argument('frames', type=int)
    ap.add_argument('out')
    ap.add_argument('--stage', type=int, default=1)
    ap.add_argument('--shot', type=int, default=0)
    ap.add_argument('--season', type=int, default=0)
    ap.add_argument('--difficulty', type=int, default=1)
    ap.add_argument('--input', default='')
    ap.add_argument('--seed', type=int, default=0)
    ap.add_argument('--every', type=int, default=1)
    ap.add_argument('--debug', action='store_true')
    a = ap.parse_args()
    o = GameOracle(pathlib.Path(a.exe), a.dat_dir, a.stage, a.shot, a.season, a.difficulty, a.seed, a.debug)
    o.boot()
    print('booted: chains=%s banks=%s device_calls=%s' % (
        [(p, hex(cb)) for p, cb, _ in o.chain_list()], o.loaded_banks, o.device_calls), file=sys.stderr)
    events = parse_input(a.input)
    held = 0
    frames = []
    for f in range(a.frames):
        if f in events:
            held = events[f]
        o.set_input(held)
        o.tick()
        if (f + 1) % a.every == 0:
            frames.append(o.snapshot())
    notes = [e for e in o.errors if not e.startswith('note')]
    pathlib.Path(a.out).write_text(json.dumps({'stage': a.stage, 'shot': a.shot, 'season': a.season, 'difficulty': a.difficulty,
                                               'input': a.input, 'frames': frames}, separators=(',', ':')))
    print('game-oracle: %d frames → %s errors=%s' % (a.frames, a.out, notes[:3]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
