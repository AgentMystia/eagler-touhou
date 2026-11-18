#!/usr/bin/env python3
"""Unicorn 整局 oracle：在原版 th16.exe 代码上走 GameManager 创建（0x42d700 → 0x42cb60，创建全部游戏管理器），
再按原版更新链（0x401460）逐帧推进整个游戏逻辑（Player / Enemy+ECL / Bullet / Item / Gui / Effect / Replay …），
输入由脚本喂进 DAT_004a50b0；导出各管理器的内存快照供移植逐字段比对。

用法：
    game-oracle.py <th16.exe> <dat 解包目录> <frames> <out.json> [--stage N] [--shot N] [--season N] [--difficulty N]
                   [--input "f:held,f:held,..."] [--seed N] [--every N] [--lives N] [--debug] [--draw]
    game-oracle.py --digest <out.json>...        → 已有快照的 Player 规范字段 FNV-1a 64（harness game-digest 同口径）

快照（每帧）：frame / game_frame / stage_frame / input [held, pressed] / power / season_gauge / enemy_count / dialogue /
    player{state, pos, pos_fixed, timers×3, dir, last_move, focus, focus_vm, speeds, velocity, option_lag, counter, speed_mult,
           ext_velocity, option_count, power_level, sub_count, flags, invincible, shot_timers, rects, radii, rects2, scale,
           options×12, shot_misc, cooldown×16, last_damage_index, bullets[活动自机弹], damage_sources[活动伤害源],
           sfx[Player 代码段登记的音效], vm(0x568 字节)} / stage{frame, ins, flags}
    --enemies → enemies{count, enemies[...]}；--bullets → bullets{count, bullets[...]}；
    --items → items{active, spawned, point_counter, scale, items[...]} + resources{score, lives, bombs, point_value, …}
    --lasers → lasers{count, next_handle, lasers[...]}；--bombs → bombs{main, sub}（Bomb / 季节解放对象，含灵梦阴阳玉）
    digest 里 VM id 只取有无（池分配顺序取决于未移植的管理器）；enemy_count / dialogue 不进 digest，供 harness 的 PlayerWorld 桩。

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
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP

STUB2 = 0x00720000
STUB2_SIZE = 0x4000
PAUSE = 0x00730000

GAME_CREATE, GAME_LOAD, CHAIN_RUN_UPDATE, SCORE_CTOR = 0x42D700, 0x42CB60, 0x401460, 0x4497E0
GAME_DESTROY = 0x42D780  # 0x43ce10 case 0xc：销毁 GameManager（0x42d200）后按原模式重建进入下一关
DAT_SCENE_CURRENT, DAT_SCENE_REQUEST = 0x4C17C0, 0x4C17C4  # Supervisor+0x6f0 / +0x6f4
CHAIN_RUN_DRAW = 0x4015A0
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
INPUT_EDGES = 0x405580              # 主循环 0x401d50 末：从 held/previous 推出 pressed（+0xc）/ repeat（+0x8）——GameMenu 读这组
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
    def __init__(self, exe, dat_dir, stage=1, shot=0, season=0, difficulty=1, seed=0, debug=False, replay_slot=-1,
                 lives=None, mode=0):
        dat_dir = pathlib.Path(dat_dir)
        self.mode = mode   # DAT_004a5bec bit4-5：0 普通 / 1 练习 / 2 符卡练习（0x449190）
        super().__init__(exe, 'text.anm', (dat_dir / 'text.anm').read_bytes())
        self.dat_dir = dat_dir
        self.stage, self.shot, self.season, self.difficulty, self.seed = stage, shot, season, difficulty, seed
        self.lives = lives
        self.replay_slot = replay_slot
        self.debug = debug
        self.enemy_snap = False  # --enemies：每帧敌人快照（不进 digest，Enemy 移植阶段的对照）
        self.bullet_snap = False # --bullets：每帧敌弹快照（BulletManager 链表）
        self.item_snap = False   # --items：每帧道具快照（ItemManager 池）与得分/残机等资源全局
        self.laser_snap = False  # --lasers：每帧激光快照（LaserManager 链表）
        self.bomb_snap = False   # --bombs：每帧 Bomb / 季节解放对象快照（DAT_004a6da8 / DAT_004a6da4）
        self.ecl_trace = None
        self.frame = -1  # 装载阶段（GAME_LOAD 之前）的 RNG 调用记为 -1 帧
        self.run_draw = False   # --draw：也跑绘制链（需 D3DXVec3ProjectArray 等桩，待渲染/裁剪阶段接入）
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
        self.replaced[0x45E1F0] = (4, self.on_sfx)                                       # 音效登记（DAT_004d9e2c 队列；RET 4）→ 只记录 (id, 调用方)
        self.replaced[0x45E150] = (8, self.on_sfx)                                       # 无声像变体（被弹 0x443f10、弹幕 Ex 音效；RET 8）
        self.sfx_log = []
        self.replaced[0x4792A8] = (0, self.on_vsprintf)                                  # __stdio_common_vsprintf（CRT 需 FLS/线程数据）
        self.replaced[0x459240] = (0x20, lambda o: None)                                 # GDI 文字 → 纹理像素（无逻辑副作用）
        # 0x43ef20 暂停/续关背景截屏：text.anm 0x34 VM 照常生成（占 VM 池、计入 spawn 计数），只跳过
        # GetSurfaceLevel / D3DXLoadSurfaceFromSurface / Release 这段纯 D3D 搬运（0x43f088 → 0x43f0df）
        self.skips[0x43F088] = 0x43F0DF
        self.replaced[0x471BD0] = (0, lambda o: None)                                    # BGM 播放位置回绕（DAT_004df470 在 oracle 中为空；0x43f350 续关画面）
        self.replaced[0x471C90] = (0, lambda o: None)                                    # BGM 缓冲重填（同上为空；0x42d7b0 换关 / 0x43f980 续关恢复）
        self.replaced[0x449A00] = (0, lambda o: self.uc.reg_write(UC_X86_REG_EAX, 0))    # 成绩文件写盘（0x42d200 换关 / 退出；只改成绩缓冲与校验和，无游戏状态副作用）
        self.replaced[0x47A09E] = (0, self.on_time64)                                    # CRT __time64（0x43e250 入榜记录时刻；CRT 内部经 DecodePointer 取时钟，oracle 无此环境）
        self.replaced[0x47BCEC] = (0, self.on_malloc)                                    # __malloc_base
        self.replaced[0x47BD3A] = (0, self.on_calloc)                                    # __calloc_base
        self.replaced[0x483CB9] = (0, self.on_realloc)                                   # __realloc_base
        self.setup_imports()
        self.setup_device()
        self.setup_laser_sound_buffer()
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

    def on_time64(self, o):
        # __time64(time_t* out) cdecl：固定返回 0（移植版 insert_rank 也传 0；宿主时钟在会话层注入），结果写入 out 与 EDX:EAX
        out = self.arg(0)
        if out != 0:
            self.uc.mem_write(out, b'\0' * 8)
        self.uc.reg_write(UC_X86_REG_EAX, 0)
        self.uc.reg_write(UC_X86_REG_EDX, 0)

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
    def splice_hook(self, addr, displaced, fn):
        """函数中段观察钩子：addr 处原指令（displaced 字节，须为完整指令且不含相对跳转）搬到桩区，
        前面挂 Python 钩子，后面 JMP 回 addr+len(displaced)。钩子只读寄存器，不改控制流。"""
        uc = self.uc
        raw = bytes(uc.mem_read(addr, len(displaced)))
        if raw != displaced:
            raise RuntimeError('splice %#x: bytes %s != expected %s' % (addr, raw.hex(), displaced.hex()))
        stub = STUB2 + self.stub2_count * 16
        self.stub2_count += 1
        back = addr + len(displaced)
        code = displaced + b'\xe9' + struct.pack('<i', back - (stub + len(displaced) + 5))
        assert len(code) <= 16
        uc.mem_write(stub, code)
        self.hooks[stub] = fn
        uc.mem_write(addr, b'\xe9' + struct.pack('<i', stub - (addr + 5)))

    def enable_ecl_trace(self):
        """0x4720af（0x472030 通用 ECL 解释器取操作码处：ESI = 指令，EDI = 上下文）记录每条执行的指令。"""
        self.ecl_trace = []
        def on_ins(o):
            uc = self.uc
            ins = uc.reg_read(UC_X86_REG_ESI)
            ctx = uc.reg_read(UC_X86_REG_EDI)
            owner = self.u32(ctx + 0x1018)
            t, op, size, mask, rank, argc = struct.unpack('<iHHHBB', uc.mem_read(ins, 12))
            self.ecl_trace.append((self.frame, self.u32(owner + 0x5740), self.i32(ctx + 4), self.i32(ctx + 8),
                                   struct.unpack('<f', uc.mem_read(ctx, 4))[0], t, op))
        # MOVSX EAX, word ptr [ESI+4]; CMP EAX,0x5d
        self.splice_hook(0x4720AF, bytes.fromhex('0fbf460483f85d'), on_ins)

    def enable_rng_trace(self, which=0x4A6D88):
        """0x402b70（单步）/ 0x402be0（双步）入口：记录对 RNG `which`（默认 DAT_004a6d88 回放同步 RNG；
        0x4a6d80 为游戏逻辑 RNG）的每次调用及其返回地址。"""
        self.rng_trace = []
        def make(step):
            def on_rng(o):
                uc = self.uc
                if uc.reg_read(UC_X86_REG_ECX) != which:
                    return
                esp = uc.reg_read(UC_X86_REG_ESP)
                ret = self.u32(esp)
                # 0x402c70 / 0x402cb0 / 0x402cf0 包装（push ebp; mov ebp,esp; push ecx; call）：真正调用方在 [esp+12]
                caller = self.u32(esp + 12) if ret in (0x402C7C, 0x402CBC, 0x402CFC) else 0
                self.rng_trace.append((self.frame, step, ret, caller, self.cur_vm_script, self.cur_vm_sprite))
            return on_rng
        # 0x45f980（AnmVm::run）入口：记下当前 VM 的脚本号（+0x24）与精灵号（+0x20），便于归因 ANM 变量随机数
        self.cur_vm_script = self.cur_vm_sprite = -1
        def on_vm_run(o):
            vm = self.uc.reg_read(UC_X86_REG_ECX)
            self.cur_vm_script = self.i32(vm + 0x24)
            self.cur_vm_sprite = self.i32(vm + 0x20)
        # PUSH EBP; MOV EBP,ESP; AND ESP,-8
        self.splice_hook(0x45F980, bytes.fromhex('558bec83e4f8'), on_vm_run)
        # CMP byte ptr [0x4c10b6], 0
        self.splice_hook(0x402B70, bytes.fromhex('803db6104c0000'), make(1))
        self.splice_hook(0x402BE0, bytes.fromhex('803db6104c0000'), make(2))

    def normalize_laser_graze_z(self):
        """激光自机判定把擦弹点的 z 留在未初始化栈槽上（0x43ad00 极坐标只写 x/y）：0x433510 直线（非全长判定时 z =
        pos.z + 垃圾）、0x435610 照射（z 槽从未写入）、0x437cf0 曲线（z = 垃圾 + 节点 z）。传给 0x444cf0 的 z 是上层调用
        残留的堆指针位——与堆布局相关、原版亦不可复现；擦弹得点道具会原样继承该 z（道具逻辑不读 z）。为让道具快照可对拍，
        在 0x444cf0 入口把这三处的擦弹点 z 归零（直线全长判定 z = pos.z 是确定值，保持不动）；移植端同口径传 0。"""
        def on_graze(o):
            uc = self.uc
            esp = uc.reg_read(UC_X86_REG_ESP)
            ret = self.u32(esp)
            if 0x433510 <= ret < 0x433720:
                laser = uc.reg_read(UC_X86_REG_ESI)
                if self.u32(laser + 0x608) & 2:
                    return
            elif not (0x435610 <= ret < 0x4357A0 or 0x437CF0 <= ret < 0x437EE0):
                return
            pos = self.u32(esp + 4)
            uc.mem_write(pos + 8, struct.pack('<f', 0.0))
        # PUSH EBP; MOV EBP,ESP; AND ESP,-8
        self.splice_hook(0x444CF0, bytes.fromhex('558bec83e4f8'), on_graze)

    def enable_vm_trace(self):
        """0x407b20（AnmFile::init_vm：按脚本模板初始化 VM）入口：记录每次 VM 生成 (frame, AnmFile this, 脚本号, 返回地址)。"""
        self.vm_trace = []
        def on_init(o):
            uc = self.uc
            esp = uc.reg_read(UC_X86_REG_ESP)
            self.vm_trace.append((self.frame, uc.reg_read(UC_X86_REG_ECX), self.i32(esp + 8), self.u32(esp)))
        # PUSH EBP; MOV EBP,ESP; PUSH EBX; MOV EBX,[EBP+8]
        self.splice_hook(0x407B20, bytes.fromhex('558bec538b5d08'), on_init)

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
            'D3DXLoadSurfaceFromSurface': (36, lambda o: self.uc.reg_write(UC_X86_REG_EAX, 0)),  # 截屏（暂停/结束画面）
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

    def setup_laser_sound_buffer(self):
        """DAT_004dba74：魔理沙激光循环音的 IDirectSoundBuffer（0x446260 每帧按自机 x 调 +0x40 SetPan）。
        oracle 没有声音设备，这里放一只全部方法都返回 0 的假对象（COM stdcall：this + 1 参，RET 8），只计数。"""
        methods = 0x60 // 4
        vt = self.alloc(methods * 4)
        for i in range(methods):
            addr = self.make_stub2(8, (lambda idx: (lambda o: self.on_laser_sound_method(idx)))(i))
            self.uc.mem_write(vt + i * 4, struct.pack('<I', addr))
        obj = self.alloc(0x10)
        self.uc.mem_write(obj, struct.pack('<I', vt))
        self.uc.mem_write(0x4DBA74, struct.pack('<I', obj))
        self.laser_sound_calls = {}

    def on_laser_sound_method(self, idx):
        self.laser_sound_calls[idx] = self.laser_sound_calls.get(idx, 0) + 1
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
        uc.mem_write(0x4A57C8, struct.pack('<i', self.replay_slot))  # DAT_004a57c8：-1 普通/练习（MainFront），≥0 符卡（MainBossSpell）
        uc.mem_write(DAT_STAGE_TABLE, struct.pack('<I', STAGE_TABLE_BASE + self.stage * STAGE_TABLE_STRIDE))
        uc.mem_write(DAT_GAME_FLAGS, struct.pack('<I', (self.mode & 3) << 4))
        # 0x43ce10 case 7（标题 → 游戏）：Supervisor(0x4c10d0)+0x700 = 1（新开局：0x42cb60 据此创建 Player）
        uc.mem_write(DAT_UI_CAMERA + 0x700, struct.pack('<I', 1))
        # GameManager 创建 + 装载（同步）
        self.game_manager = self.call(GAME_CREATE, ecx=0)
        if self.u32(DAT_GAME_MANAGER) != self.game_manager:
            raise RuntimeError('DAT_004a6dd4 not set')
        rc = self.call(GAME_LOAD)
        if rc != 0:
            raise RuntimeError('0x42cb60 failed rc=%#x errors=%s' % (rc, self.errors[-6:]))
        uc.mem_write(DAT_SCENE_CURRENT, struct.pack('<II', 7, 7))  # 0x43ce10 case 7 之后：当前 / 请求场景均为 Game
        if self.lives is not None:
            uc.mem_write(0x4A57F4, struct.pack('<i', self.lives))  # DAT_004a57f4：残机覆盖（长程无输入对拍不被 Game Over 截断）
        # Supervisor 绘制链 0x43b3d0 每帧写的场地屏幕偏移 DAT_004d9d50/54 = 640/2、(480-448)/2：
        # 自机弹出界判定 0x4456d0 → 0x468c00 → 0x406c40 读它们；oracle 不跑绘制链，在此一次性写入
        uc.mem_write(0x4D9D50, struct.pack('<ii', 320, 16))
        self.frame = 0
        self.scene_exit = None   # 游戏场景之外的 DAT_004c17c4 请求（标题 4 / 结局 0xf / 0x10 / 退出 2）

    def on_sfx(self, o):
        esp = self.uc.reg_read(UC_X86_REG_ESP)
        ret, sfx = struct.unpack('<Ii', self.uc.mem_read(esp, 8))
        self.sfx_log.append((sfx, ret))

    # ---- 帧 ----
    def set_input(self, held):
        prev = self.u32(DAT_INPUT_HELD)
        self.uc.mem_write(DAT_INPUT_HELD, struct.pack('<II', held, prev))
        self.call(INPUT_EDGES, ecx=DAT_INPUT_HELD)

    def switch_scene(self):
        """0x43ce10 的场景切换（主循环每帧在链更新前执行）：
        case 0xc（过关 → 下一关）：Supervisor+0x700（DAT_004c17d0 新开局标志）= 0、0x42d780 销毁、0x42d700(GameManager+0xac) 重建；
        case 0xa / 0xb / 0xe / 0x13（GameMenu 重开 / Extra 续关）：0x42d780 销毁、+0x700 = 1、关卡回到 DAT_004a5794、
        0x42d700(0xb 时 1) 重建；其余请求（标题 / 结局 / 退出）记录在 scene_exit 并结束对拍。"""
        request = self.u32(DAT_SCENE_REQUEST)
        if request == self.u32(DAT_SCENE_CURRENT):
            return
        if request == 0xC:
            mode = self.u32(self.u32(DAT_GAME_MANAGER) + 0xAC)
            self.uc.mem_write(DAT_UI_CAMERA + 0x700, struct.pack('<I', 0))
        elif request in (0xA, 0xB, 0xE, 0x13):
            mode = 1 if request == 0xB else 0
            self.uc.mem_write(DAT_UI_CAMERA + 0x700, struct.pack('<I', 1))
            self.uc.mem_write(DAT_UI_CAMERA + 0x704, struct.pack('<I', 1 if request == 0x13 else 0))
            stage = self.u32(DAT_STAGE_COPY)
            self.uc.mem_write(DAT_STAGE_NUMBER, struct.pack('<I', stage))
            self.uc.mem_write(DAT_STAGE_TABLE, struct.pack('<I', STAGE_TABLE_BASE + stage * STAGE_TABLE_STRIDE))
        else:
            self.scene_exit = request
            return
        self.call(GAME_DESTROY)
        self.uc.mem_write(DAT_SCENE_REQUEST, struct.pack('<I', 7))
        self.game_manager = self.call(GAME_CREATE, ecx=mode)
        rc = self.call(GAME_LOAD)
        if rc != 0:
            raise RuntimeError('0x42cb60 (scene %#x) failed rc=%#x errors=%s' % (request, rc, self.errors[-6:]))
        self.uc.mem_write(DAT_SCENE_CURRENT, struct.pack('<I', 7))

    def stage_draw_side_effects(self):
        """不跑绘制链时补 0x40a410（Stage 绘制链）尾部唯一影响逻辑状态的部分：结束计时 +0x66c8 倒数（0x40d490），
        归零时 bit1（结束中）→ bit3（可销毁，0x42d7b0 据此释放上一关），并清 bit1 / bit2。覆盖当前关 DAT_004a6da0 与
        过关切换后仍在运行的上一关 DAT_004a6d9c。bit3 时整段跳过；"bit2 且计时 ≥ 0x3c" 只跳过绘制，计时照常倒数。"""
        for addr in (0x4A6DA0, 0x4A6D9C):
            stage = self.u32(addr)
            if not stage:
                continue
            flags = self.u32(stage + 0x66C4)
            if flags & 8:
                continue
            if (flags & 4) and self.i32(stage + 0x66CC) > 0x1D:
                self.uc.mem_write(stage + 0x3353, b'\x00')
            if self.i32(stage + 0x66CC) > 0:
                self.call(0x40D490, ecx=stage + 0x66C8)
                if self.i32(stage + 0x66CC) < 1:
                    self.uc.mem_write(stage + 0x3353, b'\xff')
                    flags = self.u32(stage + 0x66C4)
                    if flags & 2:
                        flags |= 8
                    self.uc.mem_write(stage + 0x66C4, struct.pack('<I', flags & ~6))
                    self.uc.mem_write(stage + 0x3350, struct.pack('<I', 0xFFFFFF))

    def tick(self):
        self.sfx_log = []
        self.switch_scene()
        # 更新链含 AnmManager 自身的世界 tick（优先级 9，0x46e740），无需另行调用 0x46e340
        self.call(CHAIN_RUN_UPDATE, ecx=self.chain_manager)
        # 绘制链（0x4015a0，读 DAT_004a6d94）：Player 0x443730 等在此把 pos 抄进 VM entity_offset；D3D 设备为空桩
        if self.run_draw:
            self.call(CHAIN_RUN_DRAW)
        else:
            self.stage_draw_side_effects()
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

    def words(self, addr, n):
        return [self.u32(addr + 4 * i) for i in range(n)]

    def timer(self, addr):
        # Timer（0x14）：previous, current, current_f, speed_slot, flags
        return self.words(addr, 5)

    def enemy_snapshot(self):
        """EnemyManager（DAT_004a6dc0，0x190 字节）：+0x180 敌人链表（节点 {Enemy*, next}）。
        Enemy：+0x10 主 ECL 上下文 {time f32, sub, pc}，+0x120c EnemyInf（+0x44 位置、+0x88/+0xcc 两运动体、+0x4060 标志），
        +0x5740 id。sub 名取自 ECL 资源（Enemy+0x11f8 → +0x8c 子程序表 {name*, code*}）。"""
        m = self.u32(0x4A6DC0)
        if not m:
            return None
        out = {'count': self.i32(m + 0x18C), 'enemies': []}
        node = self.u32(m + 0x180)
        while node:
            e = self.u32(node)
            inf = e + 0x120C
            ctx = e + 0x10
            sub = self.i32(ctx + 4)
            res = self.u32(e + 0x11F8)
            name = self.cstr(self.u32(self.u32(res + 0x8C) + sub * 8)) if res and sub >= 0 else None
            out['enemies'].append({
                'id': self.u32(e + 0x5740), 'flags': self.u32(inf + 0x4060),
                'pos': list(struct.unpack('<3f', self.uc.mem_read(inf + 0x44, 12))),
                'motion1': self.words(inf + 0x88, 17), 'motion2': self.words(inf + 0xCC, 17),
                'ctx': [struct.unpack('<f', self.uc.mem_read(ctx, 4))[0], sub, self.i32(ctx + 8), name],
                'hp': self.words(inf + 0x3F74, 4),
                'vms': self.enemy_vm_snapshot(inf),
                'sprite_size': self.words(inf + 0x3F58, 2),
            })
            node = self.u32(node + 4)
        return out

    def enemy_vm_snapshot(self, inf):
        """EnemyInf+0x124 的 14 个 ANM VM id（ECL 303/306/307/… 生成）→ 按 0x46efa0 查到的 VM 关键字段
        [槽号, 脚本号 +0x24, 精灵号 +0x20, entity_offset +0x5e0 ×3, pos +0x2c ×3, rotation.z +0x40, flags_lo +0x530, flags_hi +0x534, 年龄 +0x558]。"""
        vms = []
        for slot in range(14):
            vid = self.u32(inf + 0x124 + slot * 4)
            if not vid:
                continue
            vm = self.anm_find(vid)
            if not vm:
                continue
            vms.append([slot, self.i32(vm + 0x24), self.i32(vm + 0x20)] + self.words(vm + 0x5E0, 3) +
                       self.words(vm + 0x2C, 3) + [self.u32(vm + 0x40), self.u32(vm + 0x530), self.u32(vm + 0x534), self.i32(vm + 0x558)])
        return vms

    def anm_find(self, vid):
        """0x46efa0：AnmManager（DAT_004c0f48）池 +0xec 起每 0x614 字节；id 低 13 位为池下标，+0x538 必须等于 id 且未打删除位。"""
        mgr = self.u32(0x4C0F48)
        if not mgr or not vid:
            return 0
        vm = mgr + 0xEC + (vid & 0x1FFF) * 0x614
        if self.uc.mem_read(vm + 0x60C, 1)[0] == 0 or self.u32(vm + 0x538) != vid:
            return 0
        return vm

    def bullet_snapshot(self):
        """BulletManager（DAT_004a6dac）：+0x40 count，+0x70 活动链表（节点在 Bullet+0x10）。"""
        bm = self.u32(0x4A6DAC)
        if not bm:
            return None
        out = {'count': self.i32(bm + 0x40), 'bullets': []}
        node = self.u32(bm + 0x70)
        while node:
            b = self.u32(node)
            out['bullets'].append({
                'index': self.i32(b + 0xC48),
                'flags': self.u32(b + 0x20),
                'state': struct.unpack('<H', self.uc.mem_read(b + 0xC72, 2))[0],
                'type': struct.unpack('<h', self.uc.mem_read(b + 0x1474, 2))[0],
                'color': struct.unpack('<h', self.uc.mem_read(b + 0x1476, 2))[0],
                'pos': self.words(b + 0xC20, 3),
                'vel': self.words(b + 0xC2C, 3),
                'speed': self.u32(b + 0xC38),
                'angle': self.u32(b + 0xC3C),
                'hitbox': self.words(b + 0xC40, 2),
                'scale': self.u32(b + 0x141C),
                'ex_index': self.i32(b + 0xC60),
                'ex_flags': self.u32(b + 0xC68),
                'timer': self.timer(b + 0x144C),
                'sprite_id': self.i32(b + 0x48),
            })
            node = self.u32(node + 4)
        return out

    LASER_VTABLES = {0x492424: 0, 0x4923B8: 1, 0x4922E0: 2, 0x49234C: 3}  # Line / Infinite / Curve / Beam

    def laser_snapshot(self):
        """LaserManager（DAT_004a6ee0，0x610 字节）：+0x14 链首（激光自身即结点：+4 prev / +8 next），+0x5e4 计数，
        +0x5e8 句柄计数器。激光公共区：+0xc 生命周期位、+0x10 状态、+0x18/+0x2c/+0x40 三计时器、+0x54 起点、+0x60 速度向量、
        +0x6c 角度、+0x70 长度、+0x74 宽度、+0x78 速率、+0x7c 起点已越过距离、+0x80 句柄、+0x594 Ex 下标、+0x598 Ex 位、
        +0x5c8 免疫、+0x5cc 弹种、+0x5d0 颜色；曲线激光另附 +0x5f4 采样点数与 +0x1524 采样点数组（0x20 字节：pos / vel / angle / speed）。"""
        m = self.u32(0x4A6EE0)
        if not m:
            return None
        out = {'count': self.i32(m + 0x5E4), 'next_handle': self.i32(m + 0x5E8), 'lasers': []}
        l = self.u32(m + 0x14)
        while l:
            kind = self.LASER_VTABLES.get(self.u32(l), -1)
            entry = {
                'kind': kind, 'lc': self.u32(l + 0xC), 'state': self.i32(l + 0x10), 'id': self.i32(l + 0x80),
                'type': self.i32(l + 0x5CC), 'color': self.i32(l + 0x5D0),
                'pos': self.words(l + 0x54, 3), 'vel': self.words(l + 0x60, 3),
                'angle': self.u32(l + 0x6C), 'length': self.u32(l + 0x70), 'width': self.u32(l + 0x74),
                'speed': self.u32(l + 0x78), 'start_drawn': self.u32(l + 0x7C),
                'ex_index': self.i32(l + 0x594), 'ex_flags': self.u32(l + 0x598), 'no_cancel': self.i32(l + 0x5C8),
                'age': self.timer(l + 0x18), 'graze': self.timer(l + 0x2C), 'frame': self.timer(l + 0x40),
                'nodes': [],
            }
            if kind == 2:
                n = max(0, min(self.i32(l + 0x5F4), 0x40))
                nodes = self.u32(l + 0x1524)
                for i in range(n):
                    nd = nodes + i * 0x20
                    entry['nodes'].append(self.words(nd, 3) + [self.u32(nd + 0x18), self.u32(nd + 0x1C)])
            out['lasers'].append(entry)
            l = self.u32(l + 8)
        return out

    # 类名取自 RTTI 类型描述符（Bomb<Name>Inf）；0x40d890：机体 0..3 → ReimuA / CirnoA / AyaA / MarisaA，
    # 子季节 0..4 → ReimuSub / CirnoSub / AyaSub / MarisaSub / AllSub
    BOMB_VTABLES = {0x491E3C: 'ReimuA', 0x491E04: 'CirnoA', 0x491DE8: 'AyaA', 0x491E20: 'MarisaA',
                    0x491DCC: 'ReimuSub', 0x491D94: 'CirnoSub', 0x491D78: 'AyaSub', 0x491DB0: 'MarisaSub', 0x491D5C: 'AllSub'}

    def bomb_snapshot(self):
        """Bomb 对象（0x108 字节，0x40d890 按机体 / 季节建两只：DAT_004a6da8 主炸弹、DAT_004a6da4 季节解放）：
        +0x14 位置、+0x20 角速度、+0x2c 角度、+0x30 活动、+0x34 计时、+0x5c / +0x64 两枚 VM、+0x68 符卡中发动标志、
        +0x70 灵梦阴阳玉数组（8 × 0xd8：VM、运动体 0x44、+0xa0 活动、+0xa4 计时、+0xb8 位移、+0xc4 目标敌机、
        +0xcc 序号、+0xd0 伤害源；+0xd4 为未写入的填充）、+0xd4 种类、+0xd8 发动时等级、+0xdc 累计奖励、+0xe0 显示奖励、
        +0xe4 奖励飘字计时、+0xf8 飘字等级、+0xfc 飘字坐标。"""
        out = {}
        for key, slot in (('main', 0x4A6DA8), ('sub', 0x4A6DA4)):
            b = self.u32(slot)
            if not b:
                out[key] = None
                continue
            has = lambda v: 1 if v else 0
            entry = {
                'class': self.BOMB_VTABLES.get(self.u32(b), hex(self.u32(b))),
                'pos': self.words(b + 0x14, 3), 'angular': self.u32(b + 0x20), 'angle': self.u32(b + 0x2C),
                'active': self.i32(b + 0x30), 'timer': self.timer(b + 0x34),
                'vm': [has(self.u32(b + 0x5C)), has(self.u32(b + 0x64))], 'in_spell': self.i32(b + 0x68),
                'kind': self.i32(b + 0xD4), 'level': self.i32(b + 0xD8), 'bonus': self.u32(b + 0xDC),
                'bonus_shown': self.u32(b + 0xE0), 'popup': self.timer(b + 0xE4), 'popup_level': self.i32(b + 0xF8),
                'popup_pos': self.words(b + 0xFC, 3), 'orbs': [],
            }
            orbs = self.u32(b + 0x70)
            if orbs:
                for i in range(8):
                    o = orbs + i * 0xD8
                    entry['orbs'].append({
                        'vm': has(self.u32(o)), 'pos': self.words(o + 4, 3), 'speed': self.u32(o + 0x1C),
                        'angle': self.u32(o + 0x20), 'radius': self.u32(o + 0x24), 'center': self.words(o + 0x38, 3),
                        'mode': self.u32(o + 0x44), 'active': self.i32(o + 0xA0), 'timer': self.timer(o + 0xA4),
                        'delta': self.words(o + 0xB8, 3), 'target': self.u32(o + 0xC4), 'index': self.i32(o + 0xCC),
                        'source': self.i32(o + 0xD0),
                    })
            out[key] = entry
        return out

    def item_snapshot(self):
        """ItemManager（DAT_004a6ddc，0x1c972ec 字节）：+0x14 起 0x1258 个道具（0xc78 字节；前 600 个为普通池，
        其后 0x1000 个为得点/季节池），+0x1c972dc 活动数、+0x1c972e0 生成计数、+0x1c972e4/+0x1c972e8 得点池计数。
        道具：+0xc08 位置、+0xc14 速度、+0xc20 减速速度、+0xc24 角度、+0xc28 Timer、+0xc50 状态、+0xc54 种类、
        +0xc58 种类副本、+0xc5c 吸引速度、+0xc60 延迟计数、+0xc68 标志；VM +0x10（主）/ +0x60c（副）。"""
        m = self.u32(0x4A6DDC)
        if not m:
            return None
        out = {'active': self.i32(m + 0x1C972DC), 'spawned': self.i32(m + 0x1C972E0),
               'point_counter': self.words(m + 0x1C972E4, 2), 'scale': self.u32(m + 0xE4B974), 'items': []}
        base = m + 0x14
        for i in range(0x1258):
            it = base + i * 0xC78
            state = self.i32(it + 0xC50)
            if state == 0:
                continue
            out['items'].append({
                'index': i, 'state': state, 'type': self.i32(it + 0xC54), 'type2': self.i32(it + 0xC58),
                'pos': self.words(it + 0xC08, 3), 'vel': self.words(it + 0xC14, 3),
                'speed': self.u32(it + 0xC20), 'angle': self.u32(it + 0xC24), 'timer': self.timer(it + 0xC28),
                'attract_speed': self.u32(it + 0xC5C), 'counter': self.i32(it + 0xC60), 'serial': self.i32(it + 0xC64),
                'flag': self.i32(it + 0xC68),
                'vm': [self.i32(it + 0x10 + 0x24), self.i32(it + 0x10 + 0x20), self.u32(it + 0x10 + 0x530), self.u32(it + 0x10 + 0x520),
                       self.i32(it + 0x60C + 0x24), self.i32(it + 0x60C + 0x20), self.u32(it + 0x60C + 0x530)],
            })
        return out

    def resource_snapshot(self):
        """得分 / 残机 / 炸弹 / 得点值等由道具拾取改写的全局（0x43e080 / 0x43df70 / 0x43dff0 / 0x430dc0 …）。"""
        return {
            'score': self.i32(0x4A57B0), 'extend_index': self.i32(0x4A57FC),
            'lives': self.i32(0x4A57F4), 'life_fragments': self.i32(0x4A57F8),
            'bombs': self.i32(0x4A5800), 'bomb_fragments': self.i32(0x4A5804),
            'point_value': self.i32(0x4A57D8), 'point_value_max': self.i32(0x4A57E0),
            'point_items': self.i32(0x4A57D4), 'graze': self.i32(0x4A57C0),
            'collect': [self.i32(0x4A5860), self.i32(0x4A5868), self.i32(0x4A586C), self.i32(0x4A587C)],
            'collect_pos': self.words(0x4A5870, 3),
        }

    def spell_snapshot(self):
        """Spellcard（DAT_004a6db0，0x4178a0）：标志 / 计时 / 奖励 / 编号 / 状态，另附 Gui 对话指针与敌机 Boss 槽。"""
        s = self.u32(0x4A6DB0)
        if not s:
            return None
        gui = self.u32(0x4A6DCC)
        em = self.u32(0x4A6DC0)
        return {
            'flags': self.u32(s + 0x78), 'timer': self.words(s + 0x20, 3), 'id': self.i32(s + 0x74),
            'bonus': self.i32(s + 0x7C), 'bonus_initial': self.i32(s + 0x80), 'time_limit': self.i32(s + 0x84),
            'count': self.i32(s + 0x88), 'state': self.i32(s + 0x8C),
            'vm': [1 if self.u32(s + 0xC + i * 4) else 0 for i in range(5)],
            'dialogue': 1 if gui and self.u32(gui + 0x1C8) else 0,  # Gui+0x1c8 Msg 指针有无（指针值随堆变化，不入对拍）
            'boss_ids': self.words(em + 0x48, 3) if em else [0, 0, 0],
            'enemy_bonus': [self.i32(0x4A5880), self.i32(0x4A5884)],
            'spell_id': self.i32(0x4A5798),
        }

    def menu_snapshot(self):
        """GameMenu（DAT_004a6ef4，0x3f4 字节，0x43e350）：暂停 / 续关 / 练习结算菜单。
        +0xc 计时、+0x20 副计时、+0x34 光标（index / previous / count）、+0x10c 名字输入光标、+0x1e4 菜单 VM、
        +0x1e8 截图 VM、+0x1ec 模式（0 关闭 1 暂停 2 续关 3 练习结算）、+0x1f0 上一模式、+0x1f4 状态、+0x1f8 名字长度、
        +0x1fc 练习标志、+0x200 无排名、+0x204 菜单种类、+0x208 保存的声音线程状态、+0x2e0 保存的游戏速度、+0x3ec 标志。
        另附 GameManager+0x88 标志、全局游戏速度、续关次数（DAT_004a57b8）/ 剩余续关（DAT_004a5bfc）与本帧全部音效。"""
        m = self.u32(0x4A6EF4)
        gm = self.u32(DAT_GAME_MANAGER)
        has = lambda v: 1 if v else 0
        out = {
            'game_flags': self.u32(gm + 0x88) if gm else 0,
            'game_speed': self.u32(DAT_GAME_SPEED),
            'continues_used': self.i32(0x4A57B8), 'continues_left': self.i32(0x4A5BFC),
            'sfx': [s for s, _ in self.sfx_log],
        }
        if not m:
            out['menu'] = None
            return out
        out['menu'] = {
            'mode': self.i32(m + 0x1EC), 'prev_mode': self.i32(m + 0x1F0), 'state': self.i32(m + 0x1F4),
            'timer': self.timer(m + 0xC), 'timer2': self.timer(m + 0x20),
            'cursor': [self.i32(m + 0x34), self.i32(m + 0x38), self.i32(m + 0x3C), self.i32(m + 0x34 + 0xD4)],
            'name_cursor': [self.i32(m + 0x10C), self.i32(m + 0x110), self.i32(m + 0x114)],
            'vm': [has(self.u32(m + 0x1E4)), has(self.u32(m + 0x1E8))],
            'name_len': self.i32(m + 0x1F8), 'practice': self.i32(m + 0x1FC), 'no_rank': self.i32(m + 0x200),
            'kind': self.i32(m + 0x204), 'saved_sound': self.i32(m + 0x208), 'saved_speed': self.u32(m + 0x2E0),
            'flags': self.u32(m + 0x3EC),
        }
        return out

    def msg_snapshot(self):
        """Gui+0x1c8 对话 Msg（0x429b20）：脚本偏移 / 三计时器 / 标志 / 侧 / 行计数 / 文字框 / VM 有无。
        与 harness `game-run` 的 msg 字段同口径；无对话时为 None。"""
        gui = self.u32(0x4A6DCC)
        m = self.u32(gui + 0x1C8) if gui else 0
        if not m:
            return None
        base = self.u32(gui + 0x1CC)
        return {
            'entry': self.i32(m), 'pc': self.u32(m + 0x158) - base,
            'age': self.i32(m + 0x4 + 4), 'timer': self.i32(m + 0x18 + 4), 'wait': self.i32(m + 0x2C + 4),
            'ecl_release': self.i32(m + 0x18C), 'flags': self.u32(m + 0x190),
            'lines': [self.i32(m + 0x194), self.i32(m + 0x198)], 'side': self.i32(m + 0x19C),
            'text_pos': self.words(m + 0x1B0, 2), 'width': self.u32(m + 0x1BC), 'box_index': self.i32(m + 0x1C4),
            'vm': [1 if self.u32(m + 0x40 + i * 4) else 0 for i in range(12)],
        }

    def player_snapshot(self):
        """Player（DAT_004a6ef8，0x2c828 字节）规范字段；与 harness `game-run` 输出一一对应。"""
        p = self.u32(DAT_PLAYER)
        if not p:
            return None
        options = []
        for base in [p + 0x660 + i * 0xE4 for i in range(4)] + [p + 0x9F0 + i * 0xE4 for i in range(8)]:
            options.append({
                'active': self.i32(base), 'target': [self.i32(base + 0x54), self.i32(base + 0x58)],
                'pos': [self.i32(base + 0x5C), self.i32(base + 0x60)],
                'offset': self.words(base + 0x64, 4), 'vm': [self.u32(base + 0xB0), self.u32(base + 0xB4)],
                'index': self.i32(base + 0xD0), 'snap': self.i32(base + 0xD4),
            })
        bullets = []
        for i in range(256):
            b = p + 0x1110 + i * 0xC0
            if self.i32(b + 0x8C) == 0:
                continue
            bullets.append({
                'index': i, 'flags': self.u32(b), 'vm': self.u32(b + 8), 'timer': self.timer(b + 0xC),
                'pos': self.words(b + 0x48, 3), 'speed': self.u32(b + 0x60), 'angle': self.u32(b + 0x64),
                'velocity': self.words(b + 0x7C, 3), 'mode': self.u32(b + 0x88), 'active': self.i32(b + 0x8C),
                'target': self.i32(b + 0x90), 'damage': self.i32(b + 0x9C), 'size': self.words(b + 0xA0, 2),
                'shot_id': self.u32(b + 0xAC), 'damage_source': self.i32(b + 0xB0),
            })
        sources = []
        for i in range(256):
            d = p + 0xD114 + i * 0x94
            if (self.u32(d) & 1) == 0:
                continue
            sources.append({
                'index': i + 1, 'flags': self.u32(d), 'f04': self.words(d + 4, 2), 'angle': self.u32(d + 0xC),
                'angular_speed': self.u32(d + 0x10), 'size': self.words(d + 0x14, 2), 'pos': self.words(d + 0x1C, 3),
                'motion': self.words(d + 0x28, 14), 'timer': self.timer(d + 0x60), 'damage': self.i32(d + 0x74),
                'tail': self.words(d + 0x78, 7),
            })
        # Player 代码段（0x440bd0..0x449000）发出的音效：0x45e1f0 的调用方返回地址落在其中。
        # GameMenu 状态机 0x43f980 一直延伸到 0x440bd0，其确认音（7）不属于 Player。
        sfx = [sid for sid, ret in self.sfx_log if 0x440BD0 <= ret < 0x449000]
        return {
            'state': self.i32(p + 0x165A8),
            'pos': self.words(p + 0x610, 3),
            'pos_fixed': [self.i32(p + 0x61C), self.i32(p + 0x620)],
            'timers': [self.timer(p + 0x624), self.timer(p + 0x638), self.timer(p + 0x64C)],
            'dir': self.i32(p + 0x2C780),
            'last_move': [self.i32(p + 0x2C778), self.i32(p + 0x2C77C)],
            'focus': self.i32(p + 0x165C8),
            'focus_vm': self.u32(p + 0x165AC),
            'speeds': self.words(p + 0x16650, 4),
            'velocity': self.words(p + 0x16660, 8),       # 0x16660 速度 ×3、0x1666c 上次非零速度 ×3、0x16678 整数速度 ×2
            'option_lag': self.i32(p + 0x16680),
            'counter': self.i32(p + 0x16684),
            'speed_mult': self.u32(p + 0x16688),
            'ext_velocity': self.words(p + 0x1668C, 3),
            'option_count': self.i32(p + 0x165F4),
            'power_level': self.i32(p + 0x16698),
            'sub_count': self.i32(p + 0x1669C),
            'flags': self.u32(p + 0x1664C),
            'invincible': self.timer(p + 0x16638),
            'shot_timers': [self.timer(p + 0x165CC), self.timer(p + 0x165E0)],
            'rects': self.words(p + 0x2C730, 6),
            'radii': self.words(p + 0x2C748, 9),
            'rects2': self.words(p + 0x2C7DC, 18),
            'scale': [self.i32(p + 0x2C7C0), self.u32(p + 0x2C7C8)],
            'options': options,
            'shot_misc': [self.i32(p + 0x2C790), self.uc.mem_read(p + 0x2C794, 1)[0]],
            'cooldown': self.words(p + 0x165F8, 16),
            'last_damage_index': self.i32(p + 0xD110),
            'bullets': bullets,
            'damage_sources': sources,
            'sfx': sfx,
            'vm': bytes(self.uc.mem_read(p + 0x14, 0x568)).hex(),
        }

    def snapshot(self):
        sm = self.u32(DAT_STAGE_MANAGER)
        snap = {
            'frame': self.frame,
            'stage': self.i32(0x4A5790),
            'game_frame': self.i32(0x4A579C),
            'stage_frame': self.i32(0x4A57A0),
            'input': [self.u32(DAT_GAME_INPUT), self.u32(DAT_GAME_INPUT + 0xC)],
            'power': self.i32(0x4A57E4),
            'season_gauge': self.i32(0x4A5808),
            'player': self.player_snapshot(),
            # 其它管理器尚未移植时 harness 的外部状态输入（PlayerWorld）：不进 digest
            'enemy_count': self.i32(self.u32(0x4A6DC0) + 0x18C) if self.u32(0x4A6DC0) else 0,
            'dialogue': 1 if self.u32(0x4A6DCC) and self.u32(self.u32(0x4A6DCC) + 0x1C8) else 0,
            'msg': self.msg_snapshot(),
            # DAT_004a6d88（回放同步 RNG）：种子与调用计数（调试对拍用；不进 digest）
            'rng': [struct.unpack('<H', self.uc.mem_read(0x4A6D88, 2))[0], self.u32(0x4A6D8C)],
            # DAT_004a6d80（游戏逻辑 RNG）：种子与调用计数（调试对拍用；不进 digest）
            'game_rng': [struct.unpack('<H', self.uc.mem_read(0x4A6D80, 2))[0], self.u32(0x4A6D84)],
        }
        if self.enemy_snap:
            snap['enemies'] = self.enemy_snapshot()
        if self.bullet_snap:
            snap['bullets'] = self.bullet_snapshot()
        if self.item_snap:
            snap['items'] = self.item_snapshot()
            snap['resources'] = self.resource_snapshot()
        if self.laser_snap:
            snap['lasers'] = self.laser_snapshot()
        if self.bomb_snap:
            snap['bombs'] = self.bomb_snapshot()
        if self.enemy_snap:
            snap['spell'] = self.spell_snapshot()
        snap['menu'] = self.menu_snapshot()
        if sm:
            snap['stage_vm'] = {'frame': self.i32(sm + 0x66E0), 'ins': self.i32(sm + 0x20), 'flags': self.u32(sm + 0x66C4)}
        return snap


def player_words(pl):
    """规范 u32 字序列（harness `game-digest` 同口径）。
    VM id 只取“有无”（池分配顺序取决于尚未移植的敌人/子弹/特效管理器）。"""
    has = lambda v: 1 if v else 0
    w = [pl['state']] + pl['pos'] + pl['pos_fixed']
    for t in pl['timers']:
        w += t[:3]
    w += [pl['dir']] + pl['last_move'] + [pl['focus'], has(pl['focus_vm'])] + pl['speeds'] + pl['velocity']
    w += [pl['option_lag'], pl['counter'], pl['speed_mult']] + pl['ext_velocity']
    w += [pl['option_count'], pl['power_level'], pl['sub_count'], pl['flags']] + pl['invincible'][:3]
    for t in pl['shot_timers']:
        w += t[:3]
    w += pl['rects'] + pl['radii'] + pl['rects2'] + pl['scale']
    for o in pl['options']:
        w += [o['active']] + o['target'] + o['pos'] + o['offset'] + [has(o['vm'][0]), has(o['vm'][1])] + [o['index'], o['snap']]
    # 射击系统（自机弹 / 伤害源 / 类型 2 冷却 / 音效）；旧快照（无这些键）按空处理
    w += pl.get('shot_misc', [0, 0]) + pl.get('cooldown', [0] * 16) + [pl.get('last_damage_index', 0)]
    bullets = pl.get('bullets', [])
    w += [len(bullets)]
    for b in bullets:
        w += [b['index'], b['flags'], has(b['vm'])] + b['timer'][:3] + b['pos'] + [b['speed'], b['angle']] + b['velocity']
        w += [b['mode'], b['active'], b['target'], b['damage']] + b['size'] + [b['shot_id'], b['damage_source']]
    sources = pl.get('damage_sources', [])
    w += [len(sources)]
    for d in sources:
        w += [d['index'], d['flags']] + d['f04'] + [d['angle'], d['angular_speed']] + d['size'] + d['pos'] + d['motion']
        w += d['timer'][:3] + [d['damage']] + d['tail']
    sfx = pl.get('sfx', [])
    w += [len(sfx)] + sfx
    return [x & 0xFFFFFFFF for x in w]


def frame_digest(frames, digest=0xCBF29CE484222325):
    mask = (1 << 64) - 1
    def feed(data):
        nonlocal digest
        for b in data:
            digest = ((digest ^ b) * 0x100000001B3) & mask
    for f in frames:
        pl = f['player']
        words = [f['frame'], f['game_frame'], f['stage_frame'], f['input'][0], f['input'][1], f['power'], f['season_gauge']]
        if pl:
            words += player_words(pl)
        feed(b''.join(struct.pack('<I', w & 0xFFFFFFFF) for w in words))
        if pl:
            feed(bytes.fromhex(pl['vm']))
    return digest


def enemy_digest(frames, digest=0xCBF29CE484222325):
    mask = (1 << 64) - 1
    def feed(data):
        nonlocal digest
        for b in data:
            digest = ((digest ^ b) * 0x100000001B3) & mask
    for f in frames:
        en = f.get('enemies') or {'count': 0, 'enemies': []}
        lst = en.get('enemies') or []
        words = [f['frame'], en['count'], len(lst)]
        feed(b''.join(struct.pack('<I', w & 0xFFFFFFFF) for w in words))
        for e in lst:
            t_bits = struct.unpack('<I', struct.pack('<f', float(e['ctx'][0])))[0]
            ew = [e['id'], e['flags']] + e['motion1'] + e['motion2'] + [t_bits, e['ctx'][1], e['ctx'][2], e['hp'][0], e['hp'][1]]
            feed(b''.join(struct.pack('<I', w & 0xFFFFFFFF) for w in ew))
            if e['ctx'][3]:
                feed(e['ctx'][3].encode('utf-8'))
    return digest


def enemy_vm_digest(frames, digest=0xCBF29CE484222325):
    """敌人 ANM VM（EnemyInf+0x124 槽）摘要：每帧 [frame, 敌人数] + 每敌人 [id, VM 数] + 每 VM 13 字。"""
    mask = (1 << 64) - 1
    def feed(data):
        nonlocal digest
        for b in data:
            digest = ((digest ^ b) * 0x100000001B3) & mask
    for f in frames:
        en = f.get('enemies') or {'count': 0, 'enemies': []}
        lst = en.get('enemies') or []
        feed(struct.pack('<II', f['frame'] & 0xFFFFFFFF, len(lst)))
        for e in lst:
            vms = e.get('vms') or []
            feed(struct.pack('<II', e['id'] & 0xFFFFFFFF, len(vms)))
            for v in vms:
                feed(b''.join(struct.pack('<I', w & 0xFFFFFFFF) for w in v))
    return digest


def bullet_digest(frames, digest=0xCBF29CE484222325):
    mask = (1 << 64) - 1
    def feed(data):
        nonlocal digest
        for b in data:
            digest = ((digest ^ b) * 0x100000001B3) & mask
    for f in frames:
        bu = f.get('bullets') or {'count': 0, 'bullets': []}
        lst = bu.get('bullets') or []
        words = [f['frame'], bu['count'], len(lst)]
        feed(b''.join(struct.pack('<I', w & 0xFFFFFFFF) for w in words))
        for b in lst:
            bw = ([b['index'], b['flags'], b['state'], b['type'], b['color']] +
                  b['pos'] + b['vel'] + [b['speed'], b['angle']] + b['hitbox'] +
                  [b['scale'], b['ex_index'], b['ex_flags']] + b['timer'][:3] + [b['sprite_id']])
            feed(b''.join(struct.pack('<I', w & 0xFFFFFFFF) for w in bw))
    return digest


def item_digest(frames):
    """道具池 + 资源全局的 FNV-1a 64（harness game-digest items= 同口径）：
    每帧 [frame, active, spawned, 资源 10 字] + 每道具 [index, state, type, type2, pos×3, vel×3, speed, angle, attract_speed,
    counter, flag, timer×3, vm×7]。"""
    digest = 0xCBF29CE484222325
    mask = (1 << 64) - 1
    def feed(data):
        nonlocal digest
        for b in data:
            digest = ((digest ^ b) * 0x100000001B3) & mask
    for f in frames:
        it = f.get('items') or {'active': 0, 'spawned': 0, 'items': []}
        r = f.get('resources') or {}
        words = [f['frame'], it['active'], it['spawned'],
                 r.get('score', 0), r.get('lives', 0), r.get('bombs', 0), r.get('bomb_fragments', 0), r.get('point_value', 0),
                 r.get('point_items', 0)] + (r.get('collect') or [0, 0, 0, 0])
        feed(b''.join(struct.pack('<I', w & 0xFFFFFFFF) for w in words))
        for i in it['items']:
            iw = ([i['index'], i['state'], i['type'], i['type2']] + i['pos'] + i['vel'] +
                  [i['speed'], i['angle'], i['attract_speed'], i['counter'], i['flag']] + i['timer'][:3] + i['vm'])
            feed(b''.join(struct.pack('<I', w & 0xFFFFFFFF) for w in iw))
    return digest


def laser_digest(frames):
    """激光链表的 FNV-1a 64（harness game-digest lasers= 同口径）：
    每帧 [frame, count, len] + 每激光 [kind, lc, state, id, type, color, pos×3, vel×3, angle, length, width, speed, start_drawn,
    ex_index, ex_flags, no_cancel, age×3, graze×3, frame×3, node_count] + 每采样点 [pos×3, angle, speed]。"""
    digest = 0xCBF29CE484222325
    mask = (1 << 64) - 1
    def feed(words):
        nonlocal digest
        for b in b''.join(struct.pack('<I', w & 0xFFFFFFFF) for w in words):
            digest = ((digest ^ b) * 0x100000001B3) & mask
    for f in frames:
        lz = f.get('lasers') or {'count': 0, 'lasers': []}
        lst = lz.get('lasers') or []
        feed([f['frame'], lz['count'], len(lst)])
        for l in lst:
            feed([l['kind'], l['lc'], l['state'], l['id'], l['type'], l['color']] + l['pos'] + l['vel'] +
                 [l['angle'], l['length'], l['width'], l['speed'], l['start_drawn'], l['ex_index'], l['ex_flags'], l['no_cancel']] +
                 l['age'][:3] + l['graze'][:3] + l['frame'][:3] + [len(l['nodes'])])
            for nd in l['nodes']:
                feed(nd)
    return digest


BOMB_CLASS_INDEX = ['ReimuA', 'CirnoA', 'AyaA', 'MarisaA', 'ReimuSub', 'CirnoSub', 'AyaSub', 'MarisaSub', 'AllSub']


def bomb_digest(frames):
    """炸弹 / 季节解放的 FNV-1a 64（harness game-digest bombs= 同口径）：
    每帧 [frame] + 主炸弹、解放各 [有无] + 公共区 24 字 [class, pos×3, angular, angle, active, timer×3, vm×2, in_spell, kind,
    level, bonus, bonus_shown, popup×3, popup_level, popup_pos×3] + 阴阳玉数 + 每枚 21 字
    [vm, pos×3, speed, angle, radius, center×3, mode, active, timer×3, delta×3, target, index, source]。"""
    digest = 0xCBF29CE484222325
    mask = (1 << 64) - 1
    def feed(words):
        nonlocal digest
        for b in b''.join(struct.pack('<I', w & 0xFFFFFFFF) for w in words):
            digest = ((digest ^ b) * 0x100000001B3) & mask
    for f in frames:
        feed([f['frame']])
        bombs = f.get('bombs') or {}
        for key in ('main', 'sub'):
            b = bombs.get(key)
            feed([1 if b else 0])
            if not b:
                continue
            cls = BOMB_CLASS_INDEX.index(b['class']) if b['class'] in BOMB_CLASS_INDEX else 0xFFFFFFFF
            feed([cls] + b['pos'] + [b['angular'], b['angle'], b['active']] + b['timer'][:3] + b['vm'] +
                 [b['in_spell'], b['kind'], b['level'], b['bonus'], b['bonus_shown']] + b['popup'][:3] +
                 [b['popup_level']] + b['popup_pos'] + [len(b['orbs'])])
            for o in b['orbs']:
                feed([o['vm']] + o['pos'] + [o['speed'], o['angle'], o['radius']] + o['center'] + [o['mode'], o['active']] +
                     o['timer'][:3] + o['delta'] + [o['target'], o['index'], o['source']])
    return digest


def spell_digest(frames):
    """Spellcard 的 FNV-1a 64（harness game-digest spell= 同口径）：
    每帧 [frame, flags, timer×3, id, bonus, bonus_initial, time_limit, count, state, vm 有无×5, dialogue, boss_ids×3,
    enemy_bonus×2, spell_id]。"""
    digest = 0xCBF29CE484222325
    mask = (1 << 64) - 1
    for f in frames:
        s = f.get('spell')
        if s is None:
            words = [f['frame']] + [0] * 20
        else:
            words = ([f['frame'], s['flags']] + s['timer'][:3] + [s['id'], s['bonus'], s['bonus_initial'], s['time_limit'],
                     s['count'], s['state']] + s['vm'] + [s['dialogue']] + s['boss_ids'] + s['enemy_bonus'] + [s['spell_id']])
        for b in b''.join(struct.pack('<I', w & 0xFFFFFFFF) for w in words):
            digest = ((digest ^ b) * 0x100000001B3) & mask
    return digest


def menu_digest(frames):
    """GameMenu 的 FNV-1a 64（harness game-digest menu= 同口径）：
    每帧 [frame, game_flags, game_speed, continues_used, continues_left, 音效数, 音效…, 菜单有无] + 菜单 [mode, prev_mode, state,
    timer×3, timer2×3, cursor×4, name_cursor×3, vm×2, name_len, practice, no_rank, kind, saved_sound, saved_speed, flags]。"""
    digest = 0xCBF29CE484222325
    mask = (1 << 64) - 1
    for f in frames:
        s = f.get('menu')
        if s is None:
            words = [f['frame']]
        else:
            words = [f['frame'], s['game_flags'], s['game_speed'], s['continues_used'], s['continues_left'], len(s['sfx'])] + s['sfx']
            m = s['menu']
            words.append(1 if m else 0)
            if m:
                words += ([m['mode'], m['prev_mode'], m['state']] + m['timer'][:3] + m['timer2'][:3] + m['cursor'] + m['name_cursor'] +
                          m['vm'] + [m['name_len'], m['practice'], m['no_rank'], m['kind'], m['saved_sound'], m['saved_speed'], m['flags']])
        for b in b''.join(struct.pack('<I', w & 0xFFFFFFFF) for w in words):
            digest = ((digest ^ b) * 0x100000001B3) & mask
    return digest


def parse_input(spec):
    events = {}
    if spec:
        for part in spec.split(','):
            f, v = part.split(':')
            events[int(f)] = int(v, 0)
    return events


def main():
    if len(sys.argv) >= 3 and sys.argv[1] == '--digest':
        for path in sys.argv[2:]:
            data = json.loads(pathlib.Path(path).read_text())
            fr = data['frames']
            print('stage=%d shot=%d frames=%d digest=%016x enemies=%016x enemy_vms=%016x bullets=%016x items=%016x spell=%016x lasers=%016x bombs=%016x menu=%016x' % (
                data['stage'], data['shot'], len(fr), frame_digest(fr), enemy_digest(fr), enemy_vm_digest(fr), bullet_digest(fr),
                item_digest(fr), spell_digest(fr), laser_digest(fr), bomb_digest(fr), menu_digest(fr)))
        return 0
    ap = argparse.ArgumentParser()
    ap.add_argument('exe')
    ap.add_argument('dat_dir')
    ap.add_argument('frames', type=int)
    ap.add_argument('out')
    ap.add_argument('--stage', type=int, default=1)
    ap.add_argument('--shot', type=int, default=0)
    ap.add_argument('--season', type=int, default=0)
    ap.add_argument('--difficulty', type=int, default=1)
    ap.add_argument('--lives', type=int, default=None, help='装载后覆盖 DAT_004a57f4 残机数（harness game-run 第 12 参同口径）')
    ap.add_argument('--refill-lives', action='store_true',
                    help='每帧开始前把 DAT_004a57f4 补回 --lives（≤8；残机显示 0x42c280 只有 8 槽）以越过 Game Over（harness 第 13 参同口径）')
    ap.add_argument('--input', default='')
    ap.add_argument('--seed', type=int, default=0)
    ap.add_argument('--every', type=int, default=1)
    ap.add_argument('--debug', action='store_true')
    ap.add_argument('--draw', action='store_true', help='每帧也跑绘制链 0x4015a0（实验）')
    ap.add_argument('--enemies', action='store_true', help='每帧附敌人快照（EnemyManager 链表）')
    ap.add_argument('--bullets', action='store_true', help='每帧附敌弹快照（BulletManager 链表）')
    ap.add_argument('--items', action='store_true', help='每帧附道具快照（ItemManager 池）与得分/残机/得点值等资源全局')
    ap.add_argument('--lasers', action='store_true', help='每帧附激光快照（LaserManager 链表；曲线激光含采样点）')
    ap.add_argument('--bombs', action='store_true', help='每帧附 Bomb / 季节解放对象快照（DAT_004a6da8 / DAT_004a6da4）')
    ap.add_argument('--mode', type=int, default=0, help='DAT_004a5bec 模式位：0 普通 / 1 练习（过关走 0x43f500 结算）/ 2 符卡练习')
    ap.add_argument('--replay-slot', type=int, default=-1, help='DAT_004a57c8（默认 -1 走 MainFront；0 走 MainBossSpell）')
    ap.add_argument('--trace-ecl', default='', help='把每条执行的 ECL 指令 (frame, enemy, sub, pc, time, ins_time, op) 写到此文件')
    ap.add_argument('--trace-rng', default='', help='把回放同步 RNG（DAT_004a6d88）的每次调用 (frame, 步数, 返回地址, 包装外层调用方) 写到此文件')
    ap.add_argument('--trace-game-rng', action='store_true', help='--trace-rng 改为追踪游戏逻辑 RNG（DAT_004a6d80）')
    ap.add_argument('--trace-vm', default='', help='把每次 ANM VM 生成 (frame, AnmFile 指针, 脚本号, 0x407b20 返回地址) 写到此文件')
    a = ap.parse_args()
    o = GameOracle(pathlib.Path(a.exe), a.dat_dir, a.stage, a.shot, a.season, a.difficulty, a.seed, a.debug, a.replay_slot,
                   a.lives, a.mode)
    o.run_draw = a.draw
    o.enemy_snap = a.enemies
    o.bullet_snap = a.bullets
    o.item_snap = a.items
    o.laser_snap = a.lasers
    o.bomb_snap = a.bombs
    o.normalize_laser_graze_z()
    if a.trace_ecl:
        o.enable_ecl_trace()
    if a.trace_rng:
        o.enable_rng_trace(0x4A6D80 if a.trace_game_rng else 0x4A6D88)
    if a.trace_vm:
        o.enable_vm_trace()
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
        if a.refill_lives and a.lives is not None and o.i32(0x4A57F4) < a.lives:
            o.uc.mem_write(0x4A57F4, struct.pack('<i', a.lives))
        o.tick()
        if o.scene_exit is not None:
            # 游戏场景结束（回标题 / 结局 / 退出）：之后无 GameManager 可对拍，终止并把帧数记入输出
            print('game-oracle: scene request %#x at frame %d, stopping' % (o.scene_exit, f + 1), file=sys.stderr)
            break
        if (f + 1) % a.every == 0:
            frames.append(o.snapshot())
    notes = [e for e in o.errors if not e.startswith('note')]
    pathlib.Path(a.out).write_text(json.dumps({'stage': a.stage, 'shot': a.shot, 'season': a.season, 'difficulty': a.difficulty,
                                               'input': a.input, 'scene_exit': o.scene_exit, 'exit_frame': o.frame if o.scene_exit is not None else None,
                                               'frames': frames}, separators=(',', ':')))
    if a.trace_ecl:
        pathlib.Path(a.trace_ecl).write_text(json.dumps(o.ecl_trace, separators=(',', ':')))
    if a.trace_rng:
        pathlib.Path(a.trace_rng).write_text(json.dumps(o.rng_trace, separators=(',', ':')))
    if a.trace_vm:
        pathlib.Path(a.trace_vm).write_text(json.dumps(o.vm_trace, separators=(',', ':')))
    print('game-oracle: %d frames → %s digest=%016x enemies=%016x enemy_vms=%016x bullets=%016x items=%016x spell=%016x lasers=%016x bombs=%016x menu=%016x errors=%s' % (
        a.frames, a.out, frame_digest(frames),
        enemy_digest(frames) if a.enemies else 0,
        enemy_vm_digest(frames) if a.enemies else 0,
        bullet_digest(frames) if a.bullets else 0,
        item_digest(frames) if a.items else 0,
        spell_digest(frames) if a.enemies else 0,
        laser_digest(frames) if a.lasers else 0,
        bomb_digest(frames) if a.bombs else 0,
        menu_digest(frames),
        notes[:3]))
    return 0


if __name__ == '__main__':
    sys.exit(main())
