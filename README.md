# STM32LowLevel

Low-level firmware for the **Rese.Q MK2** modular rescue robot, running on **STM32G474RET6**
boards inside each module.

> **Firmware version:** LL v1.0 (in development)
> **Board revision:** MK2 V3.0

Migration of [PicoLowLevel](https://github.com/Team-Isaac-Polito/PicoLowLevel) (RP2040/Arduino)
to the new MK2 V3.0 main board. The CAN bus protocol is unchanged — `reseq_ros2` on the
Jetson does not require modifications.

---

## Module Configurations

| Module | `MODULE_DEFINE` | CAN ID |
|---|---|---|
| Head | `MK2_MOD1` | `0x21` |
| Middle | `MK2_MOD2` | `0x22` |
| Tail | `MK2_MOD3` | `0x23` |

Pass the module at CMake configure time: `cmake --preset Debug -DMODULE_DEFINE=MK2_MOD1`

---

## Getting Started

Refer to [docs/getting-started.md](./docs/getting-started.md) for detailed environment setup, build, and flashing instructions.

**Prerequisites:** CMake ≥ 3.25 (required for workflows), Ninja, `arm-none-eabi-gcc`.

### Building & Flashing

If you only want to compile the code without triggering the flashing tool:
```bash
cd STM32LowLevel/STM32LowLevel
cmake --preset MK2_MOD1 
cmake --build --preset MK2_MOD1
```

For release builds (optimized, debug logging stripped out):
```bash
cmake --preset MK2_MOD1-release 
cmake --build --preset MK2_MOD1-release
```

If you want to compile and flash your code directly to your target module using integrated CMake Workflows:
```bash
# Flash Debug build (Includes runtime logging)
cmake --workflow --preset MK2_MOD1-flash

# Flash Release build (Optimized, logs compiled out)
cmake --workflow --preset MK2_MOD1-release-flash
```

*Change MK2_MOD1 to MK2_MOD2 (Middle) or MK2_MOD3 (Tail) depending on your target board.*

---

## Contributing

1. Pick an open issue, create a branch: `feat/<desc>`, `fix/<desc>`, `docs/<desc>`, or `chore/<desc>`
2. Commit using [Conventional Commits](https://www.conventionalcommits.org/): `feat(scope): description`
3. Open a PR — CI must pass, use **standard merge**

See [git-commit-best-practices](https://docs.teamisaac.it/doc/git-commit-best-practices-BqDErR7HJw) for the full commit convention.

---

## Documentation

- [MK2 V3.0 Hardware](https://docs.teamisaac.it/doc/v30-q6b3dI1ap7) — Team Isaac Outline
- [PicoLowLevel](https://docs.teamisaac.it/doc/picolowlevel-WnfPCgwOM6) — previous firmware (RP2040)

