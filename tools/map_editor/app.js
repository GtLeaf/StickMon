import {
  copy,
  uid,
  index as cellIndex,
  dimensions,
  prefabFor,
  libraryPrefabs,
  libraryCategories,
  terrainBrush,
  sampleCells,
  flatten,
  bringToFront,
  objectsInRect,
  moveObjects,
  visibleFootprint,
  worldPoints,
  addLine,
  syncJunction,
  flood,
  History,
} from "./model.js";
import { editLake, lakeSeed, retileLake } from "./lake.js";
import { rotationInfo, rotatedPrefab, rotateObject } from "./rotation.js";
import { NpcEditor } from './npc-editor.js';
import {recognizeRoads, currentRoadNetwork, roadRouteCurrent, storeRoadRoute} from './road-network.js';
import {resizeMap} from "./resize.js";
import {stackInfo, stackObject} from './stacking.js';
import {initPanels, openPanel} from './panels.js';
import {deleteLibraryPrefab} from './library.js';
initPanels();
let inspectorSelectionKey = '';
let npcEditor = null;
let savingRoadRoute = false;
const $ = (id) => document.getElementById(id),
  $$ = (selector) => [...document.querySelectorAll(selector)];
let catalog,
  history,
  images = {},
  tool = "select",
  library = "prefabs",
  category = "全部",
  page = 0,
  layer = 0;
let selectedAsset = "outside:385",
  selectedPrefab = "tree",
  selection = [],
  rect = null,
  hover = null,
  drag = null;
let zoom = 1,
  visible = [true, true, true],
  locked = [false, false, false],
  pathIndex = 0,
  report = null,
  highlight = null;
let playing = false,
  revision = 0,
  space = false,
  pan = null,
  pendingPoint = null,
  animation = 0,
  sourceRect = null,
  sourceStart = null;
let transientPrefab = null,
  toastTimer = null,
  autosaveTimer = null;
const canvas = $("map"),
  ctx = canvas.getContext("2d");
const project = () => history.project;
const mapWidth = () => dimensions(project())[0];
const mapHeight = () => dimensions(project())[1];
const index = (x,y) => cellIndex(x,y,mapWidth());
const inside = (x,y) => x>=0&&y>=0&&x<mapWidth()&&y<mapHeight();
const currentPath = () => project().map.paths[pathIndex];
const text = (tag, value, cls) => {
  const el = document.createElement(tag);
  el.textContent = value;
  if (cls) el.className = cls;
  return el;
};
function toast(message) {
  $("toast").textContent = message;
  $("toast").classList.add("visible");
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => $("toast").classList.remove("visible"), 4500);
}
function bind(id, event, fn) {
  $(id).addEventListener(event, async (e) => {
    try {
      await fn(e);
    } catch (error) {
      toast(error.message);
      console.error(error);
    }
  });
}
async function api(path, data) {
  const options =
    data === undefined
      ? {}
      : {
          method: "POST",
          headers: {
            "Content-Type": "application/json",
            "X-Editor-Token": catalog.token,
          },
          body: JSON.stringify(data),
        };
  const response = await fetch(path, options);
  if (!response.ok) {
    const error = await response.json();
    if (error.issues) showReport(error);
    throw Error(error.error || `存在 ${error.errors} 项阻止导出的错误`);
  }
  return response;
}
async function json(path, data) {
  return (await api(path, data)).json();
}
function loadImage(url) {
  return new Promise((resolve, reject) => {
    const im = new Image();
    im.onload = () => resolve(im);
    im.onerror = () => reject(Error("素材图片加载失败：" + url));
    im.src = url;
  });
}
function download(blob, name) {
  const url = URL.createObjectURL(blob),
    a = document.createElement("a");
  a.href = url;
  a.download = name;
  a.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}
function filename(ext) {
  return project().name.replace(/[\\/:*?"<>|]/g, "_") + ext;
}
function setTool(value) {
  tool = value;
  if (value === "lake" || value === "lakeErase") {
    layer = 0;
    selection = [];
    renderLayers();
  }
  pendingPoint = null;
  $$("[data-tool]").forEach((b) =>
    b.classList.toggle("active", b.dataset.tool === value),
  );
  $("hint").textContent = {
    select: "单击选择 · Shift 多选 · 拖动移动 · 空格拖动视图",
    stamp: "单击放置完整物件 · Esc 返回选择",
    brush: "拖动绘制当前图层 · 一次笔触可整体撤销",
    erase: "擦除基础图块；完整物件请选中后删除",
    fill: "填充当前图层的连续区域",
    pick: "单击吸取当前图层素材",
    rect: "框住完整物件后松手，再拖动物件整体移动 · Shift 追加 · 隐藏／锁定物件不选中",
    tileRect: "拖动框选图块，可复制或存为组合",
    route: "单击添加路线点 · 自动补齐水平／垂直连接",
    lake: "单击空地生成 3×3 小湖 · 拖动扩展 · Shift 退水 · 自动连接湖岸",
    lakeErase: "拖动逐格退水 · 恢复原地面并自动修岸 · 一次笔触可撤销",
    zone: "在右侧新增区域 · 拖动矩形或涂抹格子 · 区域独立于地图图层",
    npc: "在右侧新增 NPC · 点击设置出生点 · 拖动调整位置 · 可绘制巡逻点",
  }[tool];
  npcEditor?.toolChanged(tool);
  if (tool === 'route') openPanel('routePanel');
  renderRotation();
  render();
}

function rotationTarget() {
  if (tool === "stamp") return {pref: selectedStamp(), object: null};
  if (tool !== "select" || selection.length !== 1) return null;
  const object = project().map.objects.find((o) => o.id === selection[0]);
  return object ? {pref: prefabFor(project(),object.prefab), object} : null;
}
function renderRotation() {
  if (!history) return;
  const target = rotationTarget(), info = rotationInfo(target?.pref,catalog);
  if (!info) {
    $("rotationLabel").textContent = "选择带「可旋转」标记的集市物件";
    $("rotateLeft").disabled = true;
    $("rotateRight").disabled = true;
    return;
  }
  const name = catalog.prefabs.find((p) => p.id === info.family).name;
  $("rotationLabel").textContent = `${target.object ? "已选" : "待放置"} · ${name} · ${info.angle}° · ${target.pref.width}×${target.pref.height}`;
  const disabled = !!drag || target.pref.cells.some((c) => locked[c.layer]);
  $("rotateLeft").disabled = disabled;
  $("rotateRight").disabled = disabled;
}
function rotateCurrent(direction) {
  if (drag) throw Error("请先结束当前拖动再旋转");
  const target = rotationTarget();
  if (!target) throw Error("请选择一个集市物件，或先从素材库选择待放置物件");
  if (target.object) {
    mutate((p) => rotateObject(p,target.object.id,catalog,direction,locked));
  } else {
    if (target.pref.cells.some((c) => locked[c.layer])) throw Error("物件涉及的图层已锁定");
    const next = rotatedPrefab(project(),target.pref,catalog,direction);
    selectedPrefab = next.id;
    transientPrefab = null;
    renderLibrary();
    renderRotation();
    render();
  }
}
for (const [id,direction] of [["rotateLeft",-1],["rotateRight",1]]) {
  $(id).onclick = () => {
    try { rotateCurrent(direction); } catch (error) { toast(error.message); }
  };
}
function changed() {
  npcEditor?.invalidate();
  revision++;
  report = null;
  highlight = null;
  playing = false;
  $("play").textContent = "播放";
  $("validationSummary").textContent = "地图已修改，待校验";
  $("issues").replaceChildren(
    text("p", "修改已保留；重新校验以更新问题。", "muted"),
  );
  update();
  clearTimeout(autosaveTimer);
  autosaveTimer = setTimeout(() => {
    try {
      localStorage.setItem(
        "stickmon-map-editor-draft",
        JSON.stringify(project()),
      );
    } catch (error) {
      toast("浏览器草稿备份失败，请保存工程文件");
    }
  }, 400);
}
function mutate(fn) {
  if (history.change((p) => { fn(p); retileLake(p,catalog.lake); })) changed();
}
function replaceProject(value, saved = false) {
  history.change((p) => {
    Object.keys(p).forEach((k) => delete p[k]);
    Object.assign(p, copy(value));
  });
  selection = [];
  rect = null;
  pathIndex = 0;
  transientPrefab = null;
  changed();
  if (saved) history.markSaved();
  renderLibrary();
  update();
}
function guardReplace() {
  return (
    !history.dirty ||
    confirm("当前工程有未保存的修改。继续载入？可通过撤销恢复。")
  );
}
function checkRevision(before) {
  if (before !== revision)
    throw Error("等待期间地图已修改，操作已取消，请重试");
}
function update() {
  if (!history) return;
  $("mapDimensions").textContent = `${mapWidth()} × ${mapHeight()}`;
  $("mapWidth").value = mapWidth();
  $("mapHeight").value = mapHeight();
  if (document.activeElement !== $("projectName"))
    $("projectName").value = project().name;
  $("saveState").textContent = history.dirty
    ? "有未保存修改 · 浏览器自动备份"
    : "工程已保存";
  $("undo").disabled = !history.undoStack.length;
  $("redo").disabled = !history.redoStack.length;
  $("originLabel").textContent =
    project().origin?.type === "snapshot"
      ? `生成快照 · seed ${project().map.seed}`
      : "手工地图";
  $("resourceInfo").textContent =
    `已用地图包 ${catalog.pack.frames} / ${catalog.pack.limits.frames} 帧 · 调色板 ${catalog.pack.paletteWords} / ${catalog.pack.limits.paletteWords} words。素材库 ${Object.keys(catalog.assets).length} 块。`;
  renderInspector();
  renderPaths();
  npcEditor?.refresh();
  render();
}
function assetPending(key) {
  return catalog.assets[key]?.runtimeId == null;
}
function drawTile(context, key, x, y, size = 32, frame = 0) {
  if (!key) return;
  const a = catalog.assets[key];
  if (!a) {
    context.fillStyle = "#b04d48";
    context.fillRect(x, y, size, size);
    return;
  }
  context.imageSmoothingEnabled = false;
  if (a.atlas) {
    const [sx, sy] = a.atlas;
    context.drawImage(
      images.runtime,
      sx,
      sy + (frame % 8) * 32,
      32,
      32,
      x,
      y,
      size,
      size,
    );
  } else {
    const [sx, sy] = a.rect;
    context.drawImage(images[a.source], sx, sy, 32, 32, x, y, size, size);
  }
}
function drawPrefab(context, pref, x, y, size = 32, alpha = 1) {
  context.save();
  context.globalAlpha = alpha;
  for (let z = 0; z < 3; z++)
    for (const c of pref.cells)
      if (c.layer === z)
        drawTile(
          context,
          c.asset,
          x + c.x * size,
          y + c.y * size,
          size,
          animation,
        );
  context.restore();
}
function availablePrefabs() {
  return libraryPrefabs(project(), catalog.prefabs);
}
function selectedStamp() {
  return transientPrefab || prefabFor(project(), selectedPrefab) || catalog.prefabs.find((p) => p.id === selectedPrefab);
}
function animateLibrary() {
  for (const canvas of $$("#assetList canvas[data-animated-prefab]")) {
    const pref = availablePrefabs().find((p) => p.id === canvas.dataset.animatedPrefab);
    const context = canvas.getContext("2d");
    context.clearRect(0, 0, canvas.width, canvas.height);
    drawPrefab(context, pref, 0, 0);
  }
}
function renderLibrary() {
  if (!history) return;
  const search = $("search").value.trim().toLowerCase();
  $("tileOptions").hidden = library !== "tiles";
  $("assetList").classList.toggle("tiles", library === "tiles");
  $("categories").replaceChildren();
  if (library === "prefabs") {
    const present = new Set(availablePrefabs().map((p) => p.category));
    if (category !== '全部' && !present.has(category)) category = '全部';
    const names = ['全部', ...libraryCategories.filter((name) => present.has(name)),
      ...[...present].filter((name) => !libraryCategories.includes(name))];
    for (const name of names) {
      const b = text("button", name);
      b.classList.toggle("active", name === category);
      b.onclick = () => {
        category = name;
        page = 0;
        if (name === "湖泊") setTool("lake");
        else if (tool === "lake" || tool === "lakeErase") setTool("select");
        renderLibrary();
      };
      $("categories").append(b);
    }
  }
  $("lakeTools").hidden = library !== "prefabs" || category !== "湖泊";
  let items =
    library === "prefabs"
      ? availablePrefabs().filter(
          (p) =>
            (category === "全部" || (p.category || "自定义") === category) &&
            `${p.name} ${p.id} ${p.category} ${p.brushAsset || ''}`.toLowerCase().includes(search),
        )
      : Object.values(catalog.assets).filter(
          (a) =>
            a.source === $("sheet").value &&
            (!$("hideEmpty").checked || !a.empty) &&
            `${a.label} ${a.key}`.toLowerCase().includes(search),
        );
  const size = library === "prefabs" ? 40 : 96,
    pages = Math.max(1, Math.ceil(items.length / size));
  page = Math.min(page, pages - 1);
  $("assetCount").textContent = `${items.length} ITEMS`;
  $("pageLabel").textContent = `${page + 1} / ${pages}`;
  $("prevPage").disabled = page === 0;
  $("nextPage").disabled = page === pages - 1;
  $("assetList").replaceChildren();
  const stampRotation = rotationInfo(selectedStamp(),catalog);
  for (const item of items.slice(page * size, (page + 1) * size)) {
    const b = document.createElement("button");
    b.className = "asset-card";
    const c = document.createElement("canvas"),
      cx = c.getContext("2d");
    const display = library === "prefabs" && stampRotation?.family === item.id ? selectedStamp() : item;
    c.width = library === "prefabs" ? display.width * 32 : 32;
    c.height = library === "prefabs" ? display.height * 32 : 32;
    let pending;
    if (library === "prefabs") {
      drawPrefab(cx, display, 0, 0);
      pending = item.cells.some((v) => assetPending(v.asset));
      const frames = Math.max(...item.cells.map((v) => catalog.assets[v.asset]?.frames || 1));
      if (frames > 1) c.dataset.animatedPrefab = item.id;
      b.append(
        c,
        text("span", item.name, "name"),
        text(
          "small",
          `${display.width} × ${display.height} · ${terrainBrush(item) ? '地面笔刷' : rotationInfo(display,catalog) ? `可旋转 ${rotationInfo(display,catalog).angle}°` : pending ? "待适配" : frames > 1 ? `${frames} 帧动画` : "已接入"}`,
        ),
      );
      b.classList.toggle("selected", terrainBrush(item) ? tool === 'brush' && selectedAsset === terrainBrush(item) : tool === 'stamp' && selectedPrefab === item.id);
      if (stampRotation?.family === item.id) b.classList.add("selected");
      b.onclick = () => {
        const asset = terrainBrush(item);
        if (asset) {
          selectedAsset = asset;
          layer = 0;
          transientPrefab = null;
          selection = [];
          renderLayers();
          setTool('brush');
          renderLibrary();
          return;
        }
        if (rotationInfo(selectedStamp(),catalog)?.family !== item.id) selectedPrefab = item.id;
        transientPrefab = null;
        setTool("stamp");
        renderLibrary();
      };
    } else {
      drawTile(cx, item.key, 0, 0);
      pending = assetPending(item.key);
      b.append(c, text("span", String(item.tile), "name"));
      b.classList.toggle("selected", selectedAsset === item.key);
      b.onclick = () => {
        selectedAsset = item.key;
        setTool("brush");
        renderLibrary();
      };
    }
    b.title =
      library === "prefabs"
        ? item.name + (pending ? " · 新素材尚需运行时适配" : "")
        : item.label + (pending ? " · 尚未接入游戏" : " · 可导出");
    if (pending) b.append(text("span", "", "pending"));
    if (library === "prefabs") {
      const entry = text('div','', 'asset-entry');
      const remove = text('button','×','asset-delete');
      remove.type = 'button';
      remove.setAttribute('aria-label',`删除素材：${item.name}`);
      remove.title = '从当前工程素材库删除，保留已放置物品；可撤销';
      remove.onclick = () => {
        try {
          const active = selectedPrefab === item.id || stampRotation?.family === item.id ||
            (terrainBrush(item) && selectedAsset === terrainBrush(item));
          mutate(p => deleteLibraryPrefab(p,catalog.prefabs,item.id));
          if (active) {
            selectedPrefab = '';
            transientPrefab = null;
            setTool('select');
          }
          renderLibrary();
          toast(`已从素材库删除「${item.name}」，已放置物品保留；可撤销`);
        } catch (error) { toast(error.message); }
      };
      entry.append(b,remove);
      $("assetList").append(entry);
    } else $("assetList").append(b);
  }
}
function renderLayers() {
  $("layers").replaceChildren();
  for (let z = 0; z < 3; z++) {
    const row = document.createElement("div");
    row.className = "layer";
    const button = text("button", `L${z} · ${["地面", "物体", "前景"][z]}`);
    button.classList.toggle("active", z === layer);
    button.onclick = () => {
      layer = z;
      renderLayers();
    };
    row.append(button);
    for (const [name, array] of [
      ["显示", visible],
      ["锁定", locked],
    ]) {
      const label = text("label", name),
        input = document.createElement("input");
      input.type = "checkbox";
      input.checked = array[z];
      input.setAttribute("aria-label", `L${z}${name}`);
      input.onchange = () => {
        array[z] = input.checked;
        renderInspector();
        render();
      };
      label.prepend(input);
      row.append(label);
    }
    $("layers").append(row);
  }
}
function renderInspector() {
  selection = selection.filter((id) =>
    project().map.objects.some((o) => o.id === id),
  );
  const box = $("selection");
  const selectionKey = selection.join(',');
  if (selectionKey && selectionKey !== inspectorSelectionKey) openPanel('selectionPanel');
  inspectorSelectionKey = selectionKey;
  box.replaceChildren();
  if (selection.length === 1) {
    const o = project().map.objects.find((o) => o.id === selection[0]),
      pref = prefabFor(project(), o.prefab);
    box.append(
      text("strong", pref.name),
      text(
        "p",
        `${pref.width} × ${pref.height} 格 · 锚点 ${pref.anchor.join(", ")} · ${pref.cells.length} 图块`,
        "muted",
      ),
    );
    const row = document.createElement("div");
    row.className = "inline";
    for (const key of ["x", "y"]) {
      const label = text("label", key.toUpperCase()),
        input = document.createElement("input");
      input.type = "number";
      input.value = o[key];
      input.min = 0;
      input.max = key === "x" ? mapWidth() - pref.width : mapHeight() - pref.height;
      input.setAttribute("aria-label", `物件 ${key.toUpperCase()}`);
      input.onchange = () => {
        const n = Number(input.value);
        if (!Number.isInteger(n) || n < 0 || n > Number(input.max)) {
          toast("完整物件不能超出地图");
          renderInspector();
          return;
        }
        mutate((p) => {
          p.map.objects.find((v) => v.id === o.id)[key] = n;
          bringToFront(p, [o.id]);
        });
      };
      label.append(input);
      row.append(label);
    }
    box.append(row);
    const stack = stackInfo(pref, catalog);
    if (stack) {
      const field = text('div', '', 'stack-field');
      field.append(text('span', '堆叠层数'));
      const buttons = text('div', '', 'segmented');
      buttons.setAttribute('role', 'group');
      buttons.setAttribute('aria-label', '堆叠层数');
      for (const count of [1,2,3]) {
        const button = text('button', String(count));
        button.type = 'button';
        button.setAttribute('aria-label', `堆叠 ${count} 层`);
        button.setAttribute('aria-pressed', String(stack.count === count));
        button.classList.toggle('active', stack.count === count);
        button.disabled = pref.cells.some(c => locked[c.layer]);
        button.onclick = () => {
          try {
            if (drag) throw Error('请先结束当前拖动再调整堆叠');
            mutate(p => stackObject(p, o.id, catalog, count, locked));
          } catch (error) { toast(error.message); }
        };
        buttons.append(button);
      }
      field.append(buttons);
      box.append(field);
      if (stack.rotated) box.append(text('p', '增加堆叠前，请先旋转恢复到 0°。', 'muted'));
    }
  } else if (selection.length) {
    box.append(text("strong", `已选中 ${selection.length} 个物件`));
  } else if (rect) {
    box.append(
      text("strong", `框选 ${rect.w} × ${rect.h}`),
      text("p", `(${rect.x}, ${rect.y}) · 当前显示图层`, "muted"),
    );
  } else box.append(text("p", "选择完整物件，或框选地图中的图块。", "muted"));
  $("duplicate").disabled = !selection.length;
  $("delete").disabled = !selection.length && !rect;
  $("capture").disabled = !selection.length && !rect;
  $("explode").disabled = !selection.length;
  $("editObject").disabled = selection.length !== 1;
  renderRotation();
}
function renderPaths() {
  const network = currentRoadNetwork(project());
  $("saveRoadRoute").disabled = !network || savingRoadRoute;
  $("roadInfo").textContent = network
    ? `已识别 ${network.edges.length} 段 · ${network.junctions} 个路口 · ${network.components} 片连通道路${network.uncovered.length ? ` · ${network.uncovered.length} 格未接入（窄路或遮挡）` : ''}`
    : project().map.roadNetwork ? '道路或物件已变化，请重新识别。' : '识别已铺设的双格道路，显示路网。';
  const select = $("pathSelect"),
    paths = project().map.paths;
  pathIndex = Math.max(0, Math.min(pathIndex, paths.length - 1));
  select.replaceChildren();
  paths.forEach((p, i) => {
    const o = text("option", `路线 ${i + 1}${p.kind === 'roadNetwork' ? ' · 道路往返' : ''} · ${p.points.length} 点`);
    o.value = i;
    select.append(o);
  });
  if (!paths.length) select.append(text("option", "尚无路线"));
  select.value = String(pathIndex);
  const path = currentPath();
  $("cellCentered").checked = path?.cellCentered || false;
  const roadRoute = path?.kind === 'roadNetwork';
  $("cellCentered").disabled = !path || roadRoute;
  $("popPoint").disabled = !path || roadRoute;
  $("setExit").disabled = !path || roadRoute;
  $("exitEdge").disabled = !path || roadRoute;
  $("entryEdge").value = project().map.entry.edge;
  $("exitEdge").value = path?.exit.edge || "top";
  $("addPath").disabled = paths.length >= 3;
  $("removePath").disabled = !paths.length;
  $("routeInfo").textContent = roadRoute
    ? roadRouteCurrent(project(),path) ? '沿道路走遍各支路并返回起点 · 可播放、保存工程；游戏数据包暂不支持此类路线。' : '道路已变化，请重新识别并存为路线。'
    : path
    ? `入口 (${project().map.entry.point}) → 出口 (${path.exit.point})${path.interiorExit ? " · 原地转场" : ""}${path.nextValleyNode !== 255 ? " · 目标节点 " + path.nextValleyNode : ""}`
    : "添加路线后，切换路线工具，逐段单击地图。";
}
function tileSize() {
  return Number($("scaleMode").value);
}
function render() {
  if (!history || !images.runtime) return;
  const size = tileSize(),
    w = mapWidth() * size,
    h = mapHeight() * size;
  if (canvas.width !== w || canvas.height !== h) {
    canvas.width = w;
    canvas.height = h;
  }
  ctx.clearRect(0, 0, w, h);
  ctx.fillStyle = "#101713";
  ctx.fillRect(0, 0, w, h);
  const { layers, owners } = flatten(project());
  for (let z = 0; z < 3; z++)
    if (visible[z])
      layers[z].forEach((key, i) =>
        drawTile(
          ctx,
          key,
          (i % mapWidth()) * size,
          Math.floor(i / mapWidth()) * size,
          size,
          animation,
        ),
      );
  if ($("actorVisible").checked && currentPath()?.points.length && roadRouteCurrent(project(),currentPath())) {
    const points = worldPoints(currentPath()),
      t = (Number($("progress").value) / 1000) * (points.length - 1),
      i = Math.floor(t),
      a = points[i],
      b = points[Math.min(i + 1, points.length - 1)],
      f = t - i,
      foot = [
        ((a[0] + (b[0] - a[0]) * f) / 26) * size,
        ((a[1] + (b[1] - a[1]) * f) / 26) * size,
      ],
      ratio = size / 26,
      meta = catalog.sprite,
      sw = meta.width * ratio,
      sh = meta.height * ratio;
    const left = foot[0] - sw / 2,
      top = foot[1] - (meta.height / 2 + meta.groundOffsetY) * ratio;
    ctx.fillStyle = "#0005";
    ctx.beginPath();
    ctx.ellipse(foot[0], foot[1], 9 * ratio, 3 * ratio, 0, 0, 7);
    ctx.fill();
    ctx.drawImage(images.sprite, left, top, sw, sh);
    ctx.save();
    ctx.beginPath();
    ctx.rect(left, top, sw, sh);
    ctx.clip();
    for (let z = 1; z < 3; z++)
      if (visible[z])
        layers[z].forEach((key, n) => {
          if (!key) return;
          const asset = catalog.assets[key];
          const object =
            owners[z][n] &&
            project().map.objects.find((o) => o.id === owners[z][n]);
          const pref = object && prefabFor(project(), object.prefab);
          let depth = asset?.depth?.[z] || 0;
          let ground =
            (Math.floor(n / mapWidth()) + depth - (asset?.ladder?.[z] ? 0.5 : 0)) *
            size;
          if (object && pref.anchorMode !== "runtime") {
            ground = (object.y + pref.anchor[1]) * size;
            depth = pref.anchor[1];
          }
          if (depth && foot[1] < ground)
            drawTile(
              ctx,
              key,
              (n % mapWidth()) * size,
              Math.floor(n / mapWidth()) * size,
              size,
              animation,
            );
        });
    ctx.restore();
  }
  npcEditor?.draw(ctx,size,layers,owners,visible,drawTile);
  if ($("grid").checked) {
    ctx.strokeStyle = "#e5efd622";
    ctx.lineWidth = 1;
    ctx.beginPath();
    for (let x = 0; x <= mapWidth(); x++) {
      ctx.moveTo(x * size + 0.5, 0);
      ctx.lineTo(x * size + 0.5, h);
    }
    for (let y = 0; y <= mapHeight(); y++) {
      ctx.moveTo(0, y * size + 0.5);
      ctx.lineTo(w, y * size + 0.5);
    }
    ctx.stroke();
  }
  if ($("routesVisible").checked) {
    const network = currentRoadNetwork(project());
    if (network) {
      ctx.strokeStyle = "#86c9c1"; ctx.lineWidth = 2.5; ctx.beginPath();
      for (const [a,b] of network.edges) {
        ctx.moveTo(network.nodes[a][0]*size, network.nodes[a][1]*size);
        ctx.lineTo(network.nodes[b][0]*size, network.nodes[b][1]*size);
      }
      ctx.stroke(); ctx.fillStyle = "#86c9c1";
      for (const [x,y] of network.nodes) { ctx.beginPath(); ctx.arc(x*size,y*size,2.8,0,7); ctx.fill(); }
      ctx.strokeStyle = "#e7b66a"; ctx.lineWidth = 1;
      for (const [x,y] of network.uncovered) ctx.strokeRect(x*size+2,y*size+2,size-4,size-4);
    }
    const colors = ["#f4d989", "#a6dcce", "#f0b4a5"];
    project().map.paths.forEach((path, pi) => {
      if (!roadRouteCurrent(project(),path)) return;
      const points = worldPoints(path);
      ctx.strokeStyle = colors[pi];
      ctx.lineWidth = pi === pathIndex ? 2.5 : 1.5;
      ctx.beginPath();
      points.forEach(([x, y], i) => {
        if (!i) ctx.moveTo((x / 26) * size, (y / 26) * size);
        else ctx.lineTo((x / 26) * size, (y / 26) * size);
      });
      ctx.stroke();
      points.forEach(([x, y]) => {
        ctx.fillStyle = colors[pi];
        ctx.beginPath();
        ctx.arc((x / 26) * size, (y / 26) * size, 2.4, 0, 7);
        ctx.fill();
      });
    });
    const drawEndpoint = (e, label, color) => {
      const [x, y] = e.point;
      ctx.fillStyle = color;
      ctx.fillRect(x * size + 3, y * size + 3, 16, 15);
      ctx.font = "10px Menlo,monospace";
      ctx.fillStyle = "#142018";
      ctx.fillText(label, x * size + 5, y * size + 14);
    };
    drawEndpoint(project().map.entry, "入", "#a9ce90");
    project().map.paths.forEach((p, i) => {
      if(p.kind !== 'roadNetwork') drawEndpoint(p.exit, String(i + 1), colors[i] || colors[0]);
      else if(roadRouteCurrent(project(),p)) {
        const [x,y]=p.points[0];ctx.fillStyle=colors[i]||colors[0];ctx.font='10px Menlo,monospace';
        ctx.fillText('起/终',Math.min(w-30,x*size+4),Math.max(12,Math.min(h-4,y*size-4)));
      }
    });
  }
  for (const o of project().map.objects) {
    const pref = prefabFor(project(), o.prefab);
    if ($("anchors").checked) {
      ctx.fillStyle = "#d7865c60";
      for (const [dx, dy] of visibleFootprint(o,pref,owners,project().map.size))
        ctx.fillRect((o.x + dx) * size, (o.y + dy) * size, size, size);
      ctx.strokeStyle = "#ffd59b";
      ctx.lineWidth = 1;
      const x = (o.x + pref.anchor[0]) * size,
        y = (o.y + pref.anchor[1]) * size;
      ctx.beginPath();
      ctx.moveTo(x - 5, y);
      ctx.lineTo(x + 5, y);
      ctx.moveTo(x, y - 5);
      ctx.lineTo(x, y + 5);
      ctx.stroke();
    }
    if (selection.includes(o.id)) {
      ctx.strokeStyle = "#d9f3b6";
      ctx.lineWidth = 2;
      ctx.strokeRect(
        o.x * size + 1,
        o.y * size + 1,
        pref.width * size - 2,
        pref.height * size - 2,
      );
    }
  }
  if (rect) {
    ctx.fillStyle = "#a9ce9025";
    ctx.fillRect(rect.x * size, rect.y * size, rect.w * size, rect.h * size);
    ctx.strokeStyle = "#dbf7bb";
    ctx.setLineDash([4, 3]);
    ctx.strokeRect(
      rect.x * size + 0.5,
      rect.y * size + 0.5,
      rect.w * size - 1,
      rect.h * size - 1,
    );
    ctx.setLineDash([]);
  }
  if (highlight && highlight.x != null) {
    ctx.fillStyle = "#ff9b5340";
    ctx.fillRect(highlight.x * size, highlight.y * size, size, size);
    ctx.strokeStyle = "#ffbc73";
    ctx.lineWidth = 2;
    ctx.strokeRect(
      highlight.x * size + 1,
      highlight.y * size + 1,
      size - 2,
      size - 2,
    );
  }
  if (hover && tool === "stamp") {
    const pref = selectedStamp();
    if (pref) {
      drawPrefab(ctx, pref, hover.x * size, hover.y * size, size, 0.5);
      ctx.strokeStyle =
        hover.x + pref.width <= mapWidth() && hover.y + pref.height <= mapHeight()
          ? "#d9f3b6"
          : "#ff8f77";
      ctx.strokeRect(
        hover.x * size,
        hover.y * size,
        pref.width * size,
        pref.height * size,
      );
    }
  }
  canvas.style.width = w * zoom + "px";
  canvas.style.height = h * zoom + "px";
  $("zoomLabel").textContent = Math.round(zoom * 100) + "%";
}
function cellAt(e) {
  const bounds = canvas.getBoundingClientRect();
  return {
    x: Math.max(
      0,
      Math.min(mapWidth()-1, Math.floor(((e.clientX - bounds.left) / bounds.width) * mapWidth())),
    ),
    y: Math.max(
      0,
      Math.min(mapHeight()-1, Math.floor(((e.clientY - bounds.top) / bounds.height) * mapHeight())),
    ),
  };
}
function objectAt({ x, y }) {
  return [...project().map.objects].reverse().find((o) => {
    const p = prefabFor(project(), o.prefab);
    return p.cells.some(
      (c) => o.x + c.x === x && o.y + c.y === y && visible[c.layer],
    );
  });
}
function selectionLocked() {
  return selection.some((id) => {
    const o = project().map.objects.find((v) => v.id === id);
    return (
      o && prefabFor(project(), o.prefab).cells.some((c) => locked[c.layer])
    );
  });
}
function paint(cell) {
  if (tool === "lake" || tool === "lakeErase") {
    if (locked[0]) throw Error("地面图层已锁定");
    editLake(project(), [[cell.x,cell.y]], tool === "lakeErase" || drag?.eraseLake, catalog.lake);
    return;
  }
  if (locked[layer]) throw Error("当前图层已锁定");
  if (tool === "erase" && layer === 0) {
    const key = flatten(project()).layers[0][index(cell.x,cell.y)];
    if (Object.values(catalog.lake.byMask).includes(key) || catalog.lake.compatibleKeys.includes(key)) {
      editLake(project(), [[cell.x,cell.y]], true, catalog.lake);
      return;
    }
  }
  const { owners } = flatten(project());
  const i = index(cell.x, cell.y);
  if (owners[layer][i]) throw Error("此图块属于完整物件，请先拆分或移动物件");
  const value = tool === "erase" ? null : selectedAsset;
  if (tool === "fill")
    flood(
      project().map.layers[layer],
      i,
      value,
      new Set(owners[layer].flatMap((v, i) => (v ? [i] : []))),
      project().map.size,
    );
  else project().map.layers[layer][i] = value;
  if (layer === 0) retileLake(project(), catalog.lake);
}
function stamp(cell) {
  let pref = selectedStamp();
  if (!pref) return;
  if (cell.x + pref.width > mapWidth() || cell.y + pref.height > mapHeight())
    throw Error("完整物件超出地图，请换个位置");
  if (pref.cells.some((c) => locked[c.layer]))
    throw Error("物件涉及的图层已锁定");
  const id = uid("obj");
  mutate((p) => {
    if (!prefabFor(p, pref.id)) p.prefabs.push(copy(pref));
    for (const c of pref.cells) {
      const source = catalog.assets[c.asset]?.source;
      if (source && !p.sources[source] && catalog.sources[source]) p.sources[source] = copy(catalog.sources[source]);
    }
    p.map.objects.push({ id, prefab: pref.id, x: cell.x, y: cell.y });
  });
  selection = [id];
  rect = null;
  update();
}
function routeClick(cell) {
  const path = currentPath();
  if (!path) throw Error("请先添加一条路线");
  if (path.kind === 'roadNetwork') throw Error('道路路线请修改道路后重新识别并存为路线');
  mutate((p) => {
    const route = p.map.paths[pathIndex];
    route.points = addLine(route.points, [cell.x, cell.y]);
    route.exit.point = [cell.x, cell.y];
    syncJunction(p);
  });
}
canvas.addEventListener("pointerdown", (e) => {
  if (!history || e.button > 1) return;
  try {
    const cell = cellAt(e);
    canvas.setPointerCapture(e.pointerId);
    if (space || e.button === 1) {
      pan = {
        x: e.clientX,
        y: e.clientY,
        left: $("viewport").scrollLeft,
        top: $("viewport").scrollTop,
      };
      return;
    }
    if (pendingPoint) {
      const kind = pendingPoint;
      if (kind === "exit" && !currentPath()) throw Error("请先添加路线");
      mutate((p) => {
        const end =
          kind === "entry" ? p.map.entry : p.map.paths[pathIndex].exit;
        end.point = [cell.x, cell.y];
      });
      pendingPoint = null;
      return;
    }
    if (npcEditor?.pointerDown(cell)) return;
    if (tool === "stamp") {
      stamp(cell);
      return;
    }
    if (tool === "route") {
      routeClick(cell);
      return;
    }
    if (tool === "pick") {
      const key = flatten(project()).layers[layer][index(cell.x, cell.y)];
      if (key) {
        selectedAsset = key;
        setTool("brush");
        toast("已取样 " + key);
      }
      return;
    }
    if (tool === "rect" || tool === "tileRect") {
      drag = { type: "rect", start: cell, objects: tool === "rect", add: e.shiftKey,
        previousSelection: [...selection], previousRect: rect };
      if (!e.shiftKey || tool === "tileRect") selection = [];
      rect = { ...cell, w: 1, h: 1 };
      renderInspector();
      render();
      return;
    }
    if (tool === "select") {
      const object = objectAt(cell);
      rect = null;
      if (!object) {
        selection = [];
        update();
        return;
      }
      if (e.shiftKey) {
        selection = selection.includes(object.id)
          ? selection.filter((id) => id !== object.id)
          : [...selection, object.id];
      } else if (!selection.includes(object.id)) selection = [object.id];
      if (!selection.includes(object.id)) { update(); return; }
      if (selectionLocked()) throw Error("选中物件所在图层已锁定");
      history.begin();
      drag = {
        type: "object",
        start: cell,
        positions: project()
          .map.objects.filter((o) => selection.includes(o.id))
          .map((o) => ({ id: o.id, x: o.x, y: o.y })),
      };
      update();
      return;
    }
    history.begin();
    drag = { type: "paint", last: cell, eraseLake: e.shiftKey };
    if (tool === "lake" && !e.shiftKey) {
      if (locked[0]) throw Error("地面图层已锁定");
      editLake(project(), lakeSeed(project(),cell.x,cell.y,catalog.lake), false, catalog.lake);
    } else paint(cell);
    render();
  } catch (error) {
    history.cancel();
    drag = null;
    toast(error.message);
    render();
  }
});
canvas.addEventListener("pointermove", (e) => {
  if (!history) return;
  const cell = cellAt(e);
  hover = cell;
  $("coordinates").textContent =
    `格 ${cell.x},${cell.y} · 原 ${cell.x * 32},${cell.y * 32}px · 逻辑 ${cell.x * 26},${cell.y * 26}px`;
  if (pan) {
    $("viewport").scrollLeft = pan.left - (e.clientX - pan.x);
    $("viewport").scrollTop = pan.top - (e.clientY - pan.y);
    return;
  }
  if (npcEditor?.pointerMove(cell)) return;
  if (drag?.type === "object") {
    moveObjects(project(),drag.positions,cell.x-drag.start.x,cell.y-drag.start.y);
  } else if (drag?.type === "rect") {
    const a = drag.start;
    rect = {
      x: Math.min(a.x, cell.x),
      y: Math.min(a.y, cell.y),
      w: Math.abs(a.x - cell.x) + 1,
      h: Math.abs(a.y - cell.y) + 1,
    };
  } else if (drag?.type === "paint") {
    try {
      const points = addLine([[drag.last.x, drag.last.y]], [cell.x, cell.y],mapWidth()+mapHeight());
      for (const [x, y] of points) paint({ x, y });
      drag.last = cell;
    } catch (error) {
      toast(error.message);
    }
  }
  render();
});
function finishPointer(cancel = false) {
  pan = null;
  if (npcEditor?.finish(cancel)) return;
  if (!drag) return;
  const finished = drag;
  drag = null;
  if (finished.type === "rect") {
    if (cancel) {
      selection = finished.previousSelection; rect = finished.previousRect;
    } else if (finished.objects) {
      const ids = objectsInRect(project(),rect,visible,locked);
      selection = [...new Set([...(finished.add ? finished.previousSelection : []), ...ids])];
      rect = null;
      setTool("select");
      toast(selection.length ? `已框选 ${selection.length} 个完整物件，拖动其中任意物件即可整体移动` :
        "未选中物件：请框住完整物件，并检查图层是否隐藏或锁定");
    }
  } else if (cancel) history.cancel();
  else {
    if (finished.type === "object" && finished.positions.some(pos => {
      const object = project().map.objects.find(o => o.id === pos.id);
      return object.x !== pos.x || object.y !== pos.y;
    })) bringToFront(project(),selection);
    if (history.finish()) changed();
  }
  renderInspector();
  render();
}
canvas.addEventListener("pointerup", () => finishPointer());
canvas.addEventListener("pointercancel", () => finishPointer(true));
canvas.addEventListener("lostpointercapture", () => finishPointer());
canvas.addEventListener("pointerleave", () => {
  hover = null;
  if (!drag) render();
});
function fit() {
  const view = $("viewport"),
    size = tileSize();
  zoom = Math.min(
    3,
    Math.max(
      0.25,
      Math.min(
        (view.clientWidth - 60) / (mapWidth() * size),
        (view.clientHeight - 60) / (mapHeight() * size),
      ),
    ),
  );
  render();
}
function removeSelection() {
  if (selectionLocked()) throw Error("选中物件所在图层已锁定");
  mutate((p) => {
    if (selection.length)
      p.map.objects = p.map.objects.filter((o) => !selection.includes(o.id));
    else if (rect) {
      if (locked[layer]) throw Error("当前图层已锁定");
      const { owners } = flatten(p);
      for (let y = rect.y; y < rect.y + rect.h; y++)
        for (let x = rect.x; x < rect.x + rect.w; x++)
          if (!owners[layer][index(x, y)]) {
            const i = index(x,y);
            p.map.layers[layer][i] = layer === 0 && p.map.lakeGround?.[i]
              ? p.map.lakeGround[i] : null;
          }
    }
  });
  selection = [];
  update();
}
function duplicate() {
  if (selectionLocked()) throw Error("选中物件所在图层已锁定");
  const originals = project().map.objects.filter((o) =>
    selection.includes(o.id),
  );
  if (!originals.length) return;
  const ids = [];
  mutate((p) => {
    const dx = originals.every((o) => o.x + prefabFor(p, o.prefab).width < mapWidth())
      ? 1
      : -1;
    if (originals.some((o) => o.x + dx < 0))
      throw Error("旁边没有空间，请先移动物件");
    for (const o of originals) {
      const n = { ...copy(o), id: uid("obj"), x: o.x + dx };
      p.map.objects.push(n);
      ids.push(n.id);
    }
  });
  selection = ids;
  update();
}
function capturedPrefab() {
  const layers = flatten(project()).layers;
  let box = rect,
    cells = [];
  if (selection.length) {
    const objects = project().map.objects.filter((o) =>
      selection.includes(o.id),
    );
    const x = Math.min(...objects.map((o) => o.x)),
      y = Math.min(...objects.map((o) => o.y));
    box = {
      x,
      y,
      w:
        Math.max(
          ...objects.map((o) => o.x + prefabFor(project(), o.prefab).width),
        ) - x,
      h:
        Math.max(
          ...objects.map((o) => o.y + prefabFor(project(), o.prefab).height),
        ) - y,
    };
    for (const o of objects)
      for (const c of prefabFor(project(), o.prefab).cells)
        cells.push({ ...copy(c), x: o.x + c.x - x, y: o.y + c.y - y });
  } else if (box) {
    for (let z = 0; z < 3; z++)
      if (visible[z])
        for (let dy = 0; dy < box.h; dy++)
          for (let dx = 0; dx < box.w; dx++) {
            const asset = layers[z][index(box.x + dx, box.y + dy)];
            if (asset) cells.push({ x: dx, y: dy, layer: z, asset });
          }
  }
  if (!box || !cells.length) throw Error("请先选择物件或框选有内容的区域");
  if (box.w>16 || box.h>12) throw Error("单个组合最多 16×12 格，请分块保存组合");
  const unique = new Map(cells.map((c) => [`${c.layer}:${c.x}:${c.y}`, c]));
  return {
    id: uid("prefab"),
    name: "自定义组合",
    category: "自定义",
    width: box.w,
    height: box.h,
    cells: [...unique.values()],
    anchor: [box.w / 2, box.h],
    anchorMode: "runtime",
    footprint: runtimeFootprint([...unique.values()]),
  };
}
function showReport(value) {
  report = value;
  $("validationSummary").textContent = value.errors
    ? `${value.errors} 项错误 · ${value.warnings} 项提醒`
    : `校验通过 · ${value.warnings} 项提醒`;
  $("issues").replaceChildren();
  if (!value.issues.length)
    $("issues").append(text("p", "结构、路线和素材兼容检查通过。", "muted"));
  for (const issue of value.issues) {
    const b = text(
      "button",
      `${issue.severity === "error" ? "错误" : "提醒"}${issue.x != null ? " [" + issue.x + "," + issue.y + "]" : ""} · ${issue.message}`,
      "issue " + issue.severity,
    );
    b.onclick = () => {
      highlight = issue;
      selection = issue.objectId ? [issue.objectId] : [];
      renderInspector();
      render();
    };
    $("issues").append(b);
  }
  render();
}
async function save() {
  const data = copy(project()),
    before = revision;
  const result = await json("/api/save", data);
  if (before === revision) {
    project().draftId = result.draftId;
    history.markSaved();
    update();
  }
  toast("工程已保存到本机项目目录");
}
async function openValue(value, saved = false) {
  const before = revision;
  const checked = await json("/api/check", value);
  checkRevision(before);
  replaceProject(checked, saved);
  $("openDialog").close();
}
$$("[data-tool]").forEach((b) => (b.onclick = () => setTool(b.dataset.tool)));
$$("[data-library]").forEach(
  (b) =>
    (b.onclick = () => {
      library = b.dataset.library;
      page = 0;
      if (library === 'tiles' && (tool === 'lake' || tool === 'lakeErase')) setTool('select');
      $$("[data-library]").forEach((v) =>
        v.classList.toggle("active", v === b),
      );
      renderLibrary();
    }),
);
$$("[data-close]").forEach(
  (b) => (b.onclick = () => b.closest("dialog").close()),
);
bind("search", "input", () => {
  page = 0;
  renderLibrary();
});
bind("sheet", "change", () => {
  page = 0;
  renderLibrary();
});
bind("hideEmpty", "change", () => {
  page = 0;
  renderLibrary();
});
bind("prevPage", "click", () => {
  page--;
  renderLibrary();
});
bind("nextPage", "click", () => {
  page++;
  renderLibrary();
});
bind("new", "click", async () => {
  if (!guardReplace()) return;
  const before = revision,
    value = await json("/api/blank");
  checkRevision(before);
  replaceProject(value);
});
bind("open", "click", async () => {
  const drafts = await json("/api/drafts");
  $("draftList").replaceChildren();
  for (const draft of drafts) {
    const b = text(
      "button",
      draft.name + " · " + new Date(draft.modified * 1000).toLocaleString(),
    );
    b.onclick = async () => {
      try {
        if (!guardReplace()) return;
        const before = revision;
        const value = await json("/api/draft/" + draft.file);
        checkRevision(before);
        await openValue(value, true);
      } catch (e) {
        toast(e.message);
      }
    };
    $("draftList").append(b);
  }
  $("openDialog").showModal();
});
bind("openFile", "click", () => {
  if (guardReplace()) $("fileInput").click();
});
bind("fileInput", "change", async (e) => {
  const file = e.target.files[0];
  e.target.value = "";
  if (!file) return;
  if (file.size > 4 * 1024 * 1024) throw Error("工程文件超过 4 MB");
  await openValue(JSON.parse(await file.text()), true);
});
bind("recover", "click", async () => {
  const saved = localStorage.getItem("stickmon-map-editor-draft");
  if (!saved) throw Error("没有可恢复的浏览器草稿");
  if (guardReplace()) await openValue(JSON.parse(saved));
});
bind("save", "click", save);
bind("projectName", "input", () => {
  const name = $("projectName").value.trim();
  if (!name) {
    update();
    return;
  }
  mutate((p) => (p.name = name));
});
bind("undo", "click", () => {
  if (history.undo()) changed();
  renderLibrary();
});
bind("redo", "click", () => {
  if (history.redo()) changed();
  renderLibrary();
});
bind("snapshot", "click", () => $("snapshotDialog").showModal());
bind("importSnapshot", "click", async () => {
  if (!guardReplace()) return;
  const before = revision;
  $("importSnapshot").disabled = true;
  try {
    const value = await json("/api/snapshot", {
      area: Number($("area").value),
      seed: Number($("seed").value),
      edge: $("importEdge").value,
      level: Number($("level").value) - 1,
      count: Number($("count").value),
      node: Number($("node").value),
      entrance: Number($("entrance").value),
    });
    checkRevision(before);
    replaceProject(value);
    $("snapshotDialog").close();
    toast("已导入独立快照，可撤销返回原工程");
  } finally {
    $("importSnapshot").disabled = false;
  }
});
bind("export", "click", () => $("exportDialog").showModal());
bind("downloadProject", "click", async () => {
  await save();
  download(
    new Blob([JSON.stringify(project(), null, 2)], {
      type: "application/json",
    }),
    filename(".stickmap.json"),
  );
});
bind("downloadPng", "click", async () => {
  download(await (await api("/api/png", project())).blob(), filename(".png"));
  toast("原素材预览已导出");
});
bind("compile", "click", async () => {
  const before = revision;
  $("compile").disabled = true;
  try {
    const response = await api("/api/compile", copy(project()));
    checkRevision(before);
    download(await response.blob(), filename("-runtime.zip"));
    toast("已导出通过 C++ 指纹与路线校验的数据包");
  } catch (error) {
    $("exportDialog").close();
    throw error;
  } finally {
    $("compile").disabled = false;
  }
});
bind("validate", "click", async () => {
  const before = revision;
  const value = await json("/api/validate", copy(project()));
  checkRevision(before);
  showReport(value);
});
bind("clearIssues", "click", () => {
  highlight = null;
  render();
});
bind("refreshSources", "click", () => {
  if (confirm("请先核对原素材和预览。将本工程来源校验记录更新为当前素材？"))
    mutate((p) => {
      p.sources = copy(catalog.sources);
      p.catalogVersion = catalog.version;
    });
});
bind("toggleLeft", "click", () => {
  const workspace = document.querySelector(".workspace");
  if (matchMedia("(max-width: 900px)").matches && workspace.classList.contains("inspector-open")) {
    workspace.classList.remove("inspector-open", "hide-left");
    workspace.classList.add("hide-right");
  } else workspace.classList.toggle("hide-left");
  fit();
});
bind("toggleRight", "click", () => {
  const workspace = document.querySelector(".workspace");
  const hidden = workspace.classList.contains("hide-right") || getComputedStyle(document.querySelector(".inspector")).display === "none";
  workspace.classList.toggle("hide-right", !hidden);
  workspace.classList.toggle("inspector-open", hidden);
  fit();
});
bind("zoomIn", "click", () => {
  zoom = Math.min(6, zoom + 0.25);
  render();
});
bind("zoomOut", "click", () => {
  zoom = Math.max(0.25, zoom - 0.25);
  render();
});
bind("fit", "click", fit);
bind("scaleMode", "change", fit);
for (const id of ["grid", "routesVisible", "anchors", "actorVisible"])
  bind(id, "change", render);
bind("progress", "input", () => {
  playing = false;
  $("play").textContent = "播放";
  render();
});
bind("duplicate", "click", duplicate);
bind("delete", "click", removeSelection);
bind("explode", "click", () => {
  if (selectionLocked()) throw Error("选中物件所在图层已锁定");
  mutate((p) => {
    for (const o of p.map.objects.filter((o) => selection.includes(o.id)))
      for (const c of prefabFor(p, o.prefab).cells) {
        const x = o.x + c.x,
          y = o.y + c.y;
        if (inside(x, y)) p.map.layers[c.layer][index(x, y)] = c.asset;
      }
    p.map.objects = p.map.objects.filter((o) => !selection.includes(o.id));
  });
  selection = [];
  update();
});
let pendingCapture = null;
bind("capture", "click", () => {
  pendingCapture = capturedPrefab();
  $("prefabName").value = "自定义组合";
  $("nameDialog").showModal();
});
bind("nameDialog", "close", () => {
  if ($("nameDialog").returnValue !== "ok" || !pendingCapture) return;
  pendingCapture.name = $("prefabName").value.trim() || "自定义组合";
  const pref = pendingCapture;
  mutate((p) => p.prefabs.push(pref));
  selectedPrefab = pref.id;
  transientPrefab = null;
  library = "prefabs";
  category = "全部";
  $$("[data-library]").forEach((b) =>
    b.classList.toggle("active", b.dataset.library === library),
  );
  renderLibrary();
  setTool("stamp");
  pendingCapture = null;
});
bind("pathSelect", "change", () => {
  pathIndex = Number($("pathSelect").value);
  $("progress").value = 0;
  update();
});
bind("addPath", "click", () => {
  mutate((p) => {
    if (p.map.paths.length >= 3) throw Error("最多三条完整路线");
    p.map.paths.push({
      points: [copy(p.map.entry.point)],
      exit: { point: copy(p.map.entry.point), edge: "top" },
      cellCentered: false,
      fallsToNextLevel: false,
      nextValleyNode: 255,
      nextValleyEntrance: 0,
      terminal: false,
      interiorExit: false,
    });
    syncJunction(p);
  });
  pathIndex = project().map.paths.length - 1;
  setTool("route");
  update();
});
bind("removePath", "click", () =>
  mutate((p) => {
    p.map.paths.splice(pathIndex, 1);
    syncJunction(p);
  }),
);
bind("cellCentered", "change", () =>
  mutate(
    (p) => (p.map.paths[pathIndex].cellCentered = $("cellCentered").checked),
  ),
);
bind("entryEdge", "change", () =>
  mutate((p) => (p.map.entry.edge = $("entryEdge").value)),
);
bind("exitEdge", "change", () => {
  if (currentPath())
    mutate((p) => (p.map.paths[pathIndex].exit.edge = $("exitEdge").value));
});
bind("setEntry", "click", () => {
  pendingPoint = "entry";
  toast("单击地图设置入口；已有路线需同步调整");
});
bind("setExit", "click", () => {
  if (!currentPath()) throw Error("请先添加路线");
  pendingPoint = "exit";
  toast("单击地图设置出口");
});
bind("popPoint", "click", () => {
  if (currentPath())
    mutate((p) => {
      const path = p.map.paths[pathIndex];
      path.points.pop();
      if (path.points.length) path.exit.point = copy(path.points.at(-1));
      syncJunction(p);
    });
});
bind("recognizeRoads", "click", () => {
  const network = recognizeRoads(project(), catalog);
  $("routesVisible").checked = true;
  mutate(p => { p.map.roadNetwork = network; });
  toast(network.uncovered.length ? '路网已识别，琥珀色格子未接入' : '路网已识别');
});
bind("play", "click", () => {
  if (!currentPath()?.points.length) throw Error("请先绘制路线");
  if (!roadRouteCurrent(project(),currentPath())) throw Error('道路已变化，请重新识别并存为路线');
  playing = !playing;
  $("actorVisible").checked = true;
  $("play").textContent = playing ? "暂停" : "播放";
  render();
});
bind("saveRoadRoute", "click", async () => {
  if(savingRoadRoute)return;
  savingRoadRoute=true;
  try {
    mutate(p => { pathIndex=storeRoadRoute(p); });
    pendingPoint=null;
    $("progress").value=0; $("routesVisible").checked=true; $("actorVisible").checked=true;
    update();
    await save();
  } finally { savingRoadRoute=false;renderPaths(); }
});
function drawSheet() {
  const im = images[$("sheet").value];
  if (!im) return;
  const c = $("sourceSheet"),
    context = c.getContext("2d");
  if (c.width !== im.width || c.height !== im.height) {
    c.width = im.width;
    c.height = im.height;
  }
  context.clearRect(0, 0, c.width, c.height);
  context.drawImage(im, 0, 0);
  if (sourceRect) {
    context.fillStyle = "#a9ce9040";
    context.fillRect(
      sourceRect.x * 32,
      sourceRect.y * 32,
      sourceRect.w * 32,
      sourceRect.h * 32,
    );
    context.strokeStyle = "#e7b66a";
    context.lineWidth = 2;
    context.strokeRect(
      sourceRect.x * 32 + 1,
      sourceRect.y * 32 + 1,
      sourceRect.w * 32 - 2,
      sourceRect.h * 32 - 2,
    );
    $("sampleInfo").textContent = `${sourceRect.w} × ${sourceRect.h} 图块`;
  }
}
bind("sheetView", "click", () => {
  if ($("sheet").value === "runtime")
    throw Error("运行时别名包含多种来源，请用图块列表选择");
  sourceRect = null;
  drawSheet();
  $("sheetDialog").showModal();
});
function sheetCell(e) {
  const c = $("sourceSheet"),
    r = c.getBoundingClientRect();
  return {
    x: Math.max(
      0,
      Math.min(
        7,
        Math.floor((((e.clientX - r.left) / r.width) * c.width) / 32),
      ),
    ),
    y: Math.max(
      0,
      Math.min(
        c.height / 32 - 1,
        Math.floor((((e.clientY - r.top) / r.height) * c.height) / 32),
      ),
    ),
  };
}
$("sourceSheet").addEventListener("pointerdown", (e) => {
  sourceStart = sheetCell(e);
  sourceRect = { ...sourceStart, w: 1, h: 1 };
  $("sourceSheet").setPointerCapture(e.pointerId);
  drawSheet();
});
$("sourceSheet").addEventListener("pointermove", (e) => {
  if (!sourceStart) return;
  const b = sheetCell(e);
  sourceRect = {
    x: Math.min(sourceStart.x, b.x),
    y: Math.min(sourceStart.y, b.y),
    w: Math.abs(b.x - sourceStart.x) + 1,
    h: Math.abs(b.y - sourceStart.y) + 1,
  };
  drawSheet();
});
$("sourceSheet").addEventListener("pointerup", () => (sourceStart = null));
$("sourceSheet").addEventListener("pointercancel", () => (sourceStart = null));
bind("sampleSheet", "click", () => {
  if (!sourceRect) throw Error("请拖动选择原图区域");
  if (sourceRect.h > 12) throw Error("取样高度不能超过 12 格");
  const cells = sampleCells(catalog,$("sheet").value,sourceRect,layer);
  if (!cells.length) throw Error("所选区域只有空白或占位格");
  transientPrefab = {
    id: uid("sample"),
    name: "原图取样组合",
    category: "自定义",
    width: sourceRect.w,
    height: sourceRect.h,
    cells,
    anchor: [sourceRect.w / 2, sourceRect.h],
    anchorMode: "runtime",
    footprint: runtimeFootprint(cells),
  };
  setTool("stamp");
  $("sheetDialog").close();
  toast("已取样，单击地图放置");
});
document.addEventListener("keydown", (e) => {
  if (
    !history ||
    e.target.matches("input,select,textarea") ||
    document.querySelector("dialog[open], :popover-open")
  )
    return;
  try {
    const mod = e.metaKey || e.ctrlKey;
    if (mod && e.key.toLowerCase() === "s") {
      e.preventDefault();
      save().catch((error) => toast(error.message));
      return;
    }
    if (mod && e.key.toLowerCase() === "z") {
      e.preventDefault();
      (e.shiftKey ? $("redo") : $("undo")).click();
      return;
    }
    if (mod && e.key.toLowerCase() === "d") {
      e.preventDefault();
      duplicate();
      return;
    }
    if (mod && e.key.toLowerCase() === "c") {
      e.preventDefault();
      const object = selection.length === 1
        ? project().map.objects.find((o) => o.id === selection[0]) : null;
      const pref = object && prefabFor(project(),object.prefab);
      transientPrefab = rotationInfo(pref,catalog) ? copy(pref) : capturedPrefab();
      toast("已复制，可按 ⌘V 粘贴为组合");
      return;
    }
    if (mod && e.key.toLowerCase() === "v") {
      e.preventDefault();
      if (transientPrefab) setTool("stamp");
      return;
    }
    if (e.code === "Space") {
      if (e.target.closest("button,summary")) return;
      e.preventDefault();
      space = true;
      return;
    }
    if (e.key === "Escape") {
      pendingPoint = null;
      finishPointer(true);
      setTool("select");
      return;
    }
    if (e.key === "Delete" || e.key === "Backspace") {
      e.preventDefault();
      removeSelection();
      return;
    }
    if (!mod && !e.altKey && (e.code === "BracketLeft" || e.code === "BracketRight")) {
      e.preventDefault();
      rotateCurrent(e.code === "BracketRight" ? 1 : -1);
      return;
    }
    const keys = {
      v: "select",
      b: "stamp",
      p: "brush",
      e: "erase",
      w: "lake",
      f: "fill",
      i: "pick",
      m: "rect",
      r: "route",
    };
    if (keys[e.key.toLowerCase()]) setTool(keys[e.key.toLowerCase()]);
  } catch (error) {
    toast(error.message);
  }
});
document.addEventListener("keyup", (e) => {
  if (e.code === "Space") space = false;
});
window.addEventListener("blur", () => {
  space = false;
  finishPointer(true);
});
window.addEventListener("beforeunload", (e) => {
  if (history?.dirty) {
    e.preventDefault();
    e.returnValue = "";
  }
});
setInterval(() => {
  if (!history) return;
  if (playing) {
    const increment = currentPath()?.kind === 'roadNetwork' ? 250 / (currentPath().points.length-1) : 9;
    const value = Number($("progress").value) + increment;
    $("progress").value = value > 1000 ? 0 : value;
  }
  const animated = flatten(project()).layers.some((l) =>
    l.some((key) => catalog.assets[key]?.frames > 1),
  );
  const nextFrame = Math.floor(performance.now() / 280) % 8;
  if (animation !== nextFrame) {
    animation = nextFrame;
    animateLibrary();
  }
  if (playing || animated) {
    render();
  }
}, 150);
async function init() {
  catalog = await json("/api/catalog");
  const entries = await Promise.all(
    ["outside", "caves", "interior", "runtime", "sprite"].map(async (key) => [
      key,
      await loadImage(`/art/${key}.png`),
    ]),
  );
  images = Object.fromEntries(entries);
  history = new History(await json("/api/blank"));
  npcEditor = new NpcEditor({project,history:()=>history,tool:()=>tool,setTool,mutate,changed,render,fit,json,toast,catalog,animation:()=>animation});
  await npcEditor.load();
  renderLayers();
  renderLibrary();
  update();
  $("loading").hidden = true;
  fit();
  if (localStorage.getItem("stickmon-map-editor-draft"))
    toast("发现浏览器草稿，可在“打开”中恢复");
}
init().catch((error) => {
  $("loading").textContent = "加载失败：" + error.message;
  console.error(error);
});
setInterval(() => npcEditor?.tick(performance.now()),50);

bind("nativePreview", "click", async () => {
  const before = revision;
  $("nativePreview").disabled = true;
  $("nativePreview").textContent = "正在绘制…";
  try {
    const response = await api("/api/native-preview", copy(project()));
    checkRevision(before);
    download(await response.blob(), filename("-native-preview.zip"));
    toast("已导出正式资源、动态预览与 AMOLED 逐点行进画面");
  } finally {
    $("nativePreview").disabled = false;
    $("nativePreview").textContent = "正式绘制器预览 ZIP";
  }
});

let editedObject = null,
  editedPrefab = null;
bind("editObject", "click", () => {
  if (selection.length !== 1) return;
  editedObject = selection[0];
  const object = project().map.objects.find((o) => o.id === editedObject);
  editedPrefab = copy(prefabFor(project(), object.prefab));
  $("anchorX").value = editedPrefab.anchor[0];
  $("anchorY").value = editedPrefab.anchor[1];
  $("footprintGrid").replaceChildren();
  $("footprintGrid").style.gridTemplateColumns =
    `repeat(${editedPrefab.width},30px)`;
  for (let y = 0; y < editedPrefab.height; y++)
    for (let x = 0; x < editedPrefab.width; x++) {
      const b = text("button", `${x},${y}`);
      b.type = "button";
      b.classList.toggle(
        "blocked",
        editedPrefab.footprint.some((p) => p[0] === x && p[1] === y),
      );
      b.onclick = () => {
        const blocked = editedPrefab.footprint.some(
          (p) => p[0] === x && p[1] === y,
        );
        editedPrefab.footprint = editedPrefab.footprint.filter(
          (p) => p[0] !== x || p[1] !== y,
        );
        if (!blocked) editedPrefab.footprint.push([x, y]);
        b.classList.toggle("blocked", !blocked);
      };
      $("footprintGrid").append(b);
    }
  $("objectDialog").showModal();
});
bind("objectDialog", "close", () => {
  if ($("objectDialog").returnValue !== "ok" || !editedPrefab) return;
  if (selectionLocked()) throw Error("所在图层已锁定");
  editedPrefab.id = uid("instance");
  editedPrefab.anchor = [
    Number($("anchorX").value),
    Number($("anchorY").value),
  ];
  editedPrefab.anchorMode = "shared";
  mutate((p) => {
    p.prefabs.push(editedPrefab);
    p.map.objects.find((o) => o.id === editedObject).prefab = editedPrefab.id;
  });
  editedPrefab = null;
  renderLibrary();
});

function runtimeFootprint(cells) {
  const result = new Map();
  for (const cell of cells) {
    const a = catalog.assets[cell.asset];
    if (
      cell.layer &&
      a?.depth?.[cell.layer] === 1 &&
      ![391, 4773, 4747].includes(a.runtimeId)
    ) {
      result.set(`${cell.x}:${cell.y}`, [cell.x, cell.y]);
    }
  }
  return [...result.values()];
}

bind("applyMapSize", "click", () => {
  if (!["mapWidth","mapHeight"].every(id=>$(id).reportValidity())) return;
  const size = [Number($("mapWidth").value),Number($("mapHeight").value)];
  finishPointer();
  if (size.join() === dimensions(project()).join()) return;
  mutate(p => resizeMap(p,size,catalog.lake.defaultGround));
  selection=[]; rect=null; hover=null; pathIndex=Math.min(pathIndex,project().map.paths.length-1); pathIndex=Math.max(0,pathIndex);
  fit(); update();
  toast("地图尺寸已调整，可撤销。出入口随边界移动，请检查道路连接并重新识别路网；记得保存工程。");
});
