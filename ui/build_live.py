"""Build the approved fleet interface and retain the historical single-car replay."""
from pathlib import Path
from build_demo import build

HERE = Path(__file__).resolve().parent


def main():
    build()
    dist = HERE / "dist"
    replay = (dist / "index.html").read_text(encoding="utf-8")
    replay = replay.replace('<span class="lp-preview">', '<a href="http://127.0.0.1:8765/" style="color:inherit">打开正式双车系统</a><span class="lp-preview">', 1)
    (dist / "replay.html").write_text(replay, encoding="utf-8")
    page = (HERE / "fleet.html").read_text(encoding="utf-8")
    page = page.replace('<head>', '<head><script>if(location.protocol==="file:")location.replace("http://127.0.0.1:8765/");</script>', 1)
    (dist / "index.html").write_text(page, encoding="utf-8")
    (dist / "style.css").write_text((HERE / "fleet.css").read_text(encoding="utf-8"), encoding="utf-8")
    (dist / "app.js").write_text((HERE / "live.js").read_text(encoding="utf-8"), encoding="utf-8")
    print(f"Built formal fleet UI and historical replay at {dist}")


if __name__ == '__main__':
    main()
