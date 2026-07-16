#!/usr/bin/env python3

import argparse
import json
import re
from collections import Counter
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

from generate_explore_map import ESSENTIALS, export_map, render_map
from generate_landmark_map_run import LANDMARK_MAP_SPECS
from generate_typical_map_run import build_map


ROOT = Path(__file__).resolve().parents[1]
FONT_PATH = Path("/System/Library/Fonts/STHeiti Medium.ttc")
FOREST_IDS = set(range(800, 804)) | set(range(808, 812)) | set(range(816, 820))
FENCE_IDS = {1658, 1662, 1664, 1665, 1666, 1681, 1682}
LIGHTHOUSE_IDS = {
    3941, 3942, 3943,
    3949, 3950, 3951,
    3957, 3958, 3959,
    3965, 3966, 3967,
    3973, 3974, 3975,
    3981, 3982, 3983,
}

CATEGORY_LABELS = {
    "route": "户外路线",
    "settlement": "城镇/户外设施",
    "wild": "公园/野外区域",
    "island": "岛屿/海岸",
    "cave": "洞窟/水下",
    "service": "商店/服务设施",
    "arena": "道馆/竞技设施",
    "interior": "住宅/普通室内",
    "special": "特殊场景",
}


def load_font(size):
    if FONT_PATH.exists():
        return ImageFont.truetype(str(FONT_PATH), size)
    return ImageFont.load_default()


def classify_map(data):
    name = data["name"].lower()
    tileset = data["tilesetName"].lower()
    if data["mapId"] == 1:
        return "special"
    if tileset == "outside":
        if "island" in name or "harbor" in name:
            return "island"
        if "park" in name or "safari" in name:
            return "wild"
        if "route" in name or "plateau" in name or "region" in name:
            return "route"
        return "settlement"
    if tileset in ("caves", "dungeon cave", "underwater"):
        return "cave"
    if "harbour" in tileset:
        return "island"
    if (
        "arena" in name
        or "gym" in name
        or "tower" in name
        or "stadium" in name
        or tileset in ("gyms interior", "trainer tower interior")
    ):
        return "arena"
    if (
        "mart" in name
        or "center" in name
        or "dept." in name
        or "game corner" in name
        or "poké center" in name
        or "poke center" in name
    ):
        return "service"
    return "interior"


def used_autotiles(data, tile_counts):
    names = set()
    for tile_id in tile_counts:
        if not 48 <= tile_id < 384:
            continue
        index = tile_id // 48 - 1
        if 0 <= index < len(data["autotileNames"]):
            name = data["autotileNames"][index]
            if name:
                names.add(name)
    return names


def detect_features(data, tile_counts, category):
    features = []
    is_outside = data["tilesetName"] == "Outside"
    autotiles = used_autotiles(data, tile_counts)
    autotile_text = " ".join(autotiles).lower()
    terrain_counts = Counter()
    for tile_id, count in tile_counts.items():
        if 0 <= tile_id < len(data["terrainTags"]):
            terrain_counts[data["terrainTags"][tile_id]] += count

    if is_outside and (terrain_counts[3] or any(536 <= tile_id <= 559 for tile_id in tile_counts)):
        features.append("道路")
    if is_outside and tile_counts[390]:
        features.append("草丛")
    if is_outside and tile_counts[391]:
        features.append("前景高草")
    if is_outside and FOREST_IDS & tile_counts.keys():
        features.append("森林边界")
    if is_outside and FENCE_IDS & tile_counts.keys():
        features.append("栅栏")
    if (
        any(word in autotile_text for word in ("sea", "water", "shore", "pond"))
        or (is_outside and any(392 <= tile_id <= 414 for tile_id in tile_counts))
    ):
        features.append("水域")
    if "waterfall" in autotile_text:
        features.append("瀑布")
    if is_outside and LIGHTHOUSE_IDS & tile_counts.keys():
        features.append("灯塔")
    if category == "cave":
        features.append("岩壁分层")
    if category in ("interior", "service", "arena"):
        features.append("室内分区")
    if not features:
        features.append("室内分区" if category in ("interior", "service", "arena") else "基础地形")
    return features, sorted(autotiles)


def layer_coverage(data):
    cells = data["width"] * data["height"]
    return [
        sum(1 for tile_id in layer if tile_id >= 48) / cells
        for layer in data["layers"]
    ]


def fit_image(image, width, height, background=(27, 36, 39, 255)):
    scale = min(width / image.width, height / image.height)
    target = (
        max(1, int(image.width * scale)),
        max(1, int(image.height * scale)),
    )
    resized = image.resize(target, Image.Resampling.NEAREST)
    canvas = Image.new("RGBA", (width, height), background)
    x = (width - resized.width) // 2
    y = (height - resized.height) // 2
    canvas.alpha_composite(resized, (x, y))
    return canvas


def make_thumbnail(image, data, category):
    thumb = fit_image(image, 280, 168)
    canvas = Image.new("RGBA", (292, 222), (27, 36, 39, 255))
    canvas.alpha_composite(thumb, (6, 42))
    draw = ImageDraw.Draw(canvas)
    draw.text(
        (8, 6),
        f"Map{data['mapId']:03d}  {data['name']}",
        font=load_font(15),
        fill=(255, 222, 92, 255),
    )
    draw.text(
        (8, 25),
        f"{data['width']}x{data['height']}  {data['tilesetName']}  {CATEGORY_LABELS[category]}",
        font=load_font(11),
        fill=(190, 207, 205, 255),
    )
    draw.rectangle((0, 0, 291, 221), outline=(75, 94, 97, 255))
    return canvas


def make_all_map_atlases(records, output_dir):
    columns = 4
    rows = 4
    per_page = columns * rows
    pages = []
    for page_index in range((len(records) + per_page - 1) // per_page):
        page_records = records[page_index * per_page:(page_index + 1) * per_page]
        canvas = Image.new("RGBA", (columns * 300 + 20, rows * 230 + 62), (20, 28, 31, 255))
        draw = ImageDraw.Draw(canvas)
        draw.text(
            (14, 12),
            f"Pokemon Essentials 全地图扫描  {page_index + 1}/{(len(records) + per_page - 1) // per_page}",
            font=load_font(24),
            fill=(255, 225, 100, 255),
        )
        for index, record in enumerate(page_records):
            x = 10 + (index % columns) * 300
            y = 52 + (index // columns) * 230
            canvas.alpha_composite(record["thumbnail"], (x, y))
        path = output_dir / f"all_maps_{page_index + 1:02d}.png"
        canvas.save(path, optimize=True)
        pages.append(path)
    return pages


def make_summary_chart(records, output_dir):
    category_counts = Counter(record["category"] for record in records)
    tileset_counts = Counter(record["tileset"] for record in records)
    canvas = Image.new("RGBA", (1120, 720), (22, 31, 34, 255))
    draw = ImageDraw.Draw(canvas)
    title_font = load_font(28)
    label_font = load_font(15)
    value_font = load_font(15)
    draw.text((34, 24), "69 张地图结构统计", font=title_font, fill=(255, 225, 100, 255))

    def bars(items, left, top, width, title, color):
        draw.text((left, top), title, font=load_font(21), fill=(235, 241, 239, 255))
        max_value = max(value for _name, value in items)
        for index, (name, value) in enumerate(items):
            y = top + 44 + index * 42
            draw.text((left, y), name, font=label_font, fill=(205, 218, 216, 255))
            bar_left = left + 190
            bar_width = int((width - 240) * value / max_value)
            draw.rectangle((bar_left, y + 2, bar_left + bar_width, y + 24), fill=color)
            draw.text((bar_left + bar_width + 10, y + 2), str(value), font=value_font, fill=(255, 255, 255, 255))

    category_items = [
        (CATEGORY_LABELS[key], category_counts[key])
        for key in CATEGORY_LABELS
        if category_counts[key]
    ]
    tileset_items = tileset_counts.most_common(9)
    bars(category_items, 34, 82, 510, "地图类别", (70, 154, 224, 255))
    bars(tileset_items, 580, 82, 510, "主要 Tileset", (86, 184, 125, 255))

    outdoor = sum(category_counts[key] for key in ("route", "settlement", "wild", "island"))
    multi_layer = sum(1 for record in records if record["layerCoverage"][1] > 0.01 or record["layerCoverage"][2] > 0.01)
    high_grass = sum(1 for record in records if "前景高草" in record["features"])
    water = sum(1 for record in records if "水域" in record["features"])
    footer = f"户外地图 {outdoor}    使用明显上层构图 {multi_layer}    含水域 {water}    含前景高草 {high_grass}    灯塔实地图 0"
    draw.text((34, 674), footer, font=load_font(18), fill=(255, 203, 90, 255))
    path = output_dir / "scan_summary.png"
    canvas.save(path, optimize=True)
    return path


def make_rule_board(records_by_id, output_dir, filename, title, items):
    columns = 3
    rows = 2
    cell_w = 370
    cell_h = 300
    canvas = Image.new("RGBA", (columns * cell_w + 20, rows * cell_h + 72), (21, 30, 33, 255))
    draw = ImageDraw.Draw(canvas)
    draw.text((18, 14), title, font=load_font(26), fill=(255, 225, 100, 255))
    for index, (map_id, caption) in enumerate(items):
        record = records_by_id[map_id]
        image = render_map(record["data"])
        preview = fit_image(image, cell_w - 18, 222)
        x = 10 + (index % columns) * cell_w
        y = 60 + (index // columns) * cell_h
        canvas.alpha_composite(preview, (x + 4, y + 28))
        draw.text(
            (x + 8, y + 4),
            f"Map{map_id:03d} {record['name']}",
            font=load_font(16),
            fill=(239, 244, 242, 255),
        )
        draw.text((x + 8, y + 256), caption, font=load_font(15), fill=(133, 205, 255, 255))
        draw.rectangle((x, y, x + cell_w - 10, y + cell_h - 8), outline=(73, 91, 94, 255))
    path = output_dir / filename
    canvas.save(path, optimize=True)
    return path


def make_layer_board(record, output_dir, filename, title, notes):
    data = record["data"]
    images = [render_map(data)]
    for layer in data["layers"]:
        layer_data = dict(data)
        layer_data["layers"] = [layer]
        images.append(render_map(layer_data))
    labels = ("合成", "Layer 0 地面", "Layer 1 结构/前景", "Layer 2 遮挡")
    canvas = Image.new("RGBA", (1120, 720), (21, 30, 33, 255))
    draw = ImageDraw.Draw(canvas)
    draw.text((18, 14), title, font=load_font(26), fill=(255, 225, 100, 255))
    for index, image in enumerate(images):
        x = 12 + (index % 2) * 554
        y = 58 + (index // 2) * 296
        panel = fit_image(image, 540, 244, background=(44, 54, 57, 255))
        canvas.alpha_composite(panel, (x, y + 26))
        draw.text((x + 4, y + 2), labels[index], font=load_font(17), fill=(230, 238, 236, 255))
    draw.text((18, 670), notes, font=load_font(16), fill=(132, 202, 255, 255))
    path = output_dir / filename
    canvas.save(path, optimize=True)
    return path


def write_report(records, atlas_pages, image_paths, output_dir):
    category_counts = Counter(record["category"] for record in records)
    tileset_counts = Counter(record["tileset"] for record in records)
    maps_with_high_grass = [record for record in records if "前景高草" in record["features"]]
    maps_with_forest = [record for record in records if "森林边界" in record["features"]]
    maps_with_water = [record for record in records if "水域" in record["features"]]
    maps_with_lighthouse = [record for record in records if "灯塔" in record["features"]]

    lines = [
        "# Pokemon Essentials 69 张地图扫描与生成规则",
        "",
        "> 扫描日期：2026-07-13。来源：Pokemon Essentials v21.1。69/69 个 `MapXXX.rxdata` 均成功解析和渲染。",
        "",
        "## 关键结论",
        "",
        "- 当前草地灯塔构图不成立：灯塔素材虽然完整，但小池塘不足以建立海岸语义。灯塔应位于连续海域中的近岸基座、礁石平台、岛屿或防波堤上。",
        f"- `Outside.png` 确实包含 `3941～3983` 的 3x6 白色灯塔模块，但 69 张示例地图中实际使用灯塔的地图数量为 **{len(maps_with_lighthouse)}**。因此灯塔位置必须从 Route 4、Route 6、Route 8、港口和岛屿的海岸规则推导。",
        "- Essentials 的地图不是按装饰块随机铺设，而是先确定边界、入口/出口、主路线和功能节点，再用草丛、水域、森林、建筑及前景层填充剩余空间。",
        "- 多出口地图保留所有有效路线；路线应共享入口侧主干，并保证每段道路通向出口、建筑门口或明确事件点。",
        "",
        f"![扫描统计]({image_paths['summary'].name})",
        "",
        "## 全量统计",
        "",
        f"- 地图总数：{len(records)}",
        f"- 户外路线：{category_counts['route']}；城镇/户外设施：{category_counts['settlement']}；公园/野外区域：{category_counts['wild']}；岛屿/海岸：{category_counts['island']}",
        f"- 洞窟/水下：{category_counts['cave']}；普通室内：{category_counts['interior']}；商店/服务设施：{category_counts['service']}；道馆/竞技设施：{category_counts['arena']}",
        f"- 含水域：{len(maps_with_water)}；含森林边界：{len(maps_with_forest)}；含 Layer 1 前景高草：{len(maps_with_high_grass)}",
        "- Tileset 使用量：" + "；".join(f"{name} {count}" for name, count in tileset_counts.most_common()),
        "",
        "## 通用生成规则",
        "",
        "### 1. 先画拓扑，再画景观",
        "",
        "先确定入口、1～2 个出口、建筑门口、楼梯和事件点，再生成连接这些节点的主路线。剩余区域才分配草丛、水域、森林和装饰。不得先铺大面积道路，再反向寻找可走路线。",
        "",
        "### 2. 边界必须有语义",
        "",
        "户外地图通常用森林、悬崖、海洋、围栏或建筑连续封边，只在实际转场位置留下窄开口。屏幕边缘不能出现被截断的独立树顶、树根或半个建筑。",
        "",
        "### 3. 多出口道路共享主干",
        "",
        "多个出口可以同时存在，但路线应先共享入口侧主干，再在靠近出口处形成最短分支。每段道路必须通往入口、出口、门口或事件点；没有用途的道路属于浪费。道路宽度由玩法决定，StickMon 保持两格宽并允许精灵沿两格中线自由移动。",
        "",
        "### 4. 地形必须形成连续区域",
        "",
        "道路、草丛、水面和山壁使用完整边缘、转角、内部与收口组合。禁止把内部 tile 直接贴到普通草地，也不能用池塘模块冒充海洋。小水面使用池塘边缘；海岸使用 Sea deep -> Sea edge -> shore -> land 的连续过渡。",
        "",
        "### 5. 草有两种高度语义",
        "",
        "`390` 是 Layer 0 的普通可遇敌草；`391` 是 Map028 使用的 Layer 1 前景高草，具有 `priority=1` 和 `terrainTag=10`。高草应成片分布在道路两侧，不能盖住出口、道路、水面、灯塔或建筑门口。",
        "",
        "### 6. 森林是多行模板",
        "",
        "Map039 的森林由 Layer 2 树冠、Layer 1 栅栏、Layer 0 接缝行、树顶/树身对和根部组成。根部上一行必须是树身；位于地图底部时仍要保持完整序列，不能把树顶直接接根部。",
        "",
        "### 7. 地标需要环境支撑",
        "",
        "地标不应孤立摆放。建筑门口需要前庭和道路；喷泉需要广场；灯塔需要连续水域、海岸或堤岸；桥梁两端需要可达道路。地标周围应留出可识别轮廓和交互空间。",
        "",
        "### 8. 建筑门口对齐道路",
        "",
        "城镇建筑通常沿道路或广场布置，门口前至少留出 1～2 格净空。屋顶、招牌和树冠可进入上层，但碰撞仍由底层结构和 passage 决定。",
        "",
        "### 9. 室内保持中央通道",
        "",
        "住宅、商店和设施多采用矩形房间：墙体封边，家具沿墙或围绕功能区布置，入口到柜台、楼梯或事件点的中央通道保持畅通。竞技场和多层商店则使用明显的轴线与对称布局。",
        "",
        "### 10. 洞窟按高度带组织",
        "",
        "洞窟用连续岩壁带、台阶和窄口表达高度变化。岩石用于修整轮廓和控制通路宽度，不能随机散落到主路中央。楼梯必须连接两个可读的高度区域；楼梯所在的连续崖带必须保持同一高度，不得直接邻接另一段异高悬崖。",
        "",
        "### 11. 三层图块语义不能互换",
        "",
        "Layer 0 放地面和主体；Layer 1 放高草、围栏、建筑与可遮挡结构；Layer 2 放树冠、屋檐和更高前景。`passages`、`priorities`、`terrainTags` 必须随原 tile 保留。",
        "",
        "### 12. StickMon 的裁图规则",
        "",
        "先在更大的逻辑地图上生成完整拓扑，再选择 16x12 格镜头区域。镜头内必须同时看见当前位置、至少一段前进方向和一个景观锚点；不要为了塞满屏幕而截断树、建筑或海岸过渡。",
        "",
        "## 代表性地图",
        "",
        f"![户外路线规则]({image_paths['routes'].name})",
        "",
        f"![城镇与野外规则]({image_paths['settlements'].name})",
        "",
        f"![海岸与洞窟规则]({image_paths['coasts'].name})",
        "",
        f"![室内规则]({image_paths['interiors'].name})",
        "",
        "## 应用到 StickMon：溪流、桥梁与坡壁",
        "",
        "### 溪流主体",
        "",
        "- 竖向溪流使用 `Outside.png` 的左岸 `1096`、水面 `1097` 和右岸 `1098`，最小宽度为 3 格。",
        "- 溪流可以保持直线，也可分段变宽或变窄；禁止在宽度不变时只横向平移整条水道。",
        "- 每个分段边界上，单侧河岸最多移动 1 格。更大的宽度变化必须拆成多个渐进分段。",
        "",
        "### 岸角拼接",
        "",
        "- 溪流变宽或变窄时不能用草地直接截断竖直岸线，每一侧必须使用“外角 + 内角”两个 tile 连续收口。",
        "- 向屏幕下方变宽使用 Essentials 原生上岸角：外角 `1088/1090`，内角 `1104/1106`。",
        "- 向屏幕下方收窄使用上述岸角的纵向翻转派生 tile `4400～4403`，保留相同泥土质感，并继承原 tile 的 `passages`、`priorities` 和 `terrainTags=6`。",
        "",
        "### 桥梁、石块与语义",
        "",
        "- 桥面覆盖行及其上下各 2 格是直流缓冲区：溪流的左岸位置和宽度必须保持不变，转折、变宽和变窄都不能发生在桥下或桥边。",
        "- 水中石块必须完全位于水面中，不得覆盖桥梁或路线。所有溪岸转角格及其周围 1 格均为石块禁放区，避免高层石块帧遮挡泥土岸角。",
        "- 桥面是陆地通行面；普通溪水保持水面和漂浮通行语义；水中石块同时阻挡陆地与漂浮移动。",
        "",
        "### 坡壁与楼梯",
        "",
        "- 横向坡壁使用 `Outside.png` 的崖顶 `1188`、崖面 `1185`，两格宽楼梯使用 `1161/1162`；崖顶写入 Layer 0，崖面写入 Layer 1，楼梯覆盖两层坡壁并作为唯一陆行穿越点。",
        "- 楼梯左右至少各保留 1 格同高度悬崖，且楼梯所在的整个连续崖带必须保持同一高度。不同高度的悬崖段不得直接连在该崖带左右；需要错层时，应将其设计为独立地形模块。",
        "- 生成悬崖前必须验证楼梯上下均与两格宽道路连续；悬崖本体不得覆盖路线、溪流或桥梁，路线与崖带的交集只能是楼梯四格。",
        "- 岩石可用于遮蔽远离楼梯的错层接缝，但不得盖住楼梯、路线或溪岸转角。",
        "",
        "### 运行时生成顺序",
        "",
        "- 运行时算法 v3 将区域编号 `1` 映射为“溪桥坡地”。先生成入口、真实分叉点和两个出口的两格宽路网，再枚举一条只在同一桥区与道路相交的竖向溪流；16 次确定性拓扑尝试仍无合法桥位时才判定生成失败。",
        "- 地形按“普通地面 -> 溪流与岸角 -> 悬崖与楼梯 -> 桥梁与水中石块 -> 道路 -> 草丛/鲜花/灌木”合成。道路不得覆盖水面或崖面，路径落入水面时必须位于桥面，路径落入崖带时必须位于楼梯。",
        "- 拓扑、地表和溪流特征使用独立 XorShift32 随机流；C++ 与 Python 必须共享候选枚举顺序、随机数调用顺序、算法版本和 FNV 指纹。同一 `seed + entryEdge + areaIndex` 必须生成完全相同的三层 tile 数据。",
        "",
        "## 应用到 StickMon：灯塔海岸修正",
        "",
        "修正版不再使用池塘边缘。地图左侧按 Map039 的深海、深海边缘、近岸和陆地顺序构造连续海岸，灯塔基座位于近岸水中，道路与高草留在陆地侧。",
        "",
        f"![灯塔海岸修正]({image_paths['lighthouse'].name})",
        "",
        "## 图层证据",
        "",
        f"![Map028 高草图层]({image_paths['map028_layers'].name})",
        "",
        f"![Map039 森林图层]({image_paths['map039_layers'].name})",
        "",
        "## 全部地图图册",
        "",
    ]
    for path in atlas_pages:
        lines.extend([f"![{path.stem}]({path.name})", ""])

    lines.extend([
        "## 逐图索引",
        "",
        "| ID | 名称 | 类别 | 尺寸 | Tileset | 事件 | L1/L2覆盖 | 特征 |",
        "|---:|---|---|---:|---|---:|---:|---|",
    ])
    for record in records:
        layer_text = f"{record['layerCoverage'][1] * 100:.0f}%/{record['layerCoverage'][2] * 100:.0f}%"
        lines.append(
            f"| {record['mapId']:03d} | {record['name']} | {CATEGORY_LABELS[record['category']]} | "
            f"{record['width']}x{record['height']} | {record['tileset']} | {record['eventCount']} | "
            f"{layer_text} | {'、'.join(record['features'])} |"
        )

    report_path = output_dir / "Essentials地图生成规则.md"
    report_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return report_path


def main():
    parser = argparse.ArgumentParser(description="Scan all Pokemon Essentials maps and build rule atlases")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("/tmp/stickmon-essentials-map-analysis"),
    )
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)

    map_files = sorted((ESSENTIALS / "Data").glob("Map[0-9][0-9][0-9].rxdata"))
    records = []
    for index, path in enumerate(map_files, start=1):
        match = re.search(r"(\d{3})", path.name)
        map_id = int(match.group(1))
        data = export_map(map_id)
        image = render_map(data)
        category = classify_map(data)
        tile_counts = Counter(tile_id for layer in data["layers"] for tile_id in layer if tile_id >= 48)
        features, autotiles = detect_features(data, tile_counts, category)
        coverage = layer_coverage(data)
        record = {
            "mapId": map_id,
            "name": data["name"],
            "width": data["width"],
            "height": data["height"],
            "tileset": data["tilesetName"],
            "category": category,
            "eventCount": len(data["events"]),
            "features": features,
            "autotiles": autotiles,
            "layerCoverage": coverage,
            "thumbnail": make_thumbnail(image, data, category),
            "data": data,
        }
        records.append(record)
        print(f"[{index:02d}/{len(map_files)}] Map{map_id:03d} {data['name']}")

    records_by_id = {record["mapId"]: record for record in records}
    atlas_pages = make_all_map_atlases(records, args.output_dir)
    image_paths = {
        "summary": make_summary_chart(records, args.output_dir),
        "routes": make_rule_board(
            records_by_id,
            args.output_dir,
            "rules_outdoor_routes.png",
            "户外路线：边界、主干、分支与草丛",
            (
                (5, "主路连接入口，草丛退到两侧"),
                (21, "纵向长图分段，并保留支路"),
                (28, "环路围绕中心地标"),
                (39, "道路、围栏、森林分层组合"),
                (44, "水岸只在桥与门廊处连通"),
                (47, "山地通路由岩壁控制宽度"),
            ),
        ),
        "settlements": make_rule_board(
            records_by_id,
            args.output_dir,
            "rules_settlements.png",
            "城镇与野外：建筑、广场和景观锚点",
            (
                (2, "住宅门口对齐道路并留前庭"),
                (7, "城市道路连接多个功能建筑"),
                (23, "建筑围绕中心公共空间"),
                (52, "大型设施以广场和轴线组织"),
                (66, "入口建筑形成明确门廊"),
                (68, "野外景观围绕水体和草区"),
            ),
        ),
        "coasts": make_rule_board(
            records_by_id,
            args.output_dir,
            "rules_coasts_caves.png",
            "海岸与洞窟：连续过渡和高度带",
            (
                (34, "冰洞使用连续墙带与台阶"),
                (49, "洞窟通道由岩壁完整包围"),
                (69, "深海、海岸、悬崖逐层过渡"),
                (71, "港口设施必须接触连续水域"),
                (72, "岛屿以海洋完整封边"),
                (73, "入口码头连接岛内中心空间"),
            ),
        ),
        "interiors": make_rule_board(
            records_by_id,
            args.output_dir,
            "rules_interiors.png",
            "室内：墙体封边、中央通道和功能分区",
            (
                (3, "住宅家具沿墙，中央通道留空"),
                (10, "道馆用轴线和障碍组织路线"),
                (13, "娱乐设施划分多个功能区"),
                (14, "商店入口直达柜台与楼梯"),
                (55, "塔类设施保持中央纵向动线"),
                (63, "走廊地图用窄长比例强化方向"),
            ),
        ),
        "map028_layers": make_layer_board(
            records_by_id[28],
            args.output_dir,
            "layers_map028_high_grass.png",
            "Map028 Natural Park：高草为何放在 Layer 1",
            "391 作为前景高草覆盖普通地面，但道路、入口和中心地标保持净空。",
        ),
        "map039_layers": make_layer_board(
            records_by_id[39],
            args.output_dir,
            "layers_map039_forest.png",
            "Map039 Route 4：森林、栅栏与树冠的三层关系",
            "Layer 0 保持森林主体，Layer 1 放栅栏，Layer 2 只放跨越栅栏的树冠和高前景。",
        ),
    }
    lighthouse_clean, _lighthouse_routes = build_map(LANDMARK_MAP_SPECS[0], 20260713)
    lighthouse_path = args.output_dir / "lighthouse_coast_corrected.png"
    lighthouse_clean.save(lighthouse_path, optimize=True)
    image_paths["lighthouse"] = lighthouse_path

    inventory = []
    for record in records:
        inventory.append({key: value for key, value in record.items() if key not in ("thumbnail", "data")})
    (args.output_dir / "map_inventory.json").write_text(
        json.dumps(inventory, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    report = write_report(records, atlas_pages, image_paths, args.output_dir)
    print(f"report={report}")
    print(f"inventory={args.output_dir / 'map_inventory.json'}")


if __name__ == "__main__":
    main()
