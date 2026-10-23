"""Create a local-only live UI without touching the course repository."""
from pathlib import Path
from build_demo import build

HERE = Path(__file__).resolve().parent

def main():
    build()
    dist = HERE / "dist"
    replay = (dist / "index.html").read_text(encoding="utf-8")
    replay = replay.replace('<span class="lp-preview">', '<a href="http://127.0.0.1:8765" style="color:inherit">打开手动插单版</a><span class="lp-preview">', 1)
    (dist / "replay.html").write_text(replay, encoding="utf-8")
    template = (HERE / "interface.html").read_text(encoding="utf-8")
    markup = template.split('<script>')[0]
    markup = markup.replace('UI 预览 · 待接入 C++', '独立 Demo · C++ 驱动')
    markup = markup.replace('开始回放', '开始仿真').replace('载入示例 / 重置', '重置仿真')
    markup = markup.replace('C++ 实际仿真记录 · 393 帧', '本机 C++ 实时仿真 · 手动插单')
    markup = markup.replace('剩余路段耗时', '本车本段剩余耗时')
    tasks = '<div class="lp-task-panel" aria-live="polite"><p><span class="lp-muted">当前任务：</span><strong data-view="current-task">正在读取…</strong></p><p><span class="lp-muted">任务路径：</span><span data-view="task-path">正在读取…</span></p></div>'
    markup = markup.replace('<p data-view="path"></p></div>', '<p data-view="path"></p>'+tasks+'</div>')
    extra = '<div><dt>本车本段总耗时</dt><dd data-view="segment-total">—</dd></div><div><dt>道路当前预计耗时</dt><dd data-view="road-now">—</dd></div><div><dt>本段预计到达</dt><dd data-view="segment-arrival">—</dd></div><div><dt>上段实际耗时</dt><dd data-view="last-trip">—</dd></div>'
    markup = markup.replace('<div><dt>下一订单</dt>', extra+'<div><dt>下一订单</dt>')
    markup = markup.replace('＋ 新增订单 · 表单预览', '＋ 手动添加动态订单')
    markup = markup.replace('<details class="lp-form-box">', '<details class="lp-form-box" open>')
    markup = markup.replace('<div class="lp-form-grid">', '<div class="lp-form-grid"><label>订单编号（可留空）<input name="id" maxlength="64" placeholder="自动生成编号"></label>')
    markup = markup.replace('校验输入', '提交动态订单')
    markup = markup.replace('表单未连接 C++，不会改变当前回放。', '提交即调用 C++ 检查全部约束；不会自动加载预设动态订单。')
    start = markup.index('  <section class="lp-timeline"')
    end = markup.index('  <div class="lp-bottom">', start)
    markup = markup[:start] + '<section class="lp-timeline"><strong data-view="live-time">08:00:00</strong><p class="lp-muted">地图标签：道路当前预计耗时。车辆在途使用入边时锁定的耗时。<a href="/replay.html">查看原记录回放</a></p></section>\n' + markup[end:]
    markup = markup.replace('真实数据回放 · 单车', 'C++ 实时交互 · 单车')
    markup = markup.replace('就绪，点击“开始回放”查看配送过程。', '正在连接本机仿真服务…')
    markup += '<style>#logistics-preview td small{white-space:normal;max-width:240px}#logistics-preview .lp-side{max-height:none}#logistics-preview .lp-main{align-items:start}#logistics-preview .lp-task-panel{margin-top:12px;padding-top:10px;border-top:1px solid var(--lp-line)}#logistics-preview .lp-task-panel p{color:var(--lp-text);font-size:13px;line-height:1.8;overflow-wrap:anywhere}#logistics-preview .lp-task-panel strong{color:var(--lp-teal);font-weight:500}</style>'
    source = (HERE / "interface.html").read_text(encoding="utf-8")
    draw = source[source.index('  function svgElement'):source.index('  function render()')]
    draw = draw.replace('x:(start.x+2*control.x+end.x)/4,y:(start.y+2*control.y+end.y)/4',
                        'x:(start.x+2*control.x+end.x)/4-uy*4,y:(start.y+2*control.y+end.y)/4+ux*4')
    code = (HERE / "live.js").read_text(encoding="utf-8").replace('  /*DRAW_NETWORK*/', draw)
    if '/*DRAW_NETWORK*/' in code:
        raise RuntimeError('draw substitution failed')
    (dist / "live.js").write_text(code, encoding="utf-8")
    head = '<!doctype html><html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>物流配送 · 手动动态订单</title><style>body{margin:0;padding:20px;background:#edf1f5}@media(max-width:600px){body{padding:0}}</style></head><body>'
    fallback = '<script>if(location.protocol==="file:")location.replace("http://127.0.0.1:8765/");</script>'
    (dist / "index.html").write_text(head+fallback+markup+'<script src="/live.js"></script></body></html>', encoding="utf-8")
    print('Built live demo and preserved offline replay at dist/replay.html')

if __name__ == '__main__':
    main()
