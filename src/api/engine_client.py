from __future__ import annotations

import ctypes
import json
import os
from typing import Any


class EngineClient:
    def __init__(self, library_path: str | None = None) -> None:
        path = library_path or os.environ.get("ENGINE_LIB_PATH", "build/liblog_engine.so")
        self._lib = ctypes.CDLL(path)

        self._lib.engine_init.argtypes = []
        self._lib.engine_init.restype = ctypes.c_int

        self._lib.engine_shutdown.argtypes = []
        self._lib.engine_shutdown.restype = ctypes.c_int

        self._lib.engine_add_log.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
        self._lib.engine_add_log.restype = ctypes.c_int

        self._lib.engine_get_pending_logs.argtypes = []
        self._lib.engine_get_pending_logs.restype = ctypes.c_char_p

        self._lib.engine_process_queue.argtypes = [ctypes.c_size_t]
        self._lib.engine_process_queue.restype = ctypes.c_char_p

        self._lib.engine_get_metrics.argtypes = []
        self._lib.engine_get_metrics.restype = ctypes.c_char_p

        self._lib.engine_health.argtypes = []
        self._lib.engine_health.restype = ctypes.c_char_p

        self._lib.engine_last_error.argtypes = []
        self._lib.engine_last_error.restype = ctypes.c_char_p

    def _decode_json(self, payload: bytes | None) -> dict[str, Any]:
        if not payload:
            return {"status": "error", "error": "empty response from engine"}

        text = payload.decode("utf-8", errors="replace")
        try:
            return json.loads(text)
        except json.JSONDecodeError:
            return {"status": "error", "error": "invalid json response", "raw": text}

    def initialize(self) -> bool:
        return bool(self._lib.engine_init())

    def shutdown(self) -> bool:
        return bool(self._lib.engine_shutdown())

    def last_error(self) -> str:
        payload = self._lib.engine_last_error()
        return payload.decode("utf-8", errors="replace") if payload else "unknown error"

    def add_log(self, level: str, message: str, source: str) -> bool:
        return bool(
            self._lib.engine_add_log(
                level.encode("utf-8"),
                message.encode("utf-8"),
                source.encode("utf-8"),
            )
        )

    def pending_logs(self) -> dict[str, Any]:
        return self._decode_json(self._lib.engine_get_pending_logs())

    def process_queue(self, max_items: int) -> dict[str, Any]:
        return self._decode_json(self._lib.engine_process_queue(max_items))

    def metrics(self) -> dict[str, Any]:
        return self._decode_json(self._lib.engine_get_metrics())

    def health(self) -> dict[str, Any]:
        return self._decode_json(self._lib.engine_health())
