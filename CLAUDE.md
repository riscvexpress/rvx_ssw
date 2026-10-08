# rvx_ssw — RVX System SoftWare

## Read this first

The RVX usage reference lives outside this repo. **Read it before doing anything
non-trivial here** — it is the authority on how RVX is driven and on the
bare-metal constraints this library must satisfy:

- `~/riscv_git/npx-claude/rvx/README.md` — workflow, full `make` target
  reference per level (platform / app / RTL sim / FPGA), build modes,
  **software development rules** (libc subset, fakefile, caching, multicore,
  debugging), feature usage (clocks, user IP, ILA), gotchas.
- `~/riscv_git/npx-claude/rvx/fpga_boards.md` — supported boards, cabling, FMC
  pin map, host drivers, printf terminal.
- `~/riscv_git/npx-claude/rvx_with_xarvis/README.md` — how a trained network
  reaches the platform (`util/npx`, fakefiles, `<app>.verify_only`).
- `~/claude_report/dca_accelerators.md` — DCA instruction contracts that the
  driver source does not state, with the symptoms each one produced.

`npx-claude` is a published repository with its own `CLAUDE.md`; follow it when
writing there. Where a new report goes (there or `~/claude_report/`) is decided
by `~/claude_report/README.md`.

The report's "Software development" section describes the *contract* this repo
implements. If code here and the report disagree, the code is the truth — say so
and update the report rather than silently working around it.

Upstream docs: <https://riscvexpress.github.io/>. Every page links to
`rvx.coreicc.net`, whose **TLS certificate is expired** — fetch the same path on
`riscvexpress.github.io` instead.

## What this repo is

The system software library for RVX SoCs — the implementation behind every
`ervp_*.h` header the SW manual tells application writers to include. It is
consumed by applications in a platform's `app/` directory; **it is not built
standalone**. The top-level `Makefile` has only an `init` target.

Sibling checkout: `~/rvx_release/rvx_ssw` (separate, usually at a different
commit). This one is the `rvx_shared` workspace copy, tracking
`git@bitbucket.org:kyuseung_han/rvx_ssw.git`, branch `master`.

## Layout

Every module follows the same shape:

```
<module>/
├── env/set_env.mh     ← how the build picks the module up
└── src/*.c *.h *.S
```

`set_env.mh` only appends to the build variables, always via `${RVX_SSW_HOME}`:

```make
INCLUDES += -I${RVX_SSW_HOME}/<module>/src
C_SRC    += $(wildcard ${RVX_SSW_HOME}/<module>/src/*.c)
ASM_SRC  += $(wildcard ${RVX_SSW_HOME}/<module>/src/*.S)
```

**When adding a module, add its `env/set_env.mh` too** — otherwise nothing
compiles it.

| Path | Contents |
|---|---|
| `system_utility/src` | the core runtime: `ervp_printf.h`, `ervp_malloc.h`, `ervp_assert.h`, `ervp_lock.h`, `ervp_barrier.h`, `ervp_variable_allocation.h`, `ervp_platform_api.h`, `ervp_fakefile.h` |
| `utility/src` | general helpers, incl. `ervp_stdlib.h` (`atoi`, `atof`) |
| `core/<cpu>/` | per-CPU code; each provides its own `core_dependent.h` (`flush_cache()`, `EXCLUSIVE_ID`). CPUs: `orca`, `orca_cache`, `orca_plus`, `rocket_big`, `rocket_opt`, `rocket_jtag`, `biriscv`, `swerv_eh1`, `irom` |
| `api/<peripheral>/` | peripheral drivers — `uart`, `spi`, `i2c`, `i2s`, `gpio`, `timer`, `dma`, `vdma`, `adc`, `plic`, `mmu`, `platform_controller`, `hdmi`, `oled_*`, `jpeg*`, `wifi`, `bluetooth`, `dca`, … |
| `memorymap/` | generated register maps: `*_memorymap.h` and `*_memorymap_offset.h` |
| `matrix/`, `npx/`, `mmiox/`, `image/` | compute/accelerator layers (NPX = neuromorphic extension) |
| `text_parser/`, `uthash/`, `darknet/`, `FreeRTOS/` | vendored third-party, each with its own LICENSE |
| `irom/` | boot ROM code |
| `deprecated/` | do not add to; do not use as reference |

`api/dca/` drives the matrix accelerators by pushing instruction structs into an
MMIOX1 instruction FIFO, so several instructions are in flight at once. Two
hardware contracts are not visible in the source — an instruction carrying
`..._OPCODE_PREDICATED` must not be pushed until the `..._OPCODE_FLAG_WRITE`
instruction that produced the predicate has *completed* (the flag is not
pipelined with the FIFO), and `..._OPCODE_LSRC_FEEDBACK` takes the left operand
from the previous instruction's ALU output rather than from the matrix named in
the instruction. Details and the symptoms they produce:
`~/claude_report/dca_accelerators.md`.

`matrix/` has its own `Makefile` that generates op variants (`add sub ewmult
mult mac conv max min compare transpose downsample asl asr`) and **errors out
unless `RVX_ETRI_HOME` is set** — source the right workspace setup first.

## Constraints on code written here

Bare metal, no OS. These are hard rules, not style preferences:

- **No `stdio.h`, no `stdlib.h`.** Use the `ervp_*` replacements. See the header
  swap table in the report.
- **C only.** Assembly is discouraged — behaviour varies by CPU. The RISC-V
  **"A" extension is not supported**, so no atomics; use `ervp_lock.h` /
  `ervp_barrier.h` for synchronization.
- **Bit-field variables must be declared `int` or `unsigned int`.**
- **No cache coherency between cores.** All cores run the same image and branch
  on `EXCLUSIVE_ID`. Cross-core sharing needs explicit `flush_cache()`.
- **`flush_cache()` is not a portable data-cache flush.** Read the CPU's
  `core_dependent.h` before relying on it: `orca_cache` / `orca_plus` really
  flush the D-cache, but on `rocket_opt`, `rocket_big` and `rocket_jtag` it is
  `fence.i` alone — instruction cache only. The data-cache flush there would be
  `cflush.d.l1` (`.word 0xfc000073`, sitting commented out in those headers);
  **it is not implemented in the RTL and executing it kills the core.**
- **Therefore consistency with a bus-mastering IP comes from placement, not from
  flushing.** On a rocket platform nothing in software can write back or
  invalidate the data cache, even though `CACHING_MOST` declares
  `0x00000000`–`0xBFFFFFFF` cacheable. Keep any buffer an IP reads or writes out
  of the cached region (`NOTCACHED_DATA`, or an on-chip scratchpad such as
  `npx/src/npx_spm.c`), and treat a fallback that puts such a buffer in cached
  DRAM as a latent correctness bug, not a slow path — `npx_layer_default.c`'s
  leaky layer stages the membrane potential into the scratchpad for exactly this
  reason.
- Global variables are not cached by default; placing one in a cacheable region
  (`CACHED_DATA`, `BIG_DATA`, `BIG_DATA_BSS` from `ervp_variable_allocation.h`)
  makes consistency management *mandatory and manual*, under every policy.
- pthreads are not supported.

## Working here

- `${RVX_SSW_HOME}` must point at this directory. `source ~/rvx_shared/rvx_setup.sh`
  sets it (along with `RVX_SHARED_HOME`, `RVX_UTIL_HOME`, `RVX_SYNTHESIZER_HOME`,
  `RVX_INIT_HOME`).
- **To actually compile or test a change**, go to a platform and build an app
  against it — e.g. `~/rvx_release/platform/tip_hello`: `make syn`, then
  `make sim_rtl && cd sim_rtl && make hello.all`. Editing here alone verifies
  nothing.
- Debugging is `printf`-based (`debug_printx/d/f/c/s`, `debug_print_line`) plus
  waveforms; **OpenOCD/GDB do not work** — no RVX CPU ships the RISC-V Debug
  Module.
- `memorymap/*.h` are generated — change the generator or the hardware
  description, not these files by hand.
- Header files here are ETRI-confidential. Do not paste their contents into
  external services or public issues.
- **Remove your own instrumentation before judging a result.** DCA bugs here are
  cache- and layout-sensitive, so a debug print or a CPU read of an
  accelerator-written buffer flips a pass into a fail and back. Confirm the
  marker is gone from the built copy (`<app>/edge-*.debug/previous/`) and from
  the `.elf`, not just from the source. When a symptom disappears right after you
  added observation, the first hypothesis is the observer effect, not a fix.
- **Restore experiment values.** A size dialled to 0 or a guard commented out to
  reproduce a failure still compiles and runs. Check the staged blob
  (`git show :<path>`), not the working file.
- **`git status` alone does not prove a file changed** — a touched mtime shows as
  modified until git refreshes the index. Confirm with `git diff HEAD -- <path>`
  or `md5sum`.
- **Regenerate what debugging consumed.** `<platform>/util/generated` is wiped by
  any `make <name>.verify_only` in `<platform>/util/npx`, and a verify loop can
  leave an app's `src/` without its fakefiles. Neither is tracked, so a clean
  `git status` does not mean the next session can build.

## Commit style

Short lines, lowercase, `o. <verb> - <what>`, one `o.` line per change:

```
o. bugfix - cacheline conflict
o. rename - matrix functions
o. update - npx direct encoding
```

The user commits — never offer to. Write the message from `git diff --cached`
across every staged file, including ones you did not touch, since the user edits
the same tree. Group by change rather than by file, and call out a rename that
breaks callers.
