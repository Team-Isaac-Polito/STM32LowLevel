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

**Prerequisites:** CMake ≥ 3.22, Ninja, `arm-none-eabi-gcc`

```bash
cd STM32LowLevel/STM32LowLevel
cmake --build build/MK2_MOD1 --parallel
```

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

