# STM32LowLevel CAN bus testing toolkit

# ── Ensure libusb backend is available on ARM64 Windows ──────────────────────
# pyusb's auto-discovery doesn't find libusb-1.0.dll on ARM64 Windows.
# If the DLL exists next to the Python interpreter (or the base interpreter
# when running inside a venv), monkey-patch gs_usb to use it.
# This is a no-op on Linux or if the backend is already found.
import sys as _sys
import os as _os
import platform as _platform

if _platform.system() == "Windows" and _platform.machine() == "ARM64":
    # Check both venv prefix and base prefix (global install)
    _candidate_dirs = list(dict.fromkeys([_sys.prefix, _sys.base_prefix]))
    _dll_path = None
    for _d in _candidate_dirs:
        _p = _os.path.join(_d, "libusb-1.0.dll")
        if _os.path.isfile(_p):
            _dll_path = _p
            break

    if _dll_path:
        try:
            import usb.backend.libusb1 as _libusb1
            _backend = _libusb1.get_backend(find_library=lambda _x: _dll_path)
            if _backend is not None:
                # Patch gs_usb.gs_usb to use our backend
                import gs_usb.gs_usb as _gs
                _orig_scan = _gs.GsUsb.scan

                @classmethod
                def _patched_scan(cls):
                    import usb.core
                    devs = []
                    for dev in usb.core.find(
                        find_all=True,
                        custom_match=cls.is_gs_usb_device,
                        backend=_backend,
                    ):
                        devs.append(cls(dev))
                    return devs

                _gs.GsUsb.scan = _patched_scan
        except ImportError:
            pass
