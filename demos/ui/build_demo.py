"""Build the preserved offline replay from a checked-in, self-contained fixture."""
import json
from pathlib import Path

HERE = Path(__file__).resolve().parent

def build():
    payload = json.loads((HERE / "replay-data.json").read_text(encoding="utf-8"))
    if len(payload["frames"]) != 393 or len(payload["nodes"]) != 30 or len(payload["edges"]) != 98:
        raise ValueError("Unexpected demonstration fixture")
    serialized = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).replace("</", "<\\/")
    fragment = (HERE / "interface.html").read_text(encoding="utf-8").replace("/*REPLAY_DATA*/null", serialized)
    if "/*REPLAY_DATA*/" in fragment:
        raise RuntimeError("Missing replay template marker")
    dist = HERE / "dist"
    dist.mkdir(exist_ok=True)
    head = '<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>物流配送记录回放</title><style>body{margin:0;background:#edf1f5;padding:20px}@media(max-width:600px){body{padding:0}}</style></head><body>'
    (dist / "index.html").write_text(head + fragment + '</body></html>', encoding="utf-8")
    print("Built preserved replay: 393 frames, 30 nodes, 98 roads")

if __name__ == "__main__":
    build()
