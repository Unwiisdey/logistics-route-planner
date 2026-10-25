"""Draw the current network as a standalone SVG using the Python standard library."""
import json
import math
from html import escape
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HERE = ROOT / "docs/maps"
HERE.mkdir(parents=True, exist_ok=True)
source = json.loads((ROOT / "config/network.json").read_text(encoding="utf-8"))
nodes = {n["id"]: n for n in source["nodes"]}
positions = {n["id"]: (n["x"]*100+100, 790-n["y"]*100) for n in source["nodes"]}
backbone = ["W01", "T01", "D12", "T02", "T03", "D20", "W02"]
links = sorted({tuple(sorted((e["from_id"], e["to_id"]))) for e in source["edges"]})
directions = {(e["from_id"],e["to_id"]) for e in source["edges"]}
assert all((b,a) in directions for a,b in directions), "this overview expects bidirectional roads"
main_links = {tuple(sorted((a, b))) for a, b in zip(backbone, backbone[1:])}
highlight = ("D12", "D18")
assert set(nodes) == set(positions) and len(nodes) == 30
assert all(a != b and a in nodes and b in nodes for a, b in links)
assert len(links) * 2 >= 50

# Verify a connected graph and an unambiguous planar drawing.
adj = {n: set() for n in nodes}
for a, b in links:
    adj[a].add(b); adj[b].add(a)
seen, queue = set(), ["W01"]
while queue:
    n = queue.pop()
    if n not in seen:
        seen.add(n); queue.extend(adj[n] - seen)
assert seen == set(nodes)

def orient(a, b, c):
    return (b[0]-a[0])*(c[1]-a[1]) - (b[1]-a[1])*(c[0]-a[0])

crossings = []
for i, (a, b) in enumerate(links):
    for c, d in links[i+1:]:
        if {a, b}.intersection((c, d)):
            continue
        if orient(positions[a],positions[b],positions[c])*orient(positions[a],positions[b],positions[d]) < 0 and orient(positions[c],positions[d],positions[a])*orient(positions[c],positions[d],positions[b]) < 0:
            crossings.append((a,b,c,d))
assert not crossings, crossings

def point(n):
    x, y = positions[n]
    return x + 55, y + 90

parts = ['''<svg xmlns="http://www.w3.org/2000/svg" width="1600" height="1100" viewBox="0 0 1600 1100" role="img" aria-labelledby="title desc">
<title id="title">简化城市配送路网方案</title><desc id="desc">30 个节点，按三个片区的支路环线、片区间连接主路和少量捷径组织；绿色为 D12 与 D18 的直接双向连接。根据当前 network.json 绘制，仓库、中转站与配送点均有标注。</desc>
<defs><filter id="shadow" x="-30%" y="-30%" width="160%" height="170%"><feDropShadow dx="0" dy="2" stdDeviation="3" flood-color="#23455e" flood-opacity=".09"/></filter>
''']
colors = {"branch": "#bac9d5", "main": "#5d7e9c", "shortcut": "#208c77"}
for kind, color in colors.items():
    parts.append(f'<marker id="arrow-{kind}" viewBox="0 0 8 8" refX="6" refY="4" markerWidth="7" markerHeight="7" orient="auto-start-reverse" markerUnits="userSpaceOnUse"><path d="M1 1L7 4L1 7Z" fill="{color}"/></marker>')
parts.append('''</defs><style>text{font-family:"Microsoft YaHei","Segoe UI",sans-serif}.node-label{paint-order:stroke;stroke:#fcfdff;stroke-width:6px;stroke-linejoin:round}</style>
<rect width="1600" height="1100" fill="#f3f6fa"/><rect width="1600" height="120" fill="#1b354c"/>
<text x="42" y="43" fill="#a8bed0" font-size="14" letter-spacing="2">LOGISTICS / SIMPLIFIED ROAD NETWORK</text>
<text x="40" y="88" fill="white" font-size="31" font-weight="600">简化城市路网 · 主路、支路与少量捷径</text>
<rect x="1315" y="39" width="242" height="39" rx="20" fill="#29465f" stroke="#6e879b"/><text x="1436" y="65" fill="#dae7f3" font-size="16" text-anchor="middle">当前路网 · 配置文件驱动</text>
<rect x="32" y="146" width="1536" height="812" rx="20" fill="#fcfdff" stroke="#dfe7ef"/>
<path d="M97 418Q75 277 225 267L455 294Q552 315 563 450L553 797Q549 914 391 928L180 913Q72 902 85 762Z" fill="#f0f5fa"/>
<path d="M557 286Q580 215 726 217L909 247Q1000 260 1000 388L950 809Q914 928 752 926L590 909Q525 886 551 756Z" fill="#f5f7fa"/>
<path d="M1000 269Q1090 222 1293 277Q1449 296 1451 491L1433 786Q1404 935 1202 922L1039 910Q959 880 969 739Z" fill="#eff6f5"/>
<text x="72" y="194" fill="#4d6b86" font-size="21" font-weight="600">简明配送道路图</text>
''')
parts.append(f'<text x="1519" y="192" text-anchor="end" fill="#8094a7" font-size="17">30 节点 · {len(links)} 组双向连接（{len(links)*2} 条有向边）</text>')
parts += ['<text x="285" y="258" fill="#a2b1c0" font-size="18">西部片区</text>',
          '<text x="726" y="245" fill="#a5b0be" font-size="18">中部片区</text>',
          '<text x="1266" y="235" fill="#9bb2ab" font-size="18">东部片区</text>']

def draw_link(a, b, kind):
    x1,y1 = point(a); x2,y2=point(b)
    length=math.hypot(x2-x1,y2-y1); dx=(x2-x1)/length;dy=(y2-y1)/length
    x1+=17*dx;y1+=17*dy;x2-=17*dx;y2-=17*dy
    if kind == "shortcut":
        parts.append(f'<path d="M{x1} {y1}L{x2} {y2}" stroke="#d8eee7" stroke-width="15" fill="none" stroke-linecap="round"/>')
    width={"branch":2.6,"main":5,"shortcut":4.5}[kind]
    parts.append(f'<path d="M{x1} {y1}L{x2} {y2}" fill="none" stroke="{colors[kind]}" stroke-width="{width}" stroke-linecap="round" marker-start="url(#arrow-{kind})" marker-end="url(#arrow-{kind})"><title>{escape(a)} ↔ {escape(b)}</title></path>')

for a,b in links:
    if (a,b) != highlight:
        draw_link(a,b,"main" if (a,b) in main_links else "branch")
draw_link(*highlight,"shortcut")

for n, node in nodes.items():
    x,y = point(n); kind=node["type"]; focus=n in highlight
    if focus: parts.append(f'<circle cx="{x}" cy="{y}" r="18" fill="#dcefe8"/>')
    if kind == "warehouse":
        parts.append(f'<rect x="{x-13}" y="{y-13}" width="26" height="26" rx="6" fill="#426c91" stroke="white" stroke-width="3"/>')
        label_y=y-23;label_x=x
    elif kind == "transfer_station":
        parts.append(f'<path d="M{x} {y-14}L{x+14} {y}L{x} {y+14}L{x-14} {y}Z" fill="#d0a15e" stroke="white" stroke-width="3"/>')
        label_y=y-24;label_x=x
    else:
        parts.append(f'<circle cx="{x}" cy="{y}" r="9" fill="{colors["shortcut"] if focus else "white"}" stroke="{colors["shortcut"] if focus else "#8ca3b7"}" stroke-width="2.2"/>')
        label_y=y+6;label_x=x+17
    parts.append(f'<text class="node-label" x="{label_x}" y="{label_y}" text-anchor="{"middle" if kind!="delivery_point" else "start"}" fill="{colors["shortcut"] if focus else "#416079"}" font-size="{19 if kind!="delivery_point" else 18}" font-weight="{600 if focus or kind!="delivery_point" else 500}">{n}</text>')

x1,y1=point("D12");x2,y2=point("D18")
mx,my=(x1+x2)/2,(y1+y2)/2
parts.append(f'<rect x="{mx-52}" y="{my-43}" width="176" height="34" rx="8" fill="#208c77" filter="url(#shadow)"/><text x="{mx+36}" y="{my-20}" fill="white" text-anchor="middle" font-size="16" font-weight="600">捷径 D12 ↔ D18</text>')
parts.append('''
<text x="889" y="579" fill="#5c7f9c" font-size="16" style="paint-order:stroke;stroke:#fcfdff;stroke-width:6px">片区连接主路</text>
<line x1="62" y1="1006" x2="105" y2="1006" stroke="#5d7e9c" stroke-width="5"/><text x="119" y="1012" fill="#70899d" font-size="17">连接主路</text>
<line x1="315" y1="1006" x2="358" y2="1006" stroke="#bac9d5" stroke-width="3"/><text x="372" y="1012" fill="#70899d" font-size="17">片区支路 / 联络路</text>
<line x1="655" y1="1006" x2="698" y2="1006" stroke="#208c77" stroke-width="5"/><text x="712" y="1012" fill="#70899d" font-size="17">斜向捷径</text>
<rect x="963" y="996" width="20" height="20" rx="4" fill="#426c91"/><text x="997" y="1012" fill="#70899d" font-size="17">仓库</text>
<circle cx="1126" cy="1006" r="8" fill="white" stroke="#8ca3b7" stroke-width="2"/><text x="1147" y="1012" fill="#70899d" font-size="17">配送点</text>
<path d="M1300 995L1311 1006L1300 1017L1289 1006Z" fill="#d0a15e"/><text x="1323" y="1012" fill="#70899d" font-size="17">中转站</text>
<text x="43" y="1068" fill="#8e9faf" font-size="15">来源：config/network.json。每条连线表示双向通行，两个方向的耗时和成本可以不同；坐标单位为 km。</text>
</svg>''')
(HERE/"network.svg").write_text("\n".join(parts),encoding="utf-8")
print(f"Generated docs/maps/network.svg: {len(nodes)} nodes, {len(links)} bidirectional links / {len(links)*2} directed edges; connected; no unmarked crossings.")
