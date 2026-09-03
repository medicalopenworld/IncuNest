"""Thread-safe stdout/stderr capture for concurrent esptool.main() calls.

esptool writes straight to sys.stdout/sys.stderr, which are process-global —
this app talks to several boards at once (parallel flash threads, plus the
hotplug thread probing a device's flash size to resolve an ambiguous USB
identity), so a plain global swap would let one thread's esptool call route
output into (or clobber) another's. _EsptoolProxy is installed once as the
real sys.stdout/stderr and forwards each write to whatever writer the
*calling thread* has registered in `_tl`, so concurrent callers never see
each other's output.
"""
import io
import os
import sys
import threading
from typing import Optional

# Force plain-text esptool output for all threads (no ANSI / rich).
os.environ.setdefault('NO_COLOR', '1')

_tl = threading.local()          # per-thread writer slot
_lock = threading.Lock()
_proxy_instance: Optional['_EsptoolProxy'] = None


class _EsptoolProxy(io.TextIOBase):
    @property
    def encoding(self) -> str:
        return 'utf-8'

    def write(self, s: str) -> int:
        w = getattr(_tl, 'writer', None)
        if w is not None:
            return w.write(s)
        return len(s)  # no writer registered for this thread; discard

    def flush(self) -> None:
        w = getattr(_tl, 'writer', None)
        if w is not None:
            try:
                w.flush()
            except Exception:
                pass


def install() -> None:
    """Point sys.stdout/stderr at the proxy singleton, (re-)creating it if needed.

    Reassigns on every call (cheap) rather than a one-shot flag, so that if
    something else swaps sys.stdout/stderr later (e.g. a test runner's output
    capture) a subsequent call still routes esptool output correctly.
    """
    global _proxy_instance
    with _lock:
        if _proxy_instance is None:
            _proxy_instance = _EsptoolProxy()
        sys.stdout = _proxy_instance
        sys.stderr = _proxy_instance


def set_thread_writer(writer: Optional[io.TextIOBase]) -> None:
    """Register (or clear, with None) the current thread's esptool output sink."""
    install()
    _tl.writer = writer


class capture:
    """Context manager: collect this thread's esptool output into a buffer.

    Usage:
        with esptool_io.capture() as cap:
            esptool.main([...])
        text = cap.value
    """

    def __init__(self) -> None:
        self._buf = io.StringIO()

    def __enter__(self) -> 'capture':
        set_thread_writer(self._buf)
        return self

    def __exit__(self, *exc_info) -> None:
        set_thread_writer(None)

    @property
    def value(self) -> str:
        return self._buf.getvalue()
