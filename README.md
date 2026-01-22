# RISC-V LR/SC Reservation Set Probe

Empirical tool to measure LR/SC reservation granularity on RISC-V cores.

## ⚠️ WARNING

**This is NOT a substitute for vendor documentation.**

Empirical measurements can vary with:
- Die revisions
- Errata
- Environmental conditions (temperature, power states)
- System load

Use this for investigation only. Production systems require documented guarantees from vendors.

## The Problem

RISC-V allows reservation sets from 4 to 64 bytes. Both spec-compliant. Different behavior for lock-free code.

```c
struct {
    atomic_int counter;  // Thread A (LR/SC)
    int data;            // Thread B (store)
} foo;  // same cacheline
```

- 4B reservation: Thread B doesn't interfere
- 64B reservation: Thread B causes livelock

Public vendor documentation often doesn't specify which granularity a core implements.

## How It Works

Two threads:
- **Thread A:** Performs LR/SC operations on `target`
- **Thread B:** Continuously stores to `victim` (+32 bytes away)

If SC failure rate is high (>10%), reservation likely includes the victim field (≥64B).  
If SC failure rate is low (<1%), reservation likely excludes it (~4B).

## Usage

```bash
riscv64-linux-gnu-gcc -pthread -static -o lrsc_probe lrsc_probe.c
./lrsc_probe
```

Example output:
```
target: 0x96900
victim: 0x96920 (+32 bytes)

SC fails: 33 / 10000000 (0.0003%)
stores:   2554159
final:    10000000

→ Word-sized reservation (~4B)
```

## Results

### C906 (XuanTie)

**Processor:** XuanTie C906  
**Hardware:** Milk-V Duo  
**ISA:** RV64IMAFDC  
**Cache:** 64-byte cacheline, configurable I/D cache (8KB/16KB/32KB/64KB)  
**Pipeline:** 5-stage single-issue in-order  
**MMU:** Sv39  

**Architecture details:**
- Single-issue in-order execution
- Harvard L1 cache architecture
- 64-byte cache line size
- AXI4.0 128-bit Master interface
- Optional vector extension (V 0.7.1)

**Test result:** 0.0003% SC failure rate  
**Interpretation:** Word-sized reservation (~4 bytes)

**Analysis:**  
Despite having 64-byte cache lines, the C906 implements fine-grained LR/SC reservations. Thread B's stores at +32 bytes (same cacheline, different word) did not invalidate Thread A's reservations at measurable rates beyond normal scheduler/cache effects.

This suggests the reservation mechanism tracks at word granularity rather than cacheline granularity, even though cache coherency operates at 64B.

<img width="877" height="459" alt="image" src="https://github.com/user-attachments/assets/c7e17c5a-9066-46e1-81b2-21b0c8a58005" />

### Contribute

Have results from other cores? PRs welcome.

Please include:
- Core model and ISA configuration
- Hardware platform
- Cache architecture (line size, hierarchy)
- Pipeline details (in-order/out-of-order, issue width)
- Full probe output
- Any relevant vendor documentation links

## Cores With Public Documentation

### ✅ Explicitly Documented

- **MIPS P8700:** 64-byte cacheline reservation (Manual Section 3.2)

### ⚠️ Incomplete Public Documentation

- **SiFive U74:** Defines "reservation set" without stating size
- **C906 (XuanTie):** No public granularity specification (empirically measured as ~4B)

### ❓ Unknown

Most other cores—haven't surveyed their manuals.

**Know of others?** Please open an issue with:
- Core name
- Manual reference (section/page)
- Link to public documentation

## Technical Background

### Why Reservation Granularity Matters

LR/SC provides atomic read-modify-write operations. The "reservation" tracks a memory region:
- **LR (Load-Reserved):** Reads a value and marks a region as "reserved"
- **SC (Store-Conditional):** Writes only if reservation is still valid

The ISA doesn't mandate reservation size. Implementations vary:

**Word-sized (4-8 bytes):**
- Reservations track individual atomic variables
- Adjacent non-atomic stores don't interfere
- Better for lock-free data structures with mixed atomic/non-atomic fields

**Cacheline-sized (64 bytes):**
- Reservations track entire cache lines
- ANY store in the cacheline invalidates ALL reservations
- False sharing causes spurious SC failures
- Can cause livelocks in lock-free algorithms

**Impact on lock-free code:**
```c
struct {
    atomic_int lock_free_counter;  // 4 bytes at offset 0
    int normal_data[14];            // 56 bytes at offset 4
} shared;  // 60 bytes total, fits in one 64B cacheline
```

With word-sized reservation: safe to mix atomic and non-atomic fields.  
With cacheline-sized reservation: updates to `normal_data` break `lock_free_counter` operations.

### RISC-V Profile Constraints

RVA23 profile requires:
- Reservations are contiguous and naturally aligned
- Maximum size: 64 bytes
- Minimum size: not specified (could be 1 byte, though typically ≥4)

A core can implement 4B, 8B, 16B, 32B, or 64B and be fully compliant.

## Limitations

This probe has several limitations:

**Timing-dependent:**
- Relies on thread scheduling overlap
- Different scheduler policies may affect results
- Results can vary with system load

**Hardware-dependent:**
- Different behavior under thermal throttling
- Power management may affect cache/reservation behavior
- Multi-core vs single-core systems behave differently

**Not exhaustive:**
- Tests only one access pattern (+32B offset)
- Doesn't cover all possible invalidation conditions
- Doesn't test cross-core reservations thoroughly

**Interpretation challenges:**
- Low failure rate suggests small reservation, but doesn't prove it
- High failure rate could be scheduler effects, not hardware
- Need multiple test patterns for confirmation

## References
- [RISC-V ISA Spec - LR/SC](https://riscv.org/specifications/)
- [RVA23 Profile](https://github.com/riscv/riscv-profiles)
- [XuanTie C906](https://www.xrvm.com/product/xuantie/C906)

## License

MIT - Use freely, but remember: this measures behavior, not guarantees.

