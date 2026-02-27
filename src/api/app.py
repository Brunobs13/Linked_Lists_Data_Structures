from __future__ import annotations

from pathlib import Path

from fastapi import FastAPI, HTTPException
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel, Field
from dotenv import load_dotenv

from .engine_client import EngineClient

BASE_DIR = Path(__file__).resolve().parents[2]
WEB_DIR = BASE_DIR / "web"
load_dotenv(BASE_DIR / ".env", override=True)


class LogRequest(BaseModel):
    level: str = Field(default="INFO", min_length=1, max_length=15)
    source: str = Field(default="api", min_length=1, max_length=63)
    message: str = Field(min_length=1, max_length=511)


class ProcessRequest(BaseModel):
    max_items: int = Field(default=0, ge=0, le=100000)


app = FastAPI(title="Lightweight Log Processing Engine", version="1.0.0")
engine = EngineClient()


@app.on_event("startup")
def startup_event() -> None:
    if not engine.initialize():
        raise RuntimeError(f"engine initialization failed: {engine.last_error()}")


@app.on_event("shutdown")
def shutdown_event() -> None:
    if not engine.shutdown():
        raise RuntimeError(f"engine shutdown failed: {engine.last_error()}")


@app.get("/health")
def health() -> dict:
    data = engine.health()
    if data.get("status") in {"degraded", "down"}:
        raise HTTPException(status_code=503, detail=data)
    return data


@app.post("/logs")
def post_logs(payload: LogRequest) -> dict:
    ok = engine.add_log(payload.level, payload.message, payload.source)
    if not ok:
        raise HTTPException(status_code=500, detail={"error": engine.last_error()})

    return {
        "status": "ok",
        "message": "log accepted",
    }


@app.get("/logs")
def get_logs() -> dict:
    data = engine.pending_logs()
    if "error" in data:
        raise HTTPException(status_code=500, detail=data)
    return data


@app.post("/process")
def process(payload: ProcessRequest) -> dict:
    data = engine.process_queue(payload.max_items)
    if data.get("status") == "error":
        raise HTTPException(status_code=500, detail=data)
    return data


@app.get("/metrics")
def metrics() -> dict:
    data = engine.metrics()
    if "error" in data:
        raise HTTPException(status_code=500, detail=data)
    return data


@app.get("/")
def dashboard() -> FileResponse:
    return FileResponse(WEB_DIR / "index.html")


app.mount("/static", StaticFiles(directory=WEB_DIR), name="static")
