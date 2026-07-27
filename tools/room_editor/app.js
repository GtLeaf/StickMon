const TRIM_PADDING = 4;
const TRIM_THRESHOLD = 34;
const ITEM_MIN_TARGET_SIZE = 1;
const SESSION_STORAGE_KEY = "stickmon.roomEditor.lastSession.v1";
const LEFT_PANEL_STATE_KEY = "stickmon.roomEditor.leftPanels.v1";
const SESSION_DB_NAME = "stickmon-room-editor";
const SESSION_DB_STORE = "sessions";
const SESSION_DB_KEY = "last";
const ROOM_EDITOR_ROOT_HANDLE_KEY = "roomEditorRootHandle";
const SESSION_DEFAULT_FILE_NAME = "stickmon_room_editor_session.json";
const GAME_SCREEN_WIDTH = 240;
const GAME_SCREEN_HEIGHT = 135;
const PREVIEW_MAX_WIDTH = 240;
const LIGHT_HANDLE_RADIUS = 7;
const BASE_FIT_WIDTH = "fit_width";
const BACKGROUND_MODES = ["day", "night"];
const FURNITURE_LIBRARY_BUNDLE_TYPE = "stickmon-furniture-library-bundle";
const TOOLTIP_DELAY_MS = 900;
const CONTROL_TOOLTIPS = {
  baseInput: "导入一张或多张背景图。导入后可在 Layers 中设置 Day/Night 显示。",
  furnitureInput: "导入家具或道具 PNG。导入后可以在主画布拖动、缩放并配置影子。",
  layoutInput: "导入之前导出的 layout/session JSON，用于恢复房间配置。",
  baseOpacity: "调整主编辑画布中背景参考图的透明度，不影响最终导出。",
  sourceScale: "家具导入时的默认缩放比例。用源图像素除以这个值，得到游戏坐标尺寸。",
  furnitureImportMode: "选择家具 PNG 导入时如何确定初始大小。",
  baseFit: "背景适配方式。当前项目固定为宽度填满 240px 游戏画布。",
  spriteSpeciesSelect: "选择一个项目内精灵，用于预览精灵与家具遮挡关系。",
  spriteActionSelect: "选择精灵动作帧类型。",
  spriteFrameSelect: "选择具体帧。",
  addProjectSprite: "把当前选择的精灵帧放到画面中心做尺寸和遮挡预览。",
  librarySearch: "按名称、文件名、标签或库 ID 筛选家具库。",
  libraryCategory: "按家具类别筛选库中家具。",
  showGuides: "显示或隐藏 HUD 参考线。",
  showGrid: "显示 8px 网格，方便按游戏像素对齐家具。",
  gridOpacity: "调整 8px 网格线的透明度。只影响主编辑画布显示，不影响预览和导出。",
  snapToGrid: "拖动家具时吸附到 8px 网格。",
  dayMode: "切换到白天预览，绘制勾选 Day 的背景层和白天光照配置。",
  nightMode: "切换到夜晚预览，绘制勾选 Night 的背景层和夜晚光照配置。",
  furnitureMode: "家具模式：选择、拖动、缩放家具和精灵预览物。",
  shapeMode: "描面模式：选择或调整已有房间面。点击 Room faces 的 Add 后才会在画布上新增节点。",
  addRoomFace: "开始或暂停新增房间面。开启后点击画布生成节点，点击第一个节点闭合。",
  editZoom: "缩放主编辑画布的显示比例。只影响查看细节，不影响坐标和导出。",
  zoomOut: "缩小主编辑画布显示比例。",
  zoomIn: "放大主编辑画布显示比例。",
  zoomReset: "把主编辑画布显示比例恢复到 100%。",
  projectionGuides: "固定显示完整 2.5D 坐标与家具投影诊断。Shape 模式和阴影高级设置展开时会自动显示。",
  undoAction: "撤销上一步配置修改。",
  redoAction: "重做刚撤销的修改。",
  resetView: "清空当前选择并把主编辑画布缩放恢复到 100%。",
  preview: "最终游戏坐标预览。点击可放大查看。",
  selectTool: "选择工具：用于普通选择、拖动和编辑。",
  measureHeightTool: "测量工具：在高清主画布上测量高度，并换算成游戏像素。",
  clearMeasure: "清除当前测量线。",
  saveSession: "保存当前编辑器会话，同时更新项目 session JSON 和浏览器草稿缓存。",
  loadSession: "优先从项目 session JSON 恢复；找不到时再恢复浏览器未保存草稿。",
  changeSessionFile: "重新选择 room_editor 工具目录，session 默认保存到 sessions/ 下。",
  clearSession: "清除浏览器里的未保存草稿缓存，不删除项目 session JSON。",
  exportJson: "导出完整 layout JSON，已包含 roomGeometry、家具、引导线和光照配置。",
  exportPreview: "导出当前预览 PNG，用于快速检查最终效果。",
  exportFurnitureLibrary: "导出当前场景中已标注的家具库 bundle，作为直接写入不可用时的备用方式。",
  changeFurnitureLibraryRoot: "重新选择 room_editor 工具目录，用于 Add selected 和 session 保存。",
  exportSelectedFurnitureLibrary: "将当前选中的已标注家具添加到 room_editor 内置家具库。",
  duplicateItem: "复制当前选中的家具或精灵预览物。",
  deleteItem: "删除当前选中的家具或精灵预览物。",
  copyLightToDay: "将当前右侧光源参数写入白天配置。",
  copyLightToNight: "将当前右侧光源参数写入夜晚配置。",
  lightShape: "选择光照形状。Cone 适合模拟从窗户射入的光束。",
  lightStrength: "光照强度。数值越大，光照区域越亮。",
  lightRadius: "光照影响范围。",
  lightX: "光源在游戏坐标中的 X 位置，也可以直接拖动画布中的光源点。",
  lightY: "光源在游戏坐标中的 Y 位置，也可以直接拖动画布中的光源点。",
  lightDepth: "光源深度，用于估算物体投影长度和透视感。",
  lightAngle: "锥形光中心方向角度，也可以拖动画布中的方向点调整。",
  lightSpread: "锥形光展开角度。方向点离光源越远，展开越大。",
  castShadows: "开启或关闭家具投影绘制。",
  shadowMode: "选择投影算法。Auto 会根据光源形状自动选择。",
  shadowAlpha: "投影不透明度。",
  shadowLength: "投影长度增益。",
  shadowBlur: "投影模糊半径。",
  itemName: "当前物体名称，仅用于管理和导出识别。",
  itemKind: "物理行为预设。控制默认高度、投影、接地方式、遮挡和自动绘制层。",
  itemFurnitureType: "资源分类。用于家具库筛选和游戏语义，不改变渲染行为。",
  itemX: "物体左上角 X 坐标，单位是游戏像素。",
  itemY: "物体左上角 Y 坐标，单位是游戏像素。",
  itemW: "物体目标宽度，单位是游戏像素。",
  itemH: "物体目标高度，单位是游戏像素。",
  itemScale: "按源图尺寸缩放物体。",
  itemOpacity: "编辑器内物体透明度。",
  itemAspectLock: "锁定宽高比例，调整宽度时同步高度。",
  itemVisibleDay: "这个家具是否在白天房间中显示。",
  itemVisibleNight: "这个家具是否在夜晚房间中显示。",
  itemCastsShadow: "这个物体是否产生投影。",
  itemShadowOpacity: "这个物体自身投影的不透明度。",
  itemShadowLength: "这个物体自身投影长度。",
  itemShadowBlur: "这个物体自身投影模糊半径。",
  itemHeightPx: "物体估算高度，用于计算影子长度和墙面投影。",
  itemFootprint: "物体接触地面的形状，用于生成更自然的投影。",
  itemFootprintPolygon: "自定义接地区域多边形，使用 0-1 的物体本地坐标，每行一个 x,y。",
  itemShadowPolygon: "自定义投影轮廓多边形，使用 0-1 的物体本地坐标。留空时使用图片 alpha。",
  itemShadowAnchor: "投影锚点。墙上架子等物体应使用墙面锚点。",
  clearShadowFaceTargets: "清空指定墙面，恢复自动投影面选择。",
  itemWallShadowDepthPx: "墙面投影深度，用于控制墙上物体影子下落距离。",
  itemWallShadowOffsetY: "墙面投影垂直偏移，用于微调墙上影子位置。",
  itemReceivesShadow: "这个物体是否接收其它物体投影。",
  itemOccludesSprite: "精灵经过时，这个物体是否遮挡精灵。",
  itemZ: "主要绘制顺序。数值越大越靠上。",
  itemSortY: "同一 Z 值内的排序参考 Y 坐标。"
};
const CONTROL_TOOLTIP_RULES = [
  ["#backgroundDropZone", "拖入一张或多张房间背景图。"],
  ["#furnitureDropZone", "拖入一张或多张家具 PNG，随后进入抠图流程。"],
  [".preview-canvas", CONTROL_TOOLTIPS.preview],
  [".asset-visibility", "显示或隐藏这个图层。隐藏后不会在预览里绘制。"],
  [".face-toggle", "展开或收起这个房间面的点位列表。"],
  [".face-visibility", "在画布中显示或隐藏这个房间面的编辑覆盖层，不影响阴影接收。"],
  [".face-delete", "删除这个房间面。"],
  [".point-select", "选择这个点。选中后可在画布拖动或在右侧输入坐标。"],
  [".point-x", "点在高清编辑画布中的 X 坐标，导出时会换算成游戏像素。"],
  [".point-y", "点在高清编辑画布中的 Y 坐标，导出时会换算成游戏像素。"],
  [".face-type-select", "设置这个面是地板、墙面、精灵可活动区域或门口。语义区域只用于导出标注，不参与背景或阴影绘制。"],
  [".face-shadow-surface", "指定投影承接面：地板、左墙或右墙。"],
  [".face-receives-shadow", "控制这个房间面是否接收家具投影。"],
  [".shadow-face-target", "限制当前家具的阴影只出现在勾选的墙面上。"],
  [".insert-point", "在当前边后插入一个新点，用于细化墙面或地板轮廓。"],
  [".point-delete", "删除这一行对应的点。闭合面至少保留 3 个点。"],
  [".library-add", "把这个库中家具加入当前房间。"],
  [".library-delete", "从项目家具库中删除这个家具。当前房间里已放置的实例会保留为本地物体。"]
];
const FURNITURE_TYPES = [
  { value: "bed", label: "床" },
  { value: "bowl", label: "碗" },
  { value: "decoration", label: "装饰物品" },
  { value: "toy", label: "玩具" }
];
const ITEM_KINDS = [
  { value: "floor_flat", label: "地面平铺", summary: "地面 · 无投影 · 不遮挡精灵", heightPx: 0, footprint: "polygon", shadowAnchor: "floor", castsShadow: false, receivesShadow: true, occludesSprite: false, slot: "floor", layer: "floor" },
  { value: "floor_small", label: "地面小物件", summary: "地面 · 小型投影 · 不遮挡精灵", heightPx: 12, footprint: "ellipse", shadowAnchor: "floor", castsShadow: true, receivesShadow: true, occludesSprite: false, slot: "floor", layer: "main" },
  { value: "floor_furniture", label: "地面家具", summary: "地面 · 家具投影 · 遮挡精灵", heightPx: 32, footprint: "rect", shadowAnchor: "floor", castsShadow: true, receivesShadow: true, occludesSprite: true, slot: "floor", layer: "main" },
  { value: "wall_flat", label: "墙面装饰", summary: "墙面 · 无投影 · 不遮挡精灵", heightPx: 0, wallDepthPx: 4, footprint: "none", shadowAnchor: "wall", castsShadow: false, receivesShadow: false, occludesSprite: false, slot: "wall", layer: "wall" },
  { value: "wall_mounted", label: "墙挂物件", summary: "墙面 · 墙面投影 · 不遮挡精灵", heightPx: 24, wallDepthPx: 24, footprint: "rect", shadowAnchor: "wall", castsShadow: true, receivesShadow: true, occludesSprite: false, slot: "wall", layer: "wall" }
];
const LEGACY_ITEM_KIND_MAP = Object.freeze({
  rug: "floor_flat",
  floor_decal: "floor_flat",
  food_bowl: "floor_small",
  decoration_prop: "floor_small",
  low_prop: "floor_small",
  bed_sofa: "floor_furniture",
  furniture: "floor_furniture",
  tall_furniture: "floor_furniture",
  wall_prop: "wall_flat",
  wall_shelf: "wall_mounted",
  wall_furniture: "wall_mounted"
});
const FOOTPRINT_TYPES = [
  { value: "auto", label: "自动" },
  { value: "ellipse", label: "椭圆" },
  { value: "rect", label: "矩形" },
  { value: "polygon", label: "多边形" },
  { value: "none", label: "无" }
];
const LIGHT_SHAPES = [
  { value: "radial", label: "圆形" },
  { value: "cone", label: "锥形" }
];
const SHADOW_MODES = [
  { value: "auto", label: "自动" },
  { value: "point", label: "点光源" },
  { value: "directional", label: "方向光" }
];
const SHADOW_SURFACES = [
  { value: "floor", label: "地板" },
  { value: "left_wall", label: "左墙" },
  { value: "right_wall", label: "右墙" }
];
const SHADOW_ANCHORS = [
  { value: "auto", label: "自动" },
  { value: "floor", label: "地面" },
  { value: "wall", label: "墙面" }
];
const LIGHT_SETTING_KEYS = [
  "lightShape",
  "lightStrength",
  "lightX",
  "lightY",
  "lightDepth",
  "lightRadius",
  "lightAngle",
  "lightSpread"
];
const WALL_MOUNTED_NAME_PATTERN = /(sheld|wall[_ -]?shelf|shelf[_ -]?wall|mounted[_ -]?shelf|hanging[_ -]?shelf|壁架|挂架|墙架|层板|搁板)/i;
const FACE_TYPE_SPRITE_AREA = "sprite_area";
const FACE_TYPE_DOORWAY = "doorway";

const DEFAULT_NIGHT = {
  lightShape: "radial",
  lightStrength: 28,
  lightX: 170,
  lightY: 70,
  lightDepth: 180,
  lightRadius: 68,
  lightAngle: 125,
  lightSpread: 42,
  castShadows: true,
  shadowMode: "auto",
  shadowAlpha: 38,
  shadowLength: 28,
  shadowBlur: 0.6
};

function normalizeFurnitureType(value) {
  return FURNITURE_TYPES.some((type) => type.value === value) ? value : "decoration";
}

function furnitureTypeLabel(value) {
  const normalized = normalizeFurnitureType(value);
  return FURNITURE_TYPES.find((type) => type.value === normalized)?.label || "装饰物品";
}

function normalizeItemKind(value) {
  const canonical = LEGACY_ITEM_KIND_MAP[value] || value;
  return ITEM_KINDS.some((kind) => kind.value === canonical) ? canonical : "floor_furniture";
}

function normalizeItemKindForRecord(value, record = {}) {
  if (value === "wall_prop") return record.castsShadow === true ? "wall_mounted" : "wall_flat";
  if (value === "wall_shelf" || value === "wall_furniture") return "wall_mounted";
  return normalizeItemKind(value);
}

function itemKindPreset(value) {
  const normalized = normalizeItemKind(value);
  return ITEM_KINDS.find((kind) => kind.value === normalized) || ITEM_KINDS[2];
}

function itemKindLabel(value) {
  return itemKindPreset(value).label;
}

function normalizeFootprint(value) {
  return FOOTPRINT_TYPES.some((type) => type.value === value) ? value : "auto";
}

function normalizeLightShape(value) {
  return LIGHT_SHAPES.some((shape) => shape.value === value) ? value : "radial";
}

function normalizeShadowMode(value) {
  return SHADOW_MODES.some((mode) => mode.value === value) ? value : "auto";
}

function normalizeShadowSurface(value, faceType = "floor") {
  if (SHADOW_SURFACES.some((surface) => surface.value === value)) return value;
  return faceType === "wall" ? "left_wall" : "floor";
}

function normalizeShadowAnchor(value) {
  return SHADOW_ANCHORS.some((anchor) => anchor.value === value) ? value : "auto";
}

function normalizeShadowFaceIds(value) {
  const raw = Array.isArray(value) ? value : [];
  return [...new Set(raw.map((id) => String(id || "").trim()).filter(Boolean))];
}

function normalizeLightProfile(value) {
  return ["shared", "day", "night"].includes(value) ? value : "shared";
}

function normalizeFaceType(value) {
  if (value === "wall" || value === FACE_TYPE_SPRITE_AREA || value === FACE_TYPE_DOORWAY) return value;
  return "floor";
}

function faceTypeLabel(value) {
  const type = normalizeFaceType(value);
  if (type === "wall") return "wall";
  if (type === FACE_TYPE_SPRITE_AREA) return "sprite area";
  if (type === FACE_TYPE_DOORWAY) return "门口";
  return "floor";
}

function isSpriteAreaFace(face) {
  return normalizeFaceType(face?.type) === FACE_TYPE_SPRITE_AREA;
}

function isDoorwayFace(face) {
  return normalizeFaceType(face?.type) === FACE_TYPE_DOORWAY;
}

function isSemanticAreaFace(face) {
  return isSpriteAreaFace(face) || isDoorwayFace(face);
}

function semanticFaceSummary(face) {
  if (isSpriteAreaFace(face)) return "movement area";
  if (isDoorwayFace(face)) return "transition area";
  return "";
}

function faceVisible(face) {
  return face?.visible !== false;
}

function normalizeLightSettings(value = {}, fallback = DEFAULT_NIGHT) {
  return {
    lightShape: normalizeLightShape(value.lightShape ?? fallback.lightShape),
    lightStrength: coerceNumber(value.lightStrength, fallback.lightStrength, 0, 200),
    lightX: coerceNumber(value.lightX, fallback.lightX, -2048, 2048),
    lightY: coerceNumber(value.lightY, fallback.lightY, -2048, 2048),
    lightDepth: coerceNumber(value.lightDepth, fallback.lightDepth, 40, 480),
    lightRadius: coerceNumber(value.lightRadius, fallback.lightRadius, 8, 240),
    lightAngle: coerceNumber(value.lightAngle, fallback.lightAngle, 0, 359),
    lightSpread: coerceNumber(value.lightSpread, fallback.lightSpread, 5, 160)
  };
}

function clampNumber(value, min, max) {
  return Math.max(min, Math.min(max, Number(value)));
}

function coerceNumber(value, fallback, min, max) {
  const number = Number(value);
  const next = Number.isFinite(number) ? number : fallback;
  return clampNumber(next, min, max);
}

function normalizeNightSettings(value = {}) {
  const sharedLight = normalizeLightSettings(value, DEFAULT_NIGHT);
  return {
    ...sharedLight,
    separateModeLights: value.separateModeLights === true,
    dayLight: normalizeLightSettings(value.dayLight, sharedLight),
    nightLight: normalizeLightSettings(value.nightLight, sharedLight),
    castShadows: value.castShadows ?? DEFAULT_NIGHT.castShadows,
    shadowMode: normalizeShadowMode(value.shadowMode),
    shadowAlpha: coerceNumber(value.shadowAlpha, DEFAULT_NIGHT.shadowAlpha, 0, 80),
    shadowLength: coerceNumber(value.shadowLength, DEFAULT_NIGHT.shadowLength, 0, 80),
    shadowBlur: coerceNumber(value.shadowBlur, DEFAULT_NIGHT.shadowBlur, 0, 12)
  };
}

function inferFurnitureType(name) {
  const text = String(name || "").toLowerCase();
  if (text.includes("bed") || text.includes("床")) return "bed";
  if (text.includes("bowl") || text.includes("dish") || text.includes("碗")) return "bowl";
  if (text.includes("toy") || text.includes("玩具")) return "toy";
  return "decoration";
}

function inferItemKind(name, furnitureType = "") {
  const text = String(name || "").toLowerCase();
  if (text.includes("rug") || text.includes("carpet") || text.includes("mat") ||
      text.includes("地毯") || text.includes("毯") || text.includes("垫")) {
    return "floor_flat";
  }
  if (text.includes("decal") || text.includes("floor") || text.includes("贴花") || text.includes("地面")) {
    return "floor_flat";
  }
  if (WALL_MOUNTED_NAME_PATTERN.test(text)) {
    return "wall_mounted";
  }
  if (text.includes("wall") || text.includes("window") || text.includes("painting") ||
      text.includes("frame") || text.includes("poster") || text.includes("窗") ||
      text.includes("画") || text.includes("墙") || text.includes("壁")) {
    return "wall_flat";
  }
  if (text.includes("cabinet") || text.includes("wardrobe") || text.includes("closet") ||
      text.includes("shelf") || text.includes("bookcase") || text.includes("fridge") ||
      text.includes("柜") || text.includes("书架") || text.includes("冰箱")) {
    return "floor_furniture";
  }
  if (text.includes("sofa") || text.includes("couch") || text.includes("chair") ||
      text.includes("table") || text.includes("desk") || text.includes("bed") ||
      text.includes("沙发") || text.includes("椅") || text.includes("桌") || text.includes("床")) {
    return "floor_furniture";
  }
  if (furnitureType === "bowl" || furnitureType === "toy" ||
      text.includes("bowl") || text.includes("dish") || text.includes("toy") ||
      text.includes("ball") || text.includes("food") || text.includes("碗") ||
      text.includes("玩具") || text.includes("球") || text.includes("食")) {
    return "floor_small";
  }
  return "floor_small";
}

const state = {
  width: 240,
  height: 135,
  editWidth: 240,
  editHeight: 135,
  mode: "day",
  lightProfile: "day",
  editMode: "furniture",
  toolMode: "select",
  previewZoomed: false,
  editZoom: 1,
  base: null,
  backgroundItems: [],
  nextBackgroundId: 1,
  backgrounds: {
    day: null,
    night: null
  },
  baseFit: BASE_FIT_WIDTH,
  baseOpacity: 0.65,
  sourceScale: 4,
  baseTrim: null,
  furnitureImportMode: "source_scale",
  aspectLocked: true,
  aspectRatio: 240 / 135,
  items: [],
  selectedId: null,
  nextId: 1,
  points: [],
  faces: [],
  selectedFaceId: null,
  selectedPointIndex: null,
  selectedPointId: null,
  expandedFaceIds: new Set(),
  draftExpanded: true,
  nextFaceId: 1,
  nextPointId: 1,
  draftPoints: [],
  drawingFace: false,
  draggingPoint: false,
  dragPointId: null,
  dragging: false,
  draggingItemPolygon: false,
  activeItemPolygonKey: "footprintPolygon",
  itemPolygonDraft: {
    active: false,
    itemId: null,
    key: "footprintPolygon",
    points: [],
    dragging: false,
    dragIndex: null,
    moved: false,
    downPoint: null
  },
  dragPolygonKey: null,
  dragPolygonIndex: null,
  draggingLight: false,
  draggingConeControl: false,
  lightSelected: false,
  dragOffsetX: 0,
  dragOffsetY: 0,
  showProjectionGuides: false,
  showGrid: false,
  gridOpacity: 0.18,
  snapToGrid: false,
  measure: {
    dragging: false,
    start: null,
    end: null
  },
  guides: {
    show: true,
    spriteX: 88,
    spriteY: 50,
    spriteW: 64,
    spriteH: 64,
    hudX: 168,
    hudY: 2,
    hudW: 68,
    hudH: 54
  },
  night: normalizeNightSettings(DEFAULT_NIGHT),
  history: [],
  historyIndex: -1,
  maxHistory: 50,
  statusHoldUntil: 0
};

const canvas = document.getElementById("stage");
const ctx = canvas.getContext("2d");
const previewCanvas = document.getElementById("preview");
const previewCtx = previewCanvas.getContext("2d");
const statusEl = document.getElementById("status");
const assetList = document.getElementById("assetList");
const selectedPanel = document.getElementById("selectedPanel");
const faceList = document.getElementById("faceList");
const selectedFacePanel = document.getElementById("selectedFacePanel");
const roomFacesPanel = document.querySelector(".room-faces-panel");
const addRoomFaceButton = document.getElementById("addRoomFace");
const layersPanel = document.getElementById("layersPanel");
const propertiesPanel = document.getElementById("propertiesPanel");
const lightShadowsPanel = document.getElementById("lightShadowsPanel");
const sessionExportMeta = document.getElementById("sessionExportMeta");
const baseImageMeta = document.getElementById("baseImageMeta");
const furnitureLibraryList = document.getElementById("furnitureLibraryList");
const furnitureLibraryMeta = document.getElementById("furnitureLibraryMeta");
const librarySearchInput = document.getElementById("librarySearch");
const libraryCategoryInput = document.getElementById("libraryCategory");
const hoverTooltip = document.getElementById("hoverTooltip");
let tooltipTimer = null;
let tooltipAnchor = null;
let roomEditorRootHandle = null;
let tooltipPointer = { x: 0, y: 0 };

function setStatus(text, holdMs = 2500) {
  statusEl.textContent = text;
  state.statusHoldUntil = Date.now() + holdMs;
}

function setWorkflowStatus(text) {
  if (Date.now() < state.statusHoldUntil) return;
  statusEl.textContent = text;
}

function updateCanvasSizeReadouts() {
  const exportSize = document.getElementById("exportCanvasSize");
  const editSize = document.getElementById("editCanvasSize");
  const previewSize = document.getElementById("previewCanvasSize");
  if (exportSize) exportSize.textContent = `${Math.round(state.width)}x${Math.round(state.height)}`;
  if (editSize) editSize.textContent = `${Math.round(state.editWidth)}x${Math.round(state.editHeight)}`;
  if (previewSize) {
    previewSize.textContent =
      `Room ${Math.round(state.width)}x${Math.round(state.height)} · ` +
      `Screen ${GAME_SCREEN_WIDTH}x${GAME_SCREEN_HEIGHT}`;
  }
}

function normalizeBackgroundMode(mode) {
  return mode === "night" ? "night" : "day";
}

function backgroundItems() {
  if (!Array.isArray(state.backgroundItems)) state.backgroundItems = [];
  return state.backgroundItems;
}

function backgroundVisibleInMode(background, mode = state.mode) {
  if (!background || background.visible === false) return false;
  return normalizeBackgroundMode(mode) === "night"
    ? background.visibleInNight !== false
    : background.visibleInDay !== false;
}

function backgroundLayersForMode(mode = state.mode) {
  const normalized = normalizeBackgroundMode(mode);
  return backgroundItems()
    .filter((background) => backgroundVisibleInMode(background, normalized))
    .sort((a, b) => (a.z ?? 0) - (b.z ?? 0));
}

function firstBackgroundLayerForMode(mode = state.mode) {
  return backgroundLayersForMode(mode)[0] || null;
}

function backgroundForMode(mode = state.mode) {
  const normalized = normalizeBackgroundMode(mode);
  const layer = firstBackgroundLayerForMode(normalized);
  if (layer) return layer;
  if (backgroundItems().length) return null;
  const backgrounds = state.backgrounds || {};
  return backgrounds[normalized] || backgrounds.day || backgrounds.night || state.base || null;
}

function primaryBackground() {
  const primary = backgroundItems().find((background) =>
    background.visible !== false && (background.visibleInDay !== false || background.visibleInNight !== false)
  ) || backgroundItems()[0];
  if (primary) return primary;
  const backgrounds = state.backgrounds || {};
  return backgrounds.day || backgrounds.night || state.base || null;
}

function setLegacyBaseFromBackgrounds() {
  if (backgroundItems().length) {
    state.backgrounds = {
      day: firstBackgroundLayerForMode("day"),
      night: firstBackgroundLayerForMode("night")
    };
  }
  state.base = primaryBackground();
}

function hasBackground() {
  return Boolean(primaryBackground());
}

function backgroundLabel(mode) {
  return normalizeBackgroundMode(mode) === "night" ? "Night" : "Day";
}

function backgroundFileName(mode) {
  const bg = backgroundForMode(mode);
  return bg?.name || "";
}

function backgroundMetaText(mode) {
  const normalized = normalizeBackgroundMode(mode);
  const layers = backgroundLayersForMode(normalized);
  const bg = layers[0] || (state.backgrounds || {})[normalized];
  if (!bg) {
    return normalized === "night" && (firstBackgroundLayerForMode("day") || (state.backgrounds || {}).day)
      ? "not loaded, using day background"
      : "not loaded";
  }
  const suffix = layers.length > 1 ? ` +${layers.length - 1} layer(s)` : "";
  return `${bg.name} ${bg.width}x${bg.height}${suffix}`;
}

function updateBaseImageMeta() {
  updateCanvasSizeReadouts();
  if (!hasBackground()) {
    baseImageMeta.textContent = "Backgrounds: none";
    baseImageMeta.removeAttribute("title");
    return;
  }
  const prepared = preparedRoomMetrics();
  const trim = prepared.trim;
  const fullMeta =
    `Backgrounds: day ${backgroundMetaText("day")}; night ${backgroundMetaText("night")}; ` +
    `trim ${trim.x},${trim.y},${trim.width}x${trim.height}; ` +
    `export ${state.width}x${state.height}; edit ${state.editWidth}x${state.editHeight}; screen ${GAME_SCREEN_WIDTH}x${GAME_SCREEN_HEIGHT}`;
  baseImageMeta.textContent =
    `${backgroundItems().length} background layer(s) · room ${state.width}x${state.height} · edit ${state.editWidth}x${state.editHeight}`;
  baseImageMeta.title = fullMeta;
}

function normalizedEditZoom(value) {
  const percent = Number(value);
  const safePercent = Number.isFinite(percent) ? percent : 100;
  return clampNumber(Math.round(safePercent / 25) * 25, 25, 300) / 100;
}

function updateZoomControls() {
  const input = document.getElementById("editZoom");
  const reset = document.getElementById("zoomReset");
  if (input) input.value = Math.round(state.editZoom * 100);
  if (reset) reset.textContent = `${Math.round(state.editZoom * 100)}%`;
}

function updateGridOpacityLabel() {
  const label = document.getElementById("gridOpacityValue");
  if (label) label.textContent = `${Math.round(state.gridOpacity * 100)}%`;
}

function controlForTooltip(anchor) {
  if (!anchor) return null;
  if (anchor.id && CONTROL_TOOLTIPS[anchor.id]) return anchor;
  return anchor.querySelector?.("input[id], select[id], button[id], canvas[id]") || null;
}

function tooltipTextForElement(anchor) {
  if (!anchor) return "";
  const direct = anchor.dataset?.tooltip;
  if (direct) return direct;
  const control = controlForTooltip(anchor);
  if (control?.id && CONTROL_TOOLTIPS[control.id]) return CONTROL_TOOLTIPS[control.id];
  for (const [selector, text] of CONTROL_TOOLTIP_RULES) {
    if (anchor.matches?.(selector) || anchor.closest?.(selector)) return text;
  }
  return anchor.getAttribute?.("title") || "";
}

function tooltipAnchorForTarget(target) {
  if (!target?.closest) return null;
  const candidate = target.closest("[data-tooltip], button, label, select, input, canvas, summary, .asset-visibility, .drop-zone");
  if (!candidate) return null;
  return tooltipTextForElement(candidate) ? candidate : null;
}

function positionTooltip() {
  if (!hoverTooltip || hoverTooltip.hidden) return;
  const margin = 8;
  const offset = 14;
  const rect = hoverTooltip.getBoundingClientRect();
  let left = tooltipPointer.x + offset;
  let top = tooltipPointer.y + offset;
  if (left + rect.width > window.innerWidth - margin) {
    left = window.innerWidth - rect.width - margin;
  }
  if (top + rect.height > window.innerHeight - margin) {
    top = tooltipPointer.y - rect.height - offset;
  }
  hoverTooltip.style.left = `${Math.max(margin, left)}px`;
  hoverTooltip.style.top = `${Math.max(margin, top)}px`;
}

function showTooltip() {
  if (!hoverTooltip || !tooltipAnchor) return;
  const text = tooltipTextForElement(tooltipAnchor);
  if (!text) return;
  hoverTooltip.textContent = text;
  hoverTooltip.hidden = false;
  positionTooltip();
}

function hideTooltip() {
  if (tooltipTimer) {
    clearTimeout(tooltipTimer);
    tooltipTimer = null;
  }
  tooltipAnchor = null;
  if (hoverTooltip) hoverTooltip.hidden = true;
}

function scheduleTooltip(event) {
  const anchor = tooltipAnchorForTarget(event.target);
  if (!anchor) {
    hideTooltip();
    return;
  }
  tooltipPointer = { x: event.clientX, y: event.clientY };
  if (tooltipAnchor === anchor) {
    positionTooltip();
    return;
  }
  hideTooltip();
  tooltipAnchor = anchor;
  tooltipTimer = window.setTimeout(() => {
    tooltipTimer = null;
    showTooltip();
  }, TOOLTIP_DELAY_MS);
}

function setupHoverTooltips() {
  document.addEventListener("pointerover", scheduleTooltip, true);
  document.addEventListener("pointermove", (event) => {
    if (!tooltipAnchor) return;
    tooltipPointer = { x: event.clientX, y: event.clientY };
    positionTooltip();
  }, true);
  document.addEventListener("pointerout", (event) => {
    if (!tooltipAnchor) return;
    const next = event.relatedTarget;
    if (next && tooltipAnchor.contains?.(next)) return;
    hideTooltip();
  }, true);
  document.addEventListener("scroll", hideTooltip, true);
  document.addEventListener("keydown", hideTooltip, true);
}

function setEditZoom(percent) {
  state.editZoom = normalizedEditZoom(percent);
  updateCanvasDisplaySize();
  render();
}

function updateCanvasDisplaySize() {
  const displayWidth = Math.max(1, Math.round(state.editWidth * state.editZoom));
  canvas.style.width = `${displayWidth}px`;
  canvas.style.height = "auto";
  canvas.style.aspectRatio = `${state.editWidth} / ${state.editHeight}`;
  previewCanvas.style.width = `${Math.min(PREVIEW_MAX_WIDTH, state.width)}px`;
  previewCanvas.style.height = "auto";
  previewCanvas.style.aspectRatio = `${state.width} / ${state.height}`;
  updateZoomControls();
  updateCanvasSizeReadouts();
}

function editScaleX() {
  return state.editWidth / Math.max(1, state.width);
}

function editScaleY() {
  return state.editHeight / Math.max(1, state.height);
}

function targetToEditPoint(point) {
  return {
    x: point.x * editScaleX(),
    y: point.y * editScaleY()
  };
}

function editToTargetPoint(point) {
  return {
    x: point.x / editScaleX(),
    y: point.y / editScaleY()
  };
}

function clampCanvasPoint(point) {
  return {
    x: clampNumber(Math.round(point.x), 0, state.editWidth),
    y: clampNumber(Math.round(point.y), 0, state.editHeight)
  };
}

function formatPixelValue(value) {
  if (value == null || !Number.isFinite(value)) return "--";
  const rounded = Math.round(value * 10) / 10;
  return `${Number.isInteger(rounded) ? rounded : rounded.toFixed(1)} px`;
}

function currentMeasurement() {
  if (!state.measure.start || !state.measure.end) return null;
  const start = state.measure.start;
  const end = state.measure.end;
  const targetStart = editToTargetPoint(start);
  const targetEnd = editToTargetPoint(end);
  return {
    start,
    end,
    top: Math.min(start.y, end.y),
    bottom: Math.max(start.y, end.y),
    canvasHeight: Math.abs(end.y - start.y),
    gameHeight: Math.abs(targetEnd.y - targetStart.y),
    gameTop: Math.min(targetStart.y, targetEnd.y),
    gameBottom: Math.max(targetStart.y, targetEnd.y)
  };
}

function measurementStatusText() {
  const measurement = currentMeasurement();
  if (!measurement) return "Measure height tool active.";
  return `Height: ${formatPixelValue(measurement.canvasHeight)} canvas, ${formatPixelValue(measurement.gameHeight)} game.`;
}

function updateMeasurePanel() {
  const measurement = currentMeasurement();
  const canvasHeight = document.getElementById("measureCanvasHeight");
  const gameHeight = document.getElementById("measureGameHeight");
  const yRange = document.getElementById("measureYRange");
  const clearMeasure = document.getElementById("clearMeasure");
  const toolboxMeta = document.getElementById("toolboxMeta");

  canvasHeight.textContent = measurement ? formatPixelValue(measurement.canvasHeight) : "--";
  gameHeight.textContent = measurement ? formatPixelValue(measurement.gameHeight) : "--";
  yRange.textContent = measurement ?
    `canvas ${Math.round(measurement.top)}-${Math.round(measurement.bottom)} / game ${Math.round(measurement.gameTop * 10) / 10}-${Math.round(measurement.gameBottom * 10) / 10}` :
    "--";
  clearMeasure.disabled = !measurement;
  toolboxMeta.textContent = state.toolMode === "measureHeight" ? "Measure height" : "Select";
}

function updateToolButtons() {
  document.getElementById("selectTool").classList.toggle("active", state.toolMode === "select");
  document.getElementById("measureHeightTool").classList.toggle("active", state.toolMode === "measureHeight");
  canvas.classList.toggle("measure-tool", state.toolMode === "measureHeight");
  updateMeasurePanel();
}

function setToolMode(mode) {
  state.toolMode = mode === "measureHeight" ? "measureHeight" : "select";
  state.measure.dragging = false;
  canvas.classList.remove("dragging");
  updateToolButtons();
  render();
  setStatus(state.toolMode === "measureHeight" ? "Measure height tool active." : "Select tool active.");
}

function setMeasureStart(point) {
  const clamped = clampCanvasPoint(point);
  state.measure.start = clamped;
  state.measure.end = { ...clamped };
}

function setMeasureEnd(point) {
  if (!state.measure.start) {
    setMeasureStart(point);
    return;
  }
  const clamped = clampCanvasPoint(point);
  state.measure.end = {
    x: state.measure.start.x,
    y: clamped.y
  };
}

function clearMeasurement() {
  state.measure.dragging = false;
  state.measure.start = null;
  state.measure.end = null;
  canvas.classList.remove("dragging");
  updateMeasurePanel();
  render();
  setStatus("Measure cleared.");
}

function colorDistanceRgb(a, b) {
  const dr = a[0] - b[0];
  const dg = a[1] - b[1];
  const db = a[2] - b[2];
  return Math.sqrt(dr * dr + dg * dg + db * db);
}

function detectTrimBox(image) {
  const sourceCanvas = document.createElement("canvas");
  sourceCanvas.width = image.naturalWidth;
  sourceCanvas.height = image.naturalHeight;
  const sourceCtx = sourceCanvas.getContext("2d", { willReadFrequently: true });
  sourceCtx.imageSmoothingEnabled = false;
  sourceCtx.drawImage(image, 0, 0);

  const { width, height } = sourceCanvas;
  const { data } = sourceCtx.getImageData(0, 0, width, height);
  const samplePoints = [
    [0, 0],
    [width - 1, 0],
    [0, height - 1],
    [width - 1, height - 1],
    [Math.floor(width / 2), 0],
    [Math.floor(width / 2), height - 1],
  ];
  const bg = [0, 0, 0];
  for (const [x, y] of samplePoints) {
    const index = (y * width + x) * 4;
    bg[0] += data[index];
    bg[1] += data[index + 1];
    bg[2] += data[index + 2];
  }
  bg[0] /= samplePoints.length;
  bg[1] /= samplePoints.length;
  bg[2] /= samplePoints.length;

  let minX = width;
  let minY = height;
  let maxX = -1;
  let maxY = -1;
  for (let y = 0; y < height; ++y) {
    for (let x = 0; x < width; ++x) {
      const index = (y * width + x) * 4;
      const color = [data[index], data[index + 1], data[index + 2]];
      if (colorDistanceRgb(color, bg) <= TRIM_THRESHOLD) continue;
      minX = Math.min(minX, x);
      minY = Math.min(minY, y);
      maxX = Math.max(maxX, x);
      maxY = Math.max(maxY, y);
    }
  }

  if (maxX < minX || maxY < minY) {
    return { x: 0, y: 0, width, height };
  }

  const x = Math.max(0, minX - TRIM_PADDING);
  const y = Math.max(0, minY - TRIM_PADDING);
  const right = Math.min(width, maxX + TRIM_PADDING + 1);
  const bottom = Math.min(height, maxY + TRIM_PADDING + 1);
  return { x, y, width: right - x, height: bottom - y };
}

function fallbackTrimBox() {
  const bg = primaryBackground();
  return { x: 0, y: 0, width: bg?.width || state.width, height: bg?.height || state.height };
}

function preparedRoomMetrics() {
  const trim = state.baseTrim || fallbackTrimBox();
  const width = GAME_SCREEN_WIDTH;
  const scale = width / Math.max(1, trim.width);
  const height = Math.max(1, Math.round(trim.height * scale));
  const canvasHeight = Math.max(GAME_SCREEN_HEIGHT, height);
  const y = Math.max(0, Math.floor((canvasHeight - height) / 2));
  return { width, height, y, scale, trim, canvasWidth: GAME_SCREEN_WIDTH, canvasHeight };
}

function editRoomMetrics(prepared = preparedRoomMetrics()) {
  const sourcePerTarget = 1 / Math.max(0.000001, prepared.scale);
  const roomY = Math.max(0, Math.round(prepared.y * sourcePerTarget));
  const canvasHeight = Math.max(
    Math.round(prepared.canvasHeight * sourcePerTarget),
    roomY + prepared.trim.height
  );
  return {
    x: 0,
    y: roomY,
    width: Math.max(1, prepared.trim.width),
    height: Math.max(1, prepared.trim.height),
    canvasWidth: Math.max(1, prepared.trim.width),
    canvasHeight: Math.max(1, canvasHeight),
    sourcePerTarget
  };
}

function syncCanvasSizeToPreparedRoom() {
  const prepared = preparedRoomMetrics();
  const edit = editRoomMetrics(prepared);
  state.width = prepared.canvasWidth;
  state.height = prepared.canvasHeight;
  state.editWidth = edit.canvasWidth;
  state.editHeight = edit.canvasHeight;
  state.aspectRatio = state.width / Math.max(1, state.height);
  setRoomInputs(state.width, state.height);
}

function sourcePointToTargetPoint(point, prepared = preparedRoomMetrics()) {
  return {
    x: (point.x - prepared.trim.x) * prepared.scale,
    y: (point.y - prepared.trim.y) * prepared.scale + prepared.y
  };
}

function targetPointToSourcePoint(point, prepared = preparedRoomMetrics()) {
  return {
    x: (point.x / Math.max(0.000001, prepared.scale)) + prepared.trim.x,
    y: ((point.y - prepared.y) / Math.max(0.000001, prepared.scale)) + prepared.trim.y
  };
}

function coerceTrimBox(raw) {
  if (!raw) return null;
  const x = Number(raw.x);
  const y = Number(raw.y);
  const width = Number(raw.width);
  const height = Number(raw.height);
  if (![x, y, width, height].every(Number.isFinite) || width <= 0 || height <= 0) return null;
  return {
    x: Math.round(x),
    y: Math.round(y),
    width: Math.round(width),
    height: Math.round(height)
  };
}

function faceTargetPoints(face) {
  return face.points.map((point) => {
    const target = editToTargetPoint(point);
    return {
      x: Math.round(clampNumber(target.x, 0, state.width)),
      y: Math.round(clampNumber(target.y, 0, state.height))
    };
  });
}

function faceSourcePoints(face) {
  const prepared = preparedRoomMetrics();
  return face.points.map((point) => {
    const source = targetPointToSourcePoint(editToTargetPoint(point), prepared);
    return [Math.round(source.x), Math.round(source.y)];
  });
}

function screenFrameRect() {
  return {
    x: 0,
    y: 0,
    w: Math.min(GAME_SCREEN_WIDTH, state.width),
    h: Math.min(GAME_SCREEN_HEIGHT, state.height)
  };
}

function snapValue(value, grid = 8) {
  if (!state.snapToGrid) return value;
  return Math.round(value / grid) * grid;
}

function createShapePoint(x, y) {
  const point = {
    id: `p${state.nextPointId++}`,
    x: Math.round(x),
    y: Math.round(y)
  };
  state.points.push(point);
  return point;
}

function selectedPoint() {
  return state.points.find((point) => point.id === state.selectedPointId) || null;
}

function findPointMembership(point) {
  if (!point) return null;
  for (let faceIndex = state.faces.length - 1; faceIndex >= 0; --faceIndex) {
    const face = state.faces[faceIndex];
    const pointIndex = face.points.findIndex((entry) => entry.id === point.id);
    if (pointIndex >= 0) return { face, pointIndex };
  }
  return null;
}

function setSelectedPoint(point, membership = null) {
  state.selectedPointId = point ? point.id : null;
  if (membership) {
    state.selectedFaceId = membership.face.id;
    state.selectedPointIndex = membership.pointIndex;
    return;
  }
  state.selectedFaceId = null;
  state.selectedPointIndex = null;
}

function selectPointInFace(face, index) {
  if (!face || index < 0 || index >= face.points.length) {
    setSelectedPoint(null);
    return;
  }
  setSelectedPoint(face.points[index], { face, pointIndex: index });
}

function selectDraftPoint(index) {
  if (index < 0 || index >= state.draftPoints.length) {
    setSelectedPoint(null);
    return;
  }
  state.selectedFaceId = null;
  state.selectedPointIndex = index;
  state.selectedPointId = state.draftPoints[index].id;
}

function isPointReferenced(point) {
  if (!point) return false;
  if (state.draftPoints.some((entry) => entry.id === point.id)) return true;
  return state.faces.some((face) => face.points.some((entry) => entry.id === point.id));
}

function pointVisibleOnCanvas(point) {
  if (!point) return false;
  if (state.draftPoints.some((entry) => entry.id === point.id)) return true;
  if (state.drawingFace) {
    return state.faces.some((face) => faceVisible(face) && face.points.some((entry) => entry.id === point.id));
  }
  const face = selectedFace();
  return !!face && faceVisible(face) && face.points.some((entry) => entry.id === point.id);
}

function pruneUnusedPoints() {
  state.points = state.points.filter((point) => isPointReferenced(point));
  if (state.selectedPointId && !state.points.some((point) => point.id === state.selectedPointId)) {
    state.selectedPointId = null;
    state.selectedPointIndex = null;
  }
}

function addDraftPoint(point) {
  if (!point) return;
  const last = state.draftPoints[state.draftPoints.length - 1];
  if (last && last.id === point.id) return;
  state.draftPoints.push(point);
  selectDraftPoint(state.draftPoints.length - 1);
}

function targetRectToEdit(x, y, w, h) {
  const p = targetToEditPoint({ x, y });
  return {
    x: p.x,
    y: p.y,
    w: w * editScaleX(),
    h: h * editScaleY()
  };
}

function editHandleRadius() {
  return Math.max(5, Math.min(14, Math.max(editScaleX(), editScaleY()) * 3.5));
}

function setRoomInputs(width, height) {
  document.getElementById("canvasW").value = Math.round(width);
  document.getElementById("canvasH").value = Math.round(height);
  updateCanvasSizeReadouts();
}

function applyRoomSizeInput(changedField) {
  syncCanvasSizeToPreparedRoom();
  updateBaseImageMeta();
  render();
  commitHistory();
}

function autoSourceScaleForImage(width, height) {
  const scaleX = width / Math.max(1, state.width);
  const scaleY = height / Math.max(1, state.height);
  if (scaleX <= 1 && scaleY <= 1) return 1;
  const closeAspect = Math.abs(scaleX - scaleY) / Math.max(scaleX, scaleY) < 0.08;
  const scale = closeAspect ? (scaleX + scaleY) / 2 : Math.min(scaleX, scaleY);
  return Math.max(1, Math.round(scale * 100) / 100);
}

function furnitureInitialScale(width, height) {
  if (state.furnitureImportMode === "native") return 1;
  if (state.furnitureImportMode === "fit_canvas") {
    return Math.min(1, state.width / Math.max(1, width), state.height / Math.max(1, height));
  }
  return 1 / Math.max(0.01, state.sourceScale);
}

function readImageFile(file) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onerror = () => reject(reader.error);
    reader.onload = () => {
      const img = new Image();
      img.onload = () => resolve({ file, img, dataUrl: reader.result });
      img.onerror = reject;
      img.src = reader.result;
    };
    reader.readAsDataURL(file);
  });
}

function loadImageSource(src) {
  return new Promise((resolve, reject) => {
    const img = new Image();
    img.onload = () => resolve(img);
    img.onerror = reject;
    img.src = src;
  });
}

function blobToDataUrl(blob) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onerror = () => reject(reader.error);
    reader.onload = () => resolve(reader.result);
    reader.readAsDataURL(blob);
  });
}

async function imageSourceToDataUrl(src) {
  if (!src || /^data:/i.test(src)) return src;
  const response = await fetch(src);
  if (!response.ok) throw new Error(`Image fetch failed: ${src}`);
  return blobToDataUrl(await response.blob());
}

async function loadExportableImageSource(src) {
  try {
    const dataUrl = await imageSourceToDataUrl(src);
    return {
      img: await loadImageSource(dataUrl),
      dataUrl,
      exportable: true
    };
  } catch (_) {
    const img = await loadImageSource(src);
    return {
      img,
      dataUrl: src,
      exportable: /^data:/i.test(src)
    };
  }
}

function drawImagePreview(canvasEl, image) {
  const maxW = 300;
  const maxH = 260;
  const sourceWidth = image.naturalWidth || image.width;
  const sourceHeight = image.naturalHeight || image.height;
  const scale = Math.min(1, maxW / Math.max(1, sourceWidth), maxH / Math.max(1, sourceHeight));
  canvasEl.width = Math.max(1, Math.round(sourceWidth * scale));
  canvasEl.height = Math.max(1, Math.round(sourceHeight * scale));
  const previewCtx = canvasEl.getContext("2d");
  previewCtx.clearRect(0, 0, canvasEl.width, canvasEl.height);
  previewCtx.imageSmoothingEnabled = false;
  previewCtx.drawImage(image, 0, 0, canvasEl.width, canvasEl.height);
}

function averageCornerColor(data, width, height) {
  const size = Math.min(8, width, height);
  const corners = [
    [0, 0],
    [width - size, 0],
    [0, height - size],
    [width - size, height - size]
  ];
  const sum = [0, 0, 0];
  let count = 0;
  for (const [startX, startY] of corners) {
    for (let y = startY; y < startY + size; ++y) {
      for (let x = startX; x < startX + size; ++x) {
        const index = (y * width + x) * 4;
        if (data[index + 3] < 8) continue;
        sum[0] += data[index];
        sum[1] += data[index + 1];
        sum[2] += data[index + 2];
        count += 1;
      }
    }
  }
  if (!count) return [0, 255, 0];
  return [sum[0] / count, sum[1] / count, sum[2] / count];
}

function clampByte(value) {
  return Math.max(0, Math.min(255, Math.round(value)));
}

function rgbToHex(color) {
  return "#" + color.map((value) => clampByte(value).toString(16).padStart(2, "0")).join("");
}

function hexToRgb(value) {
  const hex = String(value || "").replace("#", "");
  if (!/^[0-9a-fA-F]{6}$/.test(hex)) return [3, 249, 3];
  return [
    parseInt(hex.slice(0, 2), 16),
    parseInt(hex.slice(2, 4), 16),
    parseInt(hex.slice(4, 6), 16)
  ];
}

function sourceSizeOf(image) {
  return {
    width: image.naturalWidth || image.width,
    height: image.naturalHeight || image.height
  };
}

function previewPointerToSource(previewCanvas, event, sourceWidth, sourceHeight) {
  const rect = previewCanvas.getBoundingClientRect();
  const previewX = (event.clientX - rect.left) * previewCanvas.width / Math.max(1, rect.width);
  const previewY = (event.clientY - rect.top) * previewCanvas.height / Math.max(1, rect.height);
  return {
    x: Math.max(0, Math.min(sourceWidth - 1, Math.floor(previewX * sourceWidth / Math.max(1, previewCanvas.width)))),
    y: Math.max(0, Math.min(sourceHeight - 1, Math.floor(previewY * sourceHeight / Math.max(1, previewCanvas.height))))
  };
}

function sampleImageColor(image, previewCanvas, event) {
  const { width, height } = sourceSizeOf(image);
  const sourcePoint = previewPointerToSource(previewCanvas, event, width, height);

  const sampleCanvas = document.createElement("canvas");
  sampleCanvas.width = width;
  sampleCanvas.height = height;
  const sampleCtx = sampleCanvas.getContext("2d", { willReadFrequently: true });
  sampleCtx.imageSmoothingEnabled = false;
  sampleCtx.drawImage(image, 0, 0);
  const pixel = sampleCtx.getImageData(sourcePoint.x, sourcePoint.y, 1, 1).data;
  return [pixel[0], pixel[1], pixel[2]];
}

function cornerKeyColor(image) {
  const { width, height } = sourceSizeOf(image);
  const sourceCanvas = document.createElement("canvas");
  sourceCanvas.width = width;
  sourceCanvas.height = height;
  const sourceCtx = sourceCanvas.getContext("2d", { willReadFrequently: true });
  sourceCtx.imageSmoothingEnabled = false;
  sourceCtx.drawImage(image, 0, 0);
  const { data } = sourceCtx.getImageData(0, 0, sourceCanvas.width, sourceCanvas.height);
  return averageCornerColor(data, sourceCanvas.width, sourceCanvas.height);
}

function cloneImageData(imageData) {
  return new ImageData(new Uint8ClampedArray(imageData.data), imageData.width, imageData.height);
}

function cutoutImageData(imageData, keyColor, tolerance, feather) {
  const output = cloneImageData(imageData);
  const { data } = output;
  const removeRange = Math.max(0, Number(tolerance) || 0);
  const featherRange = Math.max(0, Number(feather) || 0);
  const softEnd = removeRange + featherRange;

  for (let i = 0; i < data.length; i += 4) {
    const r = data[i];
    const g = data[i + 1];
    const b = data[i + 2];
    const a = data[i + 3];
    if (a === 0) continue;

    const distanceToKey = colorDistanceRgb([r, g, b], keyColor);
    if (distanceToKey <= removeRange) {
      data[i + 3] = 0;
      continue;
    }

    if (featherRange > 0 && distanceToKey <= softEnd) {
      const alphaRatio = (distanceToKey - removeRange) / featherRange;
      data[i + 3] = Math.min(data[i + 3], Math.round(a * alphaRatio));
    }
  }

  return output;
}

function pixelColorFromImageData(imageData, x, y) {
  const index = (y * imageData.width + x) * 4;
  const data = imageData.data;
  return [data[index], data[index + 1], data[index + 2], data[index + 3]];
}

function eraseConnectedRegion(imageData, startX, startY, tolerance) {
  const width = imageData.width;
  const height = imageData.height;
  const data = imageData.data;
  const startIndex = startY * width + startX;
  const startOffset = startIndex * 4;
  if (data[startOffset + 3] === 0) return { removed: 0, color: [data[startOffset], data[startOffset + 1], data[startOffset + 2]] };

  const seedColor = [data[startOffset], data[startOffset + 1], data[startOffset + 2]];
  const removeRange = Math.max(0, Number(tolerance) || 0);
  const visited = new Uint8Array(width * height);
  const stack = [startIndex];
  let removed = 0;

  while (stack.length) {
    const index = stack.pop();
    if (visited[index]) continue;
    visited[index] = 1;
    const offset = index * 4;
    if (data[offset + 3] === 0) continue;

    const color = [data[offset], data[offset + 1], data[offset + 2]];
    if (colorDistanceRgb(color, seedColor) > removeRange) continue;

    data[offset + 3] = 0;
    removed += 1;

    const x = index % width;
    const y = Math.floor(index / width);
    if (x > 0) stack.push(index - 1);
    if (x < width - 1) stack.push(index + 1);
    if (y > 0) stack.push(index - width);
    if (y < height - 1) stack.push(index + width);
  }

  return { removed, color: seedColor };
}

function applyBrushToImageData(imageData, originalImageData, centerX, centerY, size, mode) {
  const width = imageData.width;
  const height = imageData.height;
  const data = imageData.data;
  const original = originalImageData.data;
  const brushSize = Math.max(1, Math.round(Number(size) || 1));
  const minX = Math.max(0, centerX - Math.floor((brushSize - 1) / 2));
  const minY = Math.max(0, centerY - Math.floor((brushSize - 1) / 2));
  const maxX = Math.min(width - 1, minX + brushSize - 1);
  const maxY = Math.min(height - 1, minY + brushSize - 1);
  let changed = 0;

  for (let y = minY; y <= maxY; ++y) {
    for (let x = minX; x <= maxX; ++x) {
      const offset = (y * width + x) * 4;
      if (mode === "restore") {
        const differs =
          data[offset] !== original[offset] ||
          data[offset + 1] !== original[offset + 1] ||
          data[offset + 2] !== original[offset + 2] ||
          data[offset + 3] !== original[offset + 3];
        if (!differs) continue;
        data[offset] = original[offset];
        data[offset + 1] = original[offset + 1];
        data[offset + 2] = original[offset + 2];
        data[offset + 3] = original[offset + 3];
      } else {
        if (data[offset + 3] === 0) continue;
        data[offset + 3] = 0;
      }
      changed += 1;
    }
  }

  return changed;
}

function promptFurnitureCutout(loaded) {
  return new Promise((resolve) => {
    const modal = document.getElementById("cutoutModal");
    const beforeCanvas = document.getElementById("cutoutBefore");
    const afterCanvas = document.getElementById("cutoutAfter");
    const fileName = document.getElementById("cutoutFileName");
    const hint = document.getElementById("cutoutHint");
    const cancel = document.getElementById("cutoutCancel");
    const useOriginal = document.getElementById("cutoutUseOriginal");
    const runCutout = document.getElementById("cutoutRun");
    const useResult = document.getElementById("cutoutUseResult");
    const resetEdit = document.getElementById("cutoutReset");
    const toolInput = document.getElementById("cutoutTool");
    const colorInput = document.getElementById("cutoutColor");
    const toleranceInput = document.getElementById("cutoutTolerance");
    const toleranceValue = document.getElementById("cutoutToleranceValue");
    const featherInput = document.getElementById("cutoutFeather");
    const featherValue = document.getElementById("cutoutFeatherValue");
    const brushInput = document.getElementById("cutoutBrushSize");
    const brushValue = document.getElementById("cutoutBrushSizeValue");

    const width = loaded.img.naturalWidth;
    const height = loaded.img.naturalHeight;
    const originalCanvas = document.createElement("canvas");
    originalCanvas.width = width;
    originalCanvas.height = height;
    const originalCtx = originalCanvas.getContext("2d", { willReadFrequently: true });
    originalCtx.imageSmoothingEnabled = false;
    originalCtx.drawImage(loaded.img, 0, 0);
    const originalImageData = originalCtx.getImageData(0, 0, width, height);

    const workCanvas = document.createElement("canvas");
    workCanvas.width = width;
    workCanvas.height = height;
    const workCtx = workCanvas.getContext("2d", { willReadFrequently: true });
    workCtx.imageSmoothingEnabled = false;
    let workImageData = cloneImageData(originalImageData);
    let hasEdits = false;
    let isPainting = false;
    let settled = false;

    const updateRangeLabels = () => {
      toleranceValue.textContent = toleranceInput.value;
      featherValue.textContent = featherInput.value;
      brushValue.textContent = brushInput.value;
    };
    const toolDescription = () => {
      if (toolInput.value === "pick") return "Click either image to pick a color.";
      if (toolInput.value === "wand") return "Click the after image to remove one connected color region.";
      if (toolInput.value === "restore") return "Drag on the after image to restore pixels from the original.";
      return "Drag on the after image to erase pixels manually.";
    };
    const updateToolClass = () => {
      afterCanvas.classList.remove("pick-tool", "wand-tool", "brush-tool");
      beforeCanvas.classList.remove("pick-tool", "wand-tool", "brush-tool");
      beforeCanvas.classList.add("pick-tool");
      if (toolInput.value === "pick") afterCanvas.classList.add("pick-tool");
      if (toolInput.value === "wand") afterCanvas.classList.add("wand-tool");
      if (toolInput.value === "erase" || toolInput.value === "restore") afterCanvas.classList.add("brush-tool");
    };
    const updateToolHint = () => {
      hint.textContent = `${toolDescription()} Cutout applies to the current after image, so repeated cutouts combine.`;
    };
    const renderWorkPreview = () => {
      workCtx.putImageData(workImageData, 0, 0);
      drawImagePreview(afterCanvas, workCanvas);
      updateToolClass();
    };
    const markEdited = (message) => {
      hasEdits = true;
      useResult.disabled = false;
      if (message) hint.textContent = message;
    };
    const setWorkImageData = (imageData, edited, message) => {
      workImageData = imageData;
      renderWorkPreview();
      if (edited) {
        markEdited(message);
      } else {
        useResult.disabled = !hasEdits;
      }
    };
    const resultFromWorkCanvas = async () => {
      workCtx.putImageData(workImageData, 0, 0);
      const dataUrl = workCanvas.toDataURL("image/png");
      return {
        img: await loadImageSource(dataUrl),
        dataUrl,
        cutoutApplied: hasEdits
      };
    };
    const applyWand = (event) => {
      const point = previewPointerToSource(afterCanvas, event, width, height);
      const removed = eraseConnectedRegion(workImageData, point.x, point.y, Number(toleranceInput.value));
      colorInput.value = rgbToHex(removed.color);
      if (!removed.removed) {
        renderWorkPreview();
        hint.textContent = "Magic wand found no opaque pixels at that point.";
        return;
      }
      setWorkImageData(workImageData, true, `Magic wand removed ${removed.removed} connected pixel(s).`);
    };
    const applyBrush = (event) => {
      const point = previewPointerToSource(afterCanvas, event, width, height);
      const changed = applyBrushToImageData(
        workImageData,
        originalImageData,
        point.x,
        point.y,
        Number(brushInput.value),
        toolInput.value
      );
      if (!changed) return;
      setWorkImageData(
        workImageData,
        true,
        `${toolInput.value === "restore" ? "Restored" : "Erased"} ${changed} pixel(s).`
      );
    };
    const cleanup = () => {
      cancel.onclick = null;
      useOriginal.onclick = null;
      runCutout.onclick = null;
      useResult.onclick = null;
      resetEdit.onclick = null;
      beforeCanvas.onclick = null;
      afterCanvas.onclick = null;
      afterCanvas.onpointerdown = null;
      afterCanvas.onpointermove = null;
      afterCanvas.onpointerup = null;
      afterCanvas.onpointercancel = null;
      afterCanvas.onpointerleave = null;
      toolInput.onchange = null;
      colorInput.oninput = null;
      toleranceInput.oninput = null;
      featherInput.oninput = null;
      brushInput.oninput = null;
      beforeCanvas.classList.remove("pick-tool", "wand-tool", "brush-tool");
      afterCanvas.classList.remove("pick-tool", "wand-tool", "brush-tool");
      modal.hidden = true;
    };
    const settle = (value) => {
      if (settled) return;
      settled = true;
      cleanup();
      resolve(value);
    };

    fileName.textContent = `${loaded.file.name} (${loaded.img.naturalWidth}x${loaded.img.naturalHeight})`;
    colorInput.value = rgbToHex(cornerKeyColor(loaded.img));
    toolInput.value = "pick";
    toleranceInput.value = "28";
    featherInput.value = "8";
    brushInput.value = "1";
    updateRangeLabels();
    updateToolHint();
    useResult.disabled = true;
    drawImagePreview(beforeCanvas, loaded.img);
    renderWorkPreview();
    modal.hidden = false;

    beforeCanvas.onclick = (event) => {
      colorInput.value = rgbToHex(sampleImageColor(loaded.img, beforeCanvas, event));
      updateToolHint();
    };
    afterCanvas.onclick = (event) => {
      if (toolInput.value === "pick") {
        const point = previewPointerToSource(afterCanvas, event, width, height);
        colorInput.value = rgbToHex(pixelColorFromImageData(workImageData, point.x, point.y).slice(0, 3));
        updateToolHint();
      } else if (toolInput.value === "wand") {
        applyWand(event);
      }
    };
    afterCanvas.onpointerdown = (event) => {
      if (toolInput.value !== "erase" && toolInput.value !== "restore") return;
      event.preventDefault();
      isPainting = true;
      afterCanvas.setPointerCapture(event.pointerId);
      applyBrush(event);
    };
    afterCanvas.onpointermove = (event) => {
      if (!isPainting) return;
      applyBrush(event);
    };
    afterCanvas.onpointerup = (event) => {
      isPainting = false;
      if (afterCanvas.hasPointerCapture(event.pointerId)) {
        afterCanvas.releasePointerCapture(event.pointerId);
      }
    };
    afterCanvas.onpointercancel = () => {
      isPainting = false;
    };
    afterCanvas.onpointerleave = () => {
      isPainting = false;
    };
    toolInput.onchange = () => {
      updateToolClass();
      updateToolHint();
    };
    colorInput.oninput = updateToolHint;
    toleranceInput.oninput = () => {
      updateRangeLabels();
      updateToolHint();
    };
    featherInput.oninput = () => {
      updateRangeLabels();
      updateToolHint();
    };
    brushInput.oninput = () => {
      updateRangeLabels();
      updateToolHint();
    };

    cancel.onclick = () => settle(null);

    useOriginal.onclick = () => settle({
      img: loaded.img,
      dataUrl: loaded.dataUrl,
      cutoutApplied: false
    });

    resetEdit.onclick = () => {
      hasEdits = false;
      setWorkImageData(cloneImageData(originalImageData), false, null);
      useResult.disabled = true;
      updateToolHint();
    };

    runCutout.onclick = async () => {
      runCutout.disabled = true;
      hint.textContent = "Cutout running...";
      try {
        const nextImageData = cutoutImageData(
          workImageData,
          hexToRgb(colorInput.value),
          Number(toleranceInput.value),
          Number(featherInput.value)
        );
        setWorkImageData(nextImageData, true, "Cutout applied to the current after image. Pick another edge color and run cutout again to combine cleanup.");
      } catch (error) {
        console.error(error);
        hint.textContent = "Cutout failed. Use original or try another image.";
      } finally {
        runCutout.disabled = false;
      }
    };

    useResult.onclick = async () => {
      if (!hasEdits) return;
      useResult.disabled = true;
      hint.textContent = "Preparing edited PNG...";
      try {
        settle(await resultFromWorkCanvas());
      } catch (error) {
        console.error(error);
        hint.textContent = "Export failed. Use original or try again.";
        useResult.disabled = false;
      }
    };
  });
}

function projectSpriteCatalog() {
  return Array.isArray(window.STICKMON_PROJECT_SPRITES) ? window.STICKMON_PROJECT_SPRITES : [];
}

function selectedProjectSpriteEntry() {
  const select = document.getElementById("spriteSpeciesSelect");
  const speciesId = Number(select.value) || 0;
  return projectSpriteCatalog().find((entry) => entry.speciesId === speciesId) || null;
}

function selectedProjectSpriteFrame() {
  const entry = selectedProjectSpriteEntry();
  if (!entry) return null;
  const action = document.getElementById("spriteActionSelect").value;
  const framePath = document.getElementById("spriteFrameSelect").value;
  const frames = entry.actions[action] || [];
  return frames.find((frame) => frame.path === framePath) || frames[0] || null;
}

function projectSpriteFrameSource(frame) {
  return frame?.dataUrl || frame?.path || "";
}

function projectSpriteFrameForItem(item) {
  if (!item || item.source !== "project_sprite") return null;
  const entry = projectSpriteCatalog().find((candidate) => candidate.speciesId === item.speciesId);
  if (!entry?.actions) return null;
  const frames = entry.actions[item.action] || Object.values(entry.actions).flat();
  return frames.find((frame) =>
    frame.kindName === item.frame ||
    frame.file === item.fileName ||
    frame.path === item.dataUrl ||
    frame.dataUrl === item.dataUrl
  ) || null;
}

async function hydrateProjectSpriteImage(item) {
  const frame = projectSpriteFrameForItem(item);
  const source = projectSpriteFrameSource(frame);
  if (!source) return false;
  const loaded = await loadExportableImageSource(source);
  item.img = loaded.img;
  item.dataUrl = loaded.dataUrl;
  item.exportableImage = loaded.exportable;
  item.sourceWidth = item.sourceWidth || frame.width || loaded.img.naturalWidth;
  item.sourceHeight = item.sourceHeight || frame.height || loaded.img.naturalHeight;
  return loaded.exportable;
}

function slugifyId(value, fallback = "item") {
  const slug = String(value || fallback)
    .replace(/\.[^.]+$/, "")
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "_")
    .replace(/^_+|_+$/g, "");
  return slug || fallback;
}

function rawFurnitureLibraryItems() {
  const library = window.STICKMON_FURNITURE_LIBRARY;
  if (Array.isArray(library)) return library;
  if (Array.isArray(library?.items)) return library.items;
  return [];
}

function furnitureLibraryCategory(entry) {
  return entry.category || entry.furnitureType || "decoration";
}

function normalizeFurnitureLibraryEntry(raw, index = 0) {
  if (!raw || typeof raw !== "object") return null;
  const imageValue = typeof raw.image === "string" ? raw.image : raw.image?.path || raw.imagePath || raw.path || "";
  const dataUrl = raw.dataUrl || raw.imageDataUrl || raw.image?.dataUrl || "";
  const fileName = raw.fileName || raw.imageFileName || imageValue.split("/").pop() || `${raw.id || `library_${index}`}.png`;
  const furnitureType = normalizeFurnitureType(raw.furnitureType || raw.category || inferFurnitureType(fileName || raw.name));
  const kind = normalizeItemKindForRecord(
    raw.kind || inferItemKind(fileName || raw.name, furnitureType),
    raw
  );
  const id = raw.id || slugifyId(fileName || raw.name || `library_${index}`, `library_${index}`);
  return {
    ...raw,
    id,
    name: raw.name || fileName.replace(/\.[^.]+$/, "") || id,
    fileName,
    image: imageValue,
    dataUrl,
    category: furnitureLibraryCategory({ ...raw, furnitureType }),
    furnitureType,
    kind,
    sourceWidth: Number(raw.sourceWidth) || Number(raw.width) || 0,
    sourceHeight: Number(raw.sourceHeight) || Number(raw.height) || 0,
    defaultScale: Number(raw.defaultScale ?? raw.scale ?? 1) || 1,
    defaultScaleX: Number(raw.defaultScaleX ?? raw.scaleX ?? raw.defaultScale ?? raw.scale ?? 1) || 1,
    defaultScaleY: Number(raw.defaultScaleY ?? raw.scaleY ?? raw.defaultScale ?? raw.scale ?? 1) || 1,
    targetWidth: Number(raw.targetWidth) || 0,
    targetHeight: Number(raw.targetHeight) || 0,
    heightPx: Number(raw.heightPx) || 0,
    footprint: normalizeFootprint(raw.footprint),
    footprintPolygon: normalizeLocalPolygon(raw.footprintPolygon),
    shadowPolygon: normalizeLocalPolygon(raw.shadowPolygon),
    shadowAnchor: normalizeShadowAnchor(raw.shadowAnchor),
    wallShadowDepthPx: Number(raw.wallShadowDepthPx) || 0,
    wallShadowOffsetY: Number(raw.wallShadowOffsetY) || 0,
    castsShadow: raw.castsShadow !== false,
    shadowOpacity: coerceNumber(raw.shadowOpacity ?? raw.shadowAlpha, state.night.shadowAlpha, 0, 100),
    shadowLength: coerceNumber(raw.shadowLength, state.night.shadowLength, 0, 120),
    shadowBlur: coerceNumber(raw.shadowBlur, state.night.shadowBlur, 0, 16),
    receivesShadow: raw.receivesShadow !== false,
    occludesSprite: raw.occludesSprite === true,
    visibleInDay: raw.visibleInDay !== false,
    visibleInNight: raw.visibleInNight !== false,
    tags: Array.isArray(raw.tags) ? raw.tags : []
  };
}

function furnitureLibraryCatalog() {
  return rawFurnitureLibraryItems()
    .map(normalizeFurnitureLibraryEntry)
    .filter(Boolean);
}

function furnitureLibraryEntryById(id) {
  return furnitureLibraryCatalog().find((entry) => entry.id === id) || null;
}

function normalizeFurnitureAssetLookupKey(value) {
  return String(value || "")
    .split(/[\\/]/)
    .pop()
    .replace(/\.[^.]+$/, "")
    .toLowerCase()
    .replace(/forniture/g, "furniture")
    .replace(/[^a-z0-9]+/g, "_")
    .replace(/^_+|_+$/g, "");
}

function furnitureAssetLookupKeys(value) {
  const key = normalizeFurnitureAssetLookupKey(value);
  if (!key) return [];
  const keys = [key];
  const stripped = key.replace(/^(furniture_)+/, "furniture_");
  if (stripped && stripped !== key) keys.push(stripped);
  const withoutPrefix = stripped.replace(/^furniture_/, "");
  if (withoutPrefix && withoutPrefix !== stripped) keys.push(withoutPrefix);
  return [...new Set(keys)];
}

function furnitureLibraryEntryByItemReference(item) {
  const referenceKeys = [
    item?.libraryId,
    item?.fileName,
    item?.name,
    item?.id
  ].flatMap(furnitureAssetLookupKeys);
  if (!referenceKeys.length) return null;

  return furnitureLibraryCatalog().find((entry) => {
    const entryKeys = [
      entry.id,
      entry.fileName,
      entry.name,
      entry.image
    ].flatMap(furnitureAssetLookupKeys);
    return referenceKeys.some((referenceKey) =>
      entryKeys.some((entryKey) =>
        entryKey === referenceKey ||
        entryKey.endsWith(`_${referenceKey}`) ||
        referenceKey.endsWith(`_${entryKey}`)
      )
    );
  }) || null;
}

function furnitureLibraryMatches(entry, search, category) {
  if (category && furnitureLibraryCategory(entry) !== category) return false;
  if (!search) return true;
  const haystack = [
    entry.id,
    entry.name,
    entry.fileName,
    entry.category,
    entry.furnitureType,
    entry.kind,
    ...(entry.tags || [])
  ].join(" ").toLowerCase();
  return haystack.includes(search.toLowerCase());
}

function refreshFurnitureLibraryPanel() {
  if (!furnitureLibraryList) return;
  const catalog = furnitureLibraryCatalog();
  const search = librarySearchInput?.value.trim() || "";
  const category = libraryCategoryInput?.value || "";
  const categories = [...new Set(catalog.map(furnitureLibraryCategory).filter(Boolean))].sort();
  if (libraryCategoryInput) {
    const previous = libraryCategoryInput.value;
    libraryCategoryInput.innerHTML = '<option value="">All</option>' +
      categories.map((value) => `<option value="${escapeAttr(value)}">${escapeHtml(furnitureTypeLabel(value))}</option>`).join("");
    libraryCategoryInput.value = categories.includes(previous) ? previous : "";
  }
  const entries = catalog.filter((entry) => furnitureLibraryMatches(entry, search, category));
  furnitureLibraryList.innerHTML = "";
  for (const entry of entries) {
    const el = document.createElement("div");
    el.className = "library-item";
    const imageSrc = entry.dataUrl || entry.image || "";
    const polygonCount = entry.footprintPolygon.length + entry.shadowPolygon.length;
    const tags = [furnitureTypeLabel(entry.furnitureType), itemKindLabel(entry.kind), ...(entry.tags || [])]
      .filter(Boolean)
      .slice(0, 3)
      .join(" / ");
    el.innerHTML = `
      ${imageSrc ? `<img class="library-thumb" alt="" src="${escapeAttr(imageSrc)}">` : '<div class="library-thumb"></div>'}
      <div class="library-details">
        <div class="library-name">${escapeHtml(entry.name)}</div>
        <div class="library-meta">${escapeHtml(tags || entry.id)}</div>
        <div class="library-meta">${Math.round(entry.sourceWidth || 0)}x${Math.round(entry.sourceHeight || 0)}, ${polygonCount ? `${polygonCount} polygon pts` : "semantic only"}</div>
      </div>
      <div class="library-actions">
        <button class="library-add" type="button" data-library-id="${escapeAttr(entry.id)}">Add</button>
        <button class="library-delete danger" type="button" data-library-id="${escapeAttr(entry.id)}" title="Permanently delete from library">Delete</button>
      </div>
    `;
    el.querySelector(".library-add").addEventListener("click", async (event) => {
      event.preventDefault();
      event.stopPropagation();
      await addFurnitureLibraryEntry(entry);
    });
    el.querySelector(".library-delete").addEventListener("click", async (event) => {
      event.preventDefault();
      event.stopPropagation();
      try {
        await deleteFurnitureLibraryEntry(entry);
      } catch (error) {
        console.error(error);
        setStatus(isFilePickerAbort(error) ? "Room editor folder was not selected." : (error.message || "Deleting furniture library item failed."));
      }
    });
    furnitureLibraryList.appendChild(el);
  }
  if (!entries.length) {
    const empty = document.createElement("div");
    empty.className = "hint";
    empty.textContent = catalog.length ? "No matching furniture." : "No furniture library loaded. Select annotated furniture and click Add selected.";
    furnitureLibraryList.appendChild(empty);
  }
  if (furnitureLibraryMeta) {
    furnitureLibraryMeta.textContent = catalog.length
      ? `${entries.length}/${catalog.length} library item(s).`
      : "No furniture library loaded.";
  }
}

function initFurnitureLibraryControls() {
  librarySearchInput?.addEventListener("input", refreshFurnitureLibraryPanel);
  libraryCategoryInput?.addEventListener("change", refreshFurnitureLibraryPanel);
  refreshFurnitureLibraryPanel();
}

async function createFurnitureItemFromLibraryEntry(entry, placement = {}) {
  const source = entry.dataUrl || entry.image;
  if (!source) throw new Error(`Library item is missing image data: ${entry.name || entry.id}`);
  const loaded = await loadExportableImageSource(source);
  const sourceWidth = entry.sourceWidth || loaded.img.naturalWidth;
  const sourceHeight = entry.sourceHeight || loaded.img.naturalHeight;
  const scaleX = entry.targetWidth ? entry.targetWidth / Math.max(1, sourceWidth) : entry.defaultScaleX;
  const scaleY = entry.targetHeight ? entry.targetHeight / Math.max(1, sourceHeight) : entry.defaultScaleY;
  const targetW = sourceWidth * scaleX;
  const targetH = sourceHeight * scaleY;
  const x = placement.x ?? Math.round((state.width - Math.min(targetW, state.width)) / 2);
  const y = placement.y ?? Math.round((state.height - Math.min(targetH, state.height)) / 2);
  const item = {
    id: placement.id || `f${state.nextId++}`,
    name: placement.name || entry.name,
    fileName: entry.fileName,
    img: loaded.img,
    dataUrl: loaded.dataUrl,
    originalDataUrl: loaded.dataUrl,
    sourceWidth,
    sourceHeight,
    x,
    y,
    scale: placement.scale ?? scaleX,
    scaleX: placement.scaleX ?? scaleX,
    scaleY: placement.scaleY ?? scaleY,
    aspectLocked: placement.aspectLocked ?? Math.abs(scaleX - scaleY) < 0.0001,
    sourceScale: placement.sourceScale ?? entry.sourceScale ?? state.sourceScale,
    opacity: placement.opacity ?? 1,
    z: placement.z ?? 20,
    visible: placement.visible ?? true,
    visibleInDay: placement.visibleInDay ?? entry.visibleInDay ?? true,
    visibleInNight: placement.visibleInNight ?? entry.visibleInNight ?? true,
    anchor: placement.anchor || "top_left",
    slot: placement.slot || entry.slot || itemKindPreset(entry.kind).slot,
    layer: placement.layer || entry.layer || itemKindPreset(entry.kind).layer,
    source: "furniture",
    furnitureType: normalizeFurnitureType(placement.furnitureType || entry.furnitureType),
    kind: normalizeItemKind(placement.kind || entry.kind),
    heightPx: placement.heightPx ?? entry.heightPx,
    footprint: normalizeFootprint(placement.footprint || entry.footprint),
    footprintPolygon: normalizeLocalPolygon(placement.footprintPolygon ?? entry.footprintPolygon),
    shadowPolygon: normalizeLocalPolygon(placement.shadowPolygon ?? entry.shadowPolygon),
    shadowAnchor: normalizeShadowAnchor(placement.shadowAnchor || entry.shadowAnchor),
    shadowFaceIds: normalizeShadowFaceIds(placement.shadowFaceIds),
    wallShadowDepthPx: placement.wallShadowDepthPx ?? entry.wallShadowDepthPx,
    wallShadowOffsetY: placement.wallShadowOffsetY ?? entry.wallShadowOffsetY,
    castsShadow: placement.castsShadow ?? entry.castsShadow,
    shadowOpacity: placement.shadowOpacity ?? placement.shadowAlpha ?? entry.shadowOpacity,
    shadowLength: placement.shadowLength ?? entry.shadowLength,
    shadowBlur: placement.shadowBlur ?? entry.shadowBlur,
    receivesShadow: placement.receivesShadow ?? entry.receivesShadow,
    occludesSprite: placement.occludesSprite ?? entry.occludesSprite,
    sortY: placement.sortY ?? Math.round(y + targetH),
    cutoutApplied: placement.cutoutApplied ?? true,
    libraryId: entry.id,
    libraryRevision: entry.revision || 1,
    libraryCategory: furnitureLibraryCategory(entry)
  };
  normalizeItemSemantics(item);
  return item;
}

async function addFurnitureLibraryEntry(entry) {
  try {
    const item = await createFurnitureItemFromLibraryEntry(entry);
    state.items.push(item);
    state.selectedId = item.id;
    state.selectedFaceId = null;
    state.selectedPointIndex = null;
    state.selectedPointId = null;
    state.editMode = "furniture";
    updateEditModeButtons();
    setStatus(`Added library furniture: ${entry.name}.`);
    refreshList();
    refreshSelectedPanel();
    render();
    commitHistory();
  } catch (error) {
    console.error(error);
    setStatus(error.message || "Failed to add library furniture.");
  }
}

function isAnnotatedFurniture(item) {
  if (!item || item.source !== "furniture" || !item.dataUrl) return false;
  const preset = itemKindPreset(item.kind);
  return normalizeLocalPolygon(item.footprintPolygon).length > 0 ||
    normalizeLocalPolygon(item.shadowPolygon).length > 0 ||
    normalizeFootprint(item.footprint) === "polygon" ||
    normalizeShadowAnchor(item.shadowAnchor) !== "auto" ||
    Math.round(itemHeightPx(item)) !== Math.round(preset.heightPx) ||
    Math.round(itemWallShadowDepthPx(item)) !== Math.round(preset.wallDepthPx ?? 0) ||
    Math.round(itemWallShadowOffsetY(item)) !== 0 ||
    item.castsShadow !== preset.castsShadow ||
    Math.round(itemShadowOpacity(item)) !== Math.round(state.night.shadowAlpha) ||
    Math.round(itemShadowLength(item)) !== Math.round(state.night.shadowLength) ||
    Number(itemShadowBlur(item).toFixed(1)) !== Number(state.night.shadowBlur.toFixed(1)) ||
    item.receivesShadow === false ||
    item.occludesSprite !== preset.occludesSprite;
}

function furnitureLibraryIdForItem(item) {
  return item.libraryId || `furniture_${slugifyId(item.fileName || item.name || item.id)}`;
}

function serializeFurnitureLibraryItem(item, idOverride = "") {
  const bounds = itemBounds(item);
  const id = idOverride || furnitureLibraryIdForItem(item);
  const furnitureType = normalizeFurnitureType(item.furnitureType);
  const kind = normalizeItemKind(item.kind);
  return {
    id,
    name: item.name || id,
    category: furnitureType,
    tags: [...new Set([furnitureType, kind, itemKindPreset(kind).slot, itemKindPreset(kind).layer].filter(Boolean))],
    fileName: item.fileName || `${id}.png`,
    sourceWidth: item.sourceWidth || item.img?.naturalWidth || Math.round(bounds.w),
    sourceHeight: item.sourceHeight || item.img?.naturalHeight || Math.round(bounds.h),
    dataUrl: item.dataUrl,
    defaultScale: Number(itemScaleX(item).toFixed(4)),
    defaultScaleX: Number(itemScaleX(item).toFixed(4)),
    defaultScaleY: Number(itemScaleY(item).toFixed(4)),
    targetWidth: Math.round(bounds.w),
    targetHeight: Math.round(bounds.h),
    sourceScale: Number((item.sourceScale || state.sourceScale).toFixed(4)),
    furnitureType,
    kind,
    heightPx: Math.round(itemHeightPx(item)),
    footprint: normalizeFootprint(item.footprint),
    footprintPolygon: compactLocalPolygon(item.footprintPolygon),
    shadowPolygon: compactLocalPolygon(item.shadowPolygon),
    shadowAnchor: normalizeShadowAnchor(item.shadowAnchor),
    wallShadowDepthPx: Math.round(itemWallShadowDepthPx(item)),
    wallShadowOffsetY: Math.round(itemWallShadowOffsetY(item)),
    castsShadow: itemCastsShadow(item),
    shadowOpacity: Math.round(itemShadowOpacity(item)),
    shadowLength: Math.round(itemShadowLength(item)),
    shadowBlur: Number(itemShadowBlur(item).toFixed(1)),
    receivesShadow: item.receivesShadow !== false,
    occludesSprite: !!item.occludesSprite,
    visibleInDay: item.visibleInDay !== false,
    visibleInNight: item.visibleInNight !== false,
    slot: itemKindPreset(kind).slot,
    layer: itemKindPreset(kind).layer,
    cutoutApplied: !!item.cutoutApplied,
    annotated: isAnnotatedFurniture(item),
    exportedFrom: {
      roomItemId: item.id,
      x: Math.round(item.x),
      y: Math.round(item.y),
      z: item.z
    }
  };
}

function annotatedFurnitureItems(items = state.items) {
  return items.filter(isAnnotatedFurniture);
}

function uniqueSerializedFurnitureItems(items) {
  const counts = new Map();
  return items.map((item) => {
    const baseId = furnitureLibraryIdForItem(item);
    const count = counts.get(baseId) || 0;
    counts.set(baseId, count + 1);
    return serializeFurnitureLibraryItem(item, count ? `${baseId}_${count + 1}` : baseId);
  });
}

function exportFurnitureLibraryBundle(items = annotatedFurnitureItems()) {
  if (!items.length) {
    setStatus("No annotated furniture to export. Configure height, footprint, shadow polygon, or shadow target first.");
    return;
  }
  const payload = {
    type: FURNITURE_LIBRARY_BUNDLE_TYPE,
    version: 1,
    exportedAt: new Date().toISOString(),
    source: "room_editor",
    items: uniqueSerializedFurnitureItems(items)
  };
  downloadText("stickmon_furniture_library_bundle.json", JSON.stringify(payload, null, 2), "application/json");
  setStatus(`Furniture library bundle exported: ${payload.items.length} item(s). Keep it as a portable backup or batch handoff.`);
}

function dataUrlMimeType(dataUrl) {
  return (String(dataUrl || "").match(/^data:([^;,]+)/) || [])[1] || "application/octet-stream";
}

function extensionForImageMime(mime, fallbackName = "") {
  const lower = String(mime || "").toLowerCase();
  if (lower === "image/png") return ".png";
  if (lower === "image/jpeg" || lower === "image/jpg") return ".jpg";
  if (lower === "image/webp") return ".webp";
  const match = String(fallbackName || "").match(/\.[a-z0-9]+$/i);
  return match ? match[0].toLowerCase() : ".png";
}

function libraryImageFileName(item) {
  const ext = extensionForImageMime(dataUrlMimeType(item.dataUrl), item.fileName || item.image || "");
  return `${item.id}${ext}`;
}

function cleanFurnitureLibraryItem(item, imageName) {
  const clean = { ...item };
  delete clean.dataUrl;
  delete clean.imageDataUrl;
  delete clean.path;
  delete clean.exportedFrom;
  clean.image = imageName ? `images/${imageName}` : clean.image || "";
  clean.fileName = clean.fileName || imageName || `${clean.id}.png`;
  clean.revision = Number(clean.revision) || 1;
  return clean;
}

function furnitureLibraryImageFileNameForEntry(entry) {
  const image = typeof entry?.image === "string" ? entry.image : entry?.image?.path || "";
  const fromImage = image.split("/").filter(Boolean).pop();
  if (fromImage) return fromImage;
  if (entry?.dataUrl) return libraryImageFileName(entry);
  return "";
}

function mergeFurnitureLibraryItems(serializedItems) {
  const existingItems = furnitureLibraryCatalog();
  const order = [];
  const byId = new Map();
  for (const item of existingItems) {
    if (!item?.id || byId.has(item.id)) continue;
    order.push(item.id);
    byId.set(item.id, item);
  }

  const savedItems = [];
  for (const item of serializedItems) {
    if (!item?.id) continue;
    const previous = byId.get(item.id);
    const revision = (Number(previous?.revision) || 0) + 1;
    const next = { ...item, revision };
    if (!byId.has(next.id)) order.push(next.id);
    byId.set(next.id, next);
    savedItems.push(next);
  }

  return {
    items: order.map((id) => byId.get(id)).filter(Boolean),
    savedItems
  };
}

function buildFurnitureLibraryPayload(items) {
  const generatedAt = new Date().toISOString();
  const manifestItems = [];
  const jsItems = [];
  const imageFiles = [];

  for (const rawItem of items) {
    const item = normalizeFurnitureLibraryEntry(rawItem);
    if (!item?.id) continue;
    const imageName = item.dataUrl ? libraryImageFileName(item) : "";
    const cleanItem = cleanFurnitureLibraryItem(item, imageName);
    manifestItems.push(cleanItem);

    const jsItem = { ...cleanItem };
    if (item.dataUrl) jsItem.dataUrl = item.dataUrl;
    jsItems.push(jsItem);

    if (item.dataUrl) {
      imageFiles.push({
        name: imageName,
        blob: dataUrlToBlob(item.dataUrl)
      });
    }
  }

  return {
    manifest: {
      version: 1,
      generatedAt,
      items: manifestItems
    },
    js: {
      version: 1,
      generatedAt,
      items: jsItems
    },
    imageFiles
  };
}

async function writeRoomEditorRootHandle(handle) {
  roomEditorRootHandle = handle;
  const db = await openSessionDb();
  try {
    await new Promise((resolve, reject) => {
      const transaction = db.transaction(SESSION_DB_STORE, "readwrite");
      transaction.objectStore(SESSION_DB_STORE).put(handle, ROOM_EDITOR_ROOT_HANDLE_KEY);
      transaction.oncomplete = resolve;
      transaction.onerror = () => reject(transaction.error);
      transaction.onabort = () => reject(transaction.error);
    });
  } finally {
    db.close();
  }
}

async function readRoomEditorRootHandle() {
  if (roomEditorRootHandle) return roomEditorRootHandle;
  const db = await openSessionDb();
  try {
    const handle = await new Promise((resolve, reject) => {
      const transaction = db.transaction(SESSION_DB_STORE, "readonly");
      const request = transaction.objectStore(SESSION_DB_STORE).get(ROOM_EDITOR_ROOT_HANDLE_KEY);
      request.onsuccess = () => resolve(request.result || null);
      request.onerror = () => reject(request.error);
    });
    roomEditorRootHandle = handle;
    return handle;
  } finally {
    db.close();
  }
}

async function getExistingDirectoryHandle(rootHandle, parts) {
  let handle = rootHandle;
  for (const part of parts) {
    handle = await handle.getDirectoryHandle(part);
  }
  return handle;
}

async function getExistingFileHandle(rootHandle, parts) {
  let handle = rootHandle;
  for (let index = 0; index < parts.length - 1; ++index) {
    handle = await handle.getDirectoryHandle(parts[index]);
  }
  return handle.getFileHandle(parts[parts.length - 1]);
}

async function getOrCreateDirectoryHandle(rootHandle, parts) {
  let handle = rootHandle;
  for (const part of parts) {
    handle = await handle.getDirectoryHandle(part, { create: true });
  }
  return handle;
}

async function validateRoomEditorRootHandle(handle) {
  await getExistingFileHandle(handle, ["index.html"]);
  await getExistingFileHandle(handle, ["app.js"]);
  return handle;
}

async function ensureRoomEditorRootHandle(forcePick = false) {
  if (!window.showDirectoryPicker) return null;

  let handle = null;
  if (!forcePick) {
    try {
      handle = await readRoomEditorRootHandle();
    } catch (_) {}
  }

  if (handle) {
    try {
      if (await verifyFileHandlePermission(handle, "readwrite")) {
        await validateRoomEditorRootHandle(handle);
        roomEditorRootHandle = handle;
        return handle;
      }
    } catch (_) {
      handle = null;
    }
  }
  roomEditorRootHandle = null;

  setStatus("Choose the tools/room_editor folder to save editor data.");
  handle = await window.showDirectoryPicker({
    id: "stickmon-room-editor-root",
    mode: "readwrite"
  });
  if (!await verifyFileHandlePermission(handle, "readwrite")) {
    throw new Error("Room editor folder permission was not granted.");
  }
  await validateRoomEditorRootHandle(handle);
  try {
    await writeRoomEditorRootHandle(handle);
  } catch (error) {
    console.warn("Room editor folder handle could not be persisted.", error);
    roomEditorRootHandle = handle;
  }
  return handle;
}

async function writeFileText(directoryHandle, fileName, text) {
  const fileHandle = await directoryHandle.getFileHandle(fileName, { create: true });
  const writable = await fileHandle.createWritable();
  try {
    await writable.write(text);
  } finally {
    await writable.close();
  }
}

async function readFileText(fileHandle) {
  const file = await fileHandle.getFile();
  return file.text();
}

async function writeFileBlob(directoryHandle, fileName, blob) {
  const fileHandle = await directoryHandle.getFileHandle(fileName, { create: true });
  const writable = await fileHandle.createWritable();
  try {
    await writable.write(blob);
  } finally {
    await writable.close();
  }
}

async function removeEntryIfExists(directoryHandle, name, options = {}) {
  if (!directoryHandle || !name) return;
  try {
    await directoryHandle.removeEntry(name, options);
  } catch (error) {
    if (error?.name !== "NotFoundError") throw error;
  }
}

async function writeFurnitureLibraryDirectory(rootDirectoryHandle, payload) {
  const imagesDirectory = await getOrCreateDirectoryHandle(rootDirectoryHandle, ["images"]);
  const itemsDirectory = await getOrCreateDirectoryHandle(rootDirectoryHandle, ["items"]);
  for (const file of payload.imageFiles) {
    await writeFileBlob(imagesDirectory, file.name, file.blob);
  }
  for (const item of payload.manifest.items) {
    await writeFileText(itemsDirectory, `${item.id}.json`, `${JSON.stringify(item, null, 2)}\n`);
  }
  await writeFileText(rootDirectoryHandle, "manifest.json", `${JSON.stringify(payload.manifest, null, 2)}\n`);
}

async function furnitureLibraryProjectTargets(roomEditorRootHandle) {
  return {
    editorRoot: await getOrCreateDirectoryHandle(roomEditorRootHandle, ["generated", "furniture_library"]),
    editorGeneratedRoot: await getOrCreateDirectoryHandle(roomEditorRootHandle, ["generated"])
  };
}

async function writeFurnitureLibraryPayloadToProject(targets, payload) {
  await writeFurnitureLibraryDirectory(targets.editorRoot, payload);
  await writeFileText(
    targets.editorGeneratedRoot,
    "furniture_library_assets.js",
    `window.STICKMON_FURNITURE_LIBRARY = ${JSON.stringify(payload.js, null, 2)};\n`
  );
}

async function writeFurnitureLibraryToProject(items, forcePick = false) {
  const handle = await ensureRoomEditorRootHandle(forcePick);
  if (!handle) return null;

  const payload = buildFurnitureLibraryPayload(items);
  const targets = await furnitureLibraryProjectTargets(handle);
  await writeFurnitureLibraryPayloadToProject(targets, payload);

  window.STICKMON_FURNITURE_LIBRARY = payload.js;
  refreshFurnitureLibraryPanel();
  return payload;
}

async function removeFurnitureLibraryEntryFiles(rootDirectoryHandle, entry) {
  if (!entry?.id) return;
  const itemsDirectory = await getOrCreateDirectoryHandle(rootDirectoryHandle, ["items"]);
  const imagesDirectory = await getOrCreateDirectoryHandle(rootDirectoryHandle, ["images"]);
  await removeEntryIfExists(itemsDirectory, `${entry.id}.json`);
  await removeEntryIfExists(imagesDirectory, furnitureLibraryImageFileNameForEntry(entry));
}

function detachLibraryReferences(libraryId) {
  let detached = 0;
  for (const item of state.items) {
    if (item.libraryId !== libraryId) continue;
    delete item.libraryId;
    delete item.libraryRevision;
    delete item.libraryCategory;
    detached += 1;
  }
  return detached;
}

async function deleteFurnitureLibraryEntry(entry) {
  const normalized = normalizeFurnitureLibraryEntry(entry);
  if (!normalized?.id) return false;
  if (!window.showDirectoryPicker) {
    setStatus("Direct library deleting is not supported in this browser. Use a browser with folder access.");
    return false;
  }

  const placedCount = state.items.filter((item) => item.libraryId === normalized.id).length;
  const placedMessage = placedCount
    ? `\n\n${placedCount} placed item(s) will remain in this room, but they will be detached from the library.`
    : "";
  const confirmed = window.confirm(
    `Permanently delete "${normalized.name}" from the furniture library?` +
    `${placedMessage}\n\nThe library files will be removed and this action cannot be undone.`
  );
  if (!confirmed) return false;

  const remaining = furnitureLibraryCatalog().filter((item) => item.id !== normalized.id);
  const handle = await ensureRoomEditorRootHandle(false);
  if (!handle) return false;
  const targets = await furnitureLibraryProjectTargets(handle);
  const payload = buildFurnitureLibraryPayload(remaining);
  await writeFurnitureLibraryPayloadToProject(targets, payload);
  await removeFurnitureLibraryEntryFiles(targets.editorRoot, normalized);

  window.STICKMON_FURNITURE_LIBRARY = payload.js;
  const detached = detachLibraryReferences(normalized.id);
  refreshFurnitureLibraryPanel();
  refreshSelectedPanel();
  refreshList();
  render();
  if (detached) commitHistory();
  setStatus(`Deleted library furniture: ${normalized.name}.${detached ? ` Detached ${detached} placed item(s).` : ""}`);
  return true;
}

function furnitureLibrarySaveCandidates(items, requireAnnotation = true) {
  return items.filter((item) =>
    item?.source === "furniture" &&
    item.dataUrl &&
    (!requireAnnotation || isAnnotatedFurniture(item))
  );
}

async function addFurnitureItemsToLibrary(items, options = {}) {
  const requireAnnotation = options.requireAnnotation !== false;
  const quiet = options.quiet === true;
  const candidates = furnitureLibrarySaveCandidates(items, requireAnnotation);
  if (!candidates.length) {
    if (!quiet) {
      setStatus(requireAnnotation
        ? "Cannot add selected furniture yet. Configure height, footprint, shadow polygon, or shadow target first."
        : "No importable furniture image data found.");
    }
    return false;
  }

  if (!window.showDirectoryPicker) {
    if (requireAnnotation) exportFurnitureLibraryBundle(candidates);
    if (!quiet) {
      setStatus(requireAnnotation
        ? "Direct library writing is not supported in this browser. A furniture bundle was exported instead."
        : "Direct library writing is not supported in this browser. Furniture was kept in the current session only.");
    }
    return false;
  }

  const serializedItems = uniqueSerializedFurnitureItems(candidates);
  const merged = mergeFurnitureLibraryItems(serializedItems);
  await writeFurnitureLibraryToProject(merged.items);

  candidates.forEach((item, index) => {
    const saved = merged.savedItems[index];
    if (!saved) return;
    item.libraryId = saved.id;
    item.libraryRevision = saved.revision;
    item.libraryCategory = furnitureLibraryCategory(saved);
  });
  if (!quiet) {
    refreshSelectedPanel();
    refreshList();
    render();
    commitHistory();
    setStatus(`Saved ${merged.savedItems.length} furniture item(s) to the room editor library.`);
  }
  return true;
}

function updateProjectSpriteMeta() {
  const meta = document.getElementById("projectSpriteMeta");
  const frame = selectedProjectSpriteFrame();
  if (!frame) {
    meta.textContent = "No generated project sprites found.";
    return;
  }
  meta.textContent = `${frame.kindName}, ${frame.width}x${frame.height}`;
}

function updateSpriteActionOptions() {
  const entry = selectedProjectSpriteEntry();
  const actionSelect = document.getElementById("spriteActionSelect");
  actionSelect.innerHTML = "";
  if (!entry || !entry.actions) {
    updateSpriteFrameOptions();
    return;
  }
  for (const action of Object.keys(entry.actions)) {
    const option = document.createElement("option");
    option.value = action;
    option.textContent = action;
    actionSelect.appendChild(option);
  }
  updateSpriteFrameOptions();
}

function updateSpriteFrameOptions() {
  const entry = selectedProjectSpriteEntry();
  const actionSelect = document.getElementById("spriteActionSelect");
  const frameSelect = document.getElementById("spriteFrameSelect");
  const action = actionSelect.value;
  frameSelect.innerHTML = "";
  if (!entry || !entry.actions || !entry.actions[action]) {
    updateProjectSpriteMeta();
    return;
  }
  for (const frame of entry.actions[action]) {
    const option = document.createElement("option");
    option.value = frame.path;
    option.textContent = frame.kindName;
    frameSelect.appendChild(option);
  }
  updateProjectSpriteMeta();
}

function initProjectSpriteControls() {
  const speciesSelect = document.getElementById("spriteSpeciesSelect");
  const addButton = document.getElementById("addProjectSprite");
  const catalog = projectSpriteCatalog();
  speciesSelect.innerHTML = "";
  for (const entry of catalog) {
    const option = document.createElement("option");
    option.value = String(entry.speciesId);
    option.textContent = entry.name;
    speciesSelect.appendChild(option);
  }
  const empty = catalog.length === 0;
  speciesSelect.disabled = empty;
  document.getElementById("spriteActionSelect").disabled = empty;
  document.getElementById("spriteFrameSelect").disabled = empty;
  addButton.disabled = empty;
  speciesSelect.addEventListener("change", updateSpriteActionOptions);
  document.getElementById("spriteActionSelect").addEventListener("change", updateSpriteFrameOptions);
  document.getElementById("spriteFrameSelect").addEventListener("change", updateProjectSpriteMeta);
  addButton.addEventListener("click", () => {
    addProjectSpritePreview();
  });
  updateSpriteActionOptions();
}

async function addProjectSpritePreview() {
  const entry = selectedProjectSpriteEntry();
  const frame = selectedProjectSpriteFrame();
  if (!entry || !frame) return;
  const loaded = await loadExportableImageSource(projectSpriteFrameSource(frame));
  const maxSize = 64;
  const scale = Math.min(
    maxSize / Math.max(1, frame.width),
    maxSize / Math.max(1, frame.height)
  );
  const targetW = frame.width * scale;
  const targetH = frame.height * scale;
  const frameRect = screenFrameRect();
  const targetX = frameRect.x + (frameRect.w - targetW) / 2;
  const targetY = frameRect.y + (frameRect.h - targetH) / 2;
  const item = {
    id: `f${state.nextId++}`,
    name: entry.name,
    fileName: frame.file,
    img: loaded.img,
    dataUrl: loaded.dataUrl,
    sourceWidth: frame.width,
    sourceHeight: frame.height,
    x: Math.round(targetX),
    y: Math.round(targetY),
    scale,
    scaleX: scale,
    scaleY: scale,
    aspectLocked: true,
    sourceScale: 1,
    opacity: 1,
    z: 30,
    visible: true,
    anchor: "top_left",
    slot: "sprite_preview",
    layer: "sprite_preview",
    source: "project_sprite",
    kind: "floor_small",
    heightPx: 16,
    footprint: "ellipse",
    castsShadow: true,
    receivesShadow: true,
    occludesSprite: false,
    sortY: Math.round(targetY + targetH),
    speciesId: entry.speciesId,
    action: document.getElementById("spriteActionSelect").value,
    frame: frame.kindName,
    exportableImage: loaded.exportable
  };
  state.items.push(item);
  state.selectedId = item.id;
  state.selectedFaceId = null;
  state.selectedPointIndex = null;
  state.selectedPointId = null;
  state.editMode = "furniture";
  updateEditModeButtons();
  setStatus(`Added sprite preview: ${entry.name} ${frame.kindName}.`);
  refreshList();
  refreshSelectedPanel();
  refreshFaceList();
  refreshSelectedFacePanel();
  render();
  commitHistory();
}

async function updateProjectSpriteFrame(item, speciesId, action, framePath) {
  const entry = projectSpriteCatalog().find((e) => e.speciesId === speciesId);
  if (!entry || !entry.actions || !entry.actions[action]) return;
  const frame = entry.actions[action].find((f) => f.path === framePath);
  if (!frame) return;
  const loaded = await loadExportableImageSource(projectSpriteFrameSource(frame));
  item.name = entry.name;
  item.fileName = frame.file;
  item.img = loaded.img;
  item.dataUrl = loaded.dataUrl;
  item.sourceWidth = frame.width;
  item.sourceHeight = frame.height;
  item.speciesId = speciesId;
  item.action = action;
  item.frame = frame.kindName;
  item.exportableImage = loaded.exportable;
  item.sortY = autoSortY(item);
  render();
  commitHistory();
}

function inferBackgroundVisibility(fileName, forcedMode = "") {
  if (forcedMode) {
    const mode = normalizeBackgroundMode(forcedMode);
    return {
      visibleInDay: mode === "day",
      visibleInNight: mode === "night"
    };
  }
  const text = String(fileName || "").toLowerCase();
  const isNight = /(^|[_\-\s])(night|dark|moon|evening)([_\-\s.]|$)|夜|晚上|夜晚/.test(text);
  const isDay = /(^|[_\-\s])(day|light|sun|morning)([_\-\s.]|$)|昼|白天|白日/.test(text);
  if (isNight && !isDay) return { visibleInDay: false, visibleInNight: true };
  if (isDay && !isNight) return { visibleInDay: true, visibleInNight: false };
  return { visibleInDay: true, visibleInNight: true };
}

function normalizeBackgroundItem(background, index = 0) {
  if (!background) return null;
  const visibility = inferBackgroundVisibility(background.name || background.fileName || "", "");
  return {
    ...background,
    id: background.id || `b${state.nextBackgroundId++}`,
    name: background.name || background.fileName || `background_${index + 1}.png`,
    fileName: background.fileName || background.name || "",
    visible: background.visible !== false,
    visibleInDay: background.visibleInDay ?? visibility.visibleInDay,
    visibleInNight: background.visibleInNight ?? visibility.visibleInNight,
    z: Number(background.z ?? index)
  };
}

async function loadBackgroundFiles(files, options = {}) {
  const list = [...files].filter(Boolean);
  if (!list.length) return;
  const hadBackground = hasBackground();
  const loadedBackgrounds = [];
  for (const file of list) {
    const loaded = await readImageFile(file);
    const visibility = inferBackgroundVisibility(file.name, options.mode || "");
    const background = normalizeBackgroundItem({
      id: `b${state.nextBackgroundId++}`,
      name: file.name,
      fileName: file.name,
      img: loaded.img,
      dataUrl: loaded.dataUrl,
      width: loaded.img.naturalWidth,
      height: loaded.img.naturalHeight,
      visible: true,
      visibleInDay: visibility.visibleInDay,
      visibleInNight: visibility.visibleInNight,
      z: backgroundItems().length + loadedBackgrounds.length
    });
    loadedBackgrounds.push(background);
  }
  state.backgroundItems.push(...loadedBackgrounds);
  setLegacyBaseFromBackgrounds();

  const firstLoaded = loadedBackgrounds[0];
  const shouldUpdateSharedTransform = !hadBackground || !state.baseTrim || options.updateTransform === true;
  let prepared = preparedRoomMetrics();
  if (shouldUpdateSharedTransform && firstLoaded?.img) {
    state.baseTrim = detectTrimBox(firstLoaded.img);
    prepared = preparedRoomMetrics();
    const detectedScale = Math.max(0.01, Math.round((prepared.trim.width / GAME_SCREEN_WIDTH) * 100) / 100);
    state.sourceScale = detectedScale;
    document.getElementById("sourceScale").value = detectedScale;
  }
  state.baseFit = BASE_FIT_WIDTH;
  syncCanvasSizeToPreparedRoom();
  document.getElementById("baseFit").value = BASE_FIT_WIDTH;
  updateBaseImageMeta();
  const names = loadedBackgrounds.map((background) => background.name).join(", ");
  setStatus(`Loaded ${loadedBackgrounds.length} background layer(s): ${names}. Shared fit ${GAME_SCREEN_WIDTH}x${prepared.height}, canvas ${state.width}x${state.height}.`);
  refreshList();
  render();
  commitHistory();
}

async function loadBase(file, mode = "day") {
  await loadBackgroundFiles([file], { mode, updateTransform: normalizeBackgroundMode(mode) === "day" || !hasBackground() });
}

async function addFurnitureFiles(files) {
  let loadedCount = 0;
  let skippedCount = 0;
  const loadedItems = [];
  for (const file of files) {
    const loaded = await readImageFile(file);
    const prepared = await promptFurnitureCutout(loaded);
    if (!prepared) {
      skippedCount += 1;
      continue;
    }
    const initialScale = furnitureInitialScale(prepared.img.naturalWidth, prepared.img.naturalHeight);
    const targetW = prepared.img.naturalWidth * initialScale;
    const targetH = prepared.img.naturalHeight * initialScale;
    const furnitureType = inferFurnitureType(file.name);
    const kindPreset = itemKindPreset(inferItemKind(file.name, furnitureType));
    const item = {
      id: `f${state.nextId++}`,
      name: file.name.replace(/\.[^.]+$/, ""),
      fileName: file.name,
      img: prepared.img,
      dataUrl: prepared.dataUrl,
      originalDataUrl: loaded.dataUrl,
      sourceWidth: prepared.img.naturalWidth,
      sourceHeight: prepared.img.naturalHeight,
      x: Math.round((state.width - Math.min(targetW, state.width)) / 2),
      y: Math.round((state.height - Math.min(targetH, state.height)) / 2),
      scale: initialScale,
      scaleX: initialScale,
      scaleY: initialScale,
      aspectLocked: true,
      sourceScale: state.sourceScale,
      opacity: 1,
      z: 20,
      visible: true,
      anchor: "top_left",
      slot: kindPreset.slot,
      layer: kindPreset.layer,
      source: "furniture",
      furnitureType,
      kind: kindPreset.value,
      heightPx: kindPreset.heightPx,
      footprint: kindPreset.footprint,
      castsShadow: kindPreset.castsShadow,
      receivesShadow: kindPreset.receivesShadow,
      occludesSprite: kindPreset.occludesSprite,
      sortY: Math.round(((state.height - Math.min(targetH, state.height)) / 2) + targetH),
      cutoutApplied: prepared.cutoutApplied
    };
    normalizeItemSemantics(item);
    state.items.push(item);
    state.selectedId = item.id;
    loadedItems.push(item);
    loadedCount += 1;
  }
  if (!loadedCount) {
    setStatus(skippedCount ? `Skipped ${skippedCount} furniture file(s).` : "No furniture files loaded.");
    return;
  }
  let librarySuffix = "";
  try {
    const copiedToLibrary = await addFurnitureItemsToLibrary(loadedItems, {
      requireAnnotation: false,
      quiet: true
    });
    librarySuffix = copiedToLibrary
      ? " Copied to the room editor library."
      : " Furniture is currently session-only; select the tools/room_editor folder to copy it into the library.";
  } catch (error) {
    console.error(error);
    librarySuffix = isFilePickerAbort(error)
      ? " Furniture is currently session-only because the room_editor folder was not selected."
      : ` Furniture library copy failed: ${error.message || "unknown error"}.`;
  }
  const skippedSuffix = skippedCount ? ` Skipped ${skippedCount}.` : "";
  setStatus(`Loaded ${loadedCount} furniture file(s), sampled with ${state.furnitureImportMode.replace("_", " ")}.${skippedSuffix}${librarySuffix}`);
  refreshList();
  refreshSelectedPanel();
  refreshFurnitureLibraryPanel();
  render();
  commitHistory();
}

function selectedItem() {
  return state.items.find((item) => item.id === state.selectedId) || null;
}

function positiveNumber(value, fallback) {
  const number = Number(value);
  return Number.isFinite(number) && number > 0 ? number : fallback;
}

function itemScaleX(item) {
  return positiveNumber(item.scaleX, positiveNumber(item.scale, 1));
}

function itemScaleY(item) {
  return positiveNumber(item.scaleY, positiveNumber(item.scale, 1));
}

function itemAspectLocked(item) {
  return item.aspectLocked !== false;
}

function itemTargetWidth(item) {
  return Math.max(ITEM_MIN_TARGET_SIZE, item.sourceWidth * itemScaleX(item));
}

function itemTargetHeight(item) {
  return Math.max(ITEM_MIN_TARGET_SIZE, item.sourceHeight * itemScaleY(item));
}

function setItemUniformScale(item, scale) {
  const value = positiveNumber(scale, itemScaleX(item));
  item.scale = value;
  item.scaleX = value;
  item.scaleY = value;
}

function setItemTargetWidth(item, width) {
  const targetWidth = Math.max(ITEM_MIN_TARGET_SIZE, positiveNumber(width, itemTargetWidth(item)));
  const nextScale = targetWidth / Math.max(1, item.sourceWidth);
  item.scaleX = nextScale;
  if (itemAspectLocked(item)) {
    item.scaleY = nextScale;
  }
  item.scale = item.scaleX;
}

function setItemTargetHeight(item, height) {
  const targetHeight = Math.max(ITEM_MIN_TARGET_SIZE, positiveNumber(height, itemTargetHeight(item)));
  const nextScale = targetHeight / Math.max(1, item.sourceHeight);
  item.scaleY = nextScale;
  if (itemAspectLocked(item)) {
    item.scaleX = nextScale;
  }
  item.scale = item.scaleX;
}

function setItemAspectLocked(item, locked) {
  item.aspectLocked = locked;
  if (locked) {
    item.scaleY = itemScaleX(item);
  }
  item.scale = itemScaleX(item);
}

function deleteItemById(itemId) {
  const item = state.items.find((value) => value.id === itemId);
  if (!item) return false;

  state.items = state.items.filter((value) => value.id !== itemId);
  if (state.selectedId === itemId) {
    state.selectedId = null;
  }
  setStatus(`Deleted ${item.name}.`);
  refreshList();
  refreshSelectedPanel();
  render();
  commitHistory();
  return true;
}

function sortedItems() {
  return [...state.items].sort((a, b) => {
    if (a.z !== b.z) return a.z - b.z;
    const sortDiff = itemSortY(a) - itemSortY(b);
    if (sortDiff !== 0) return sortDiff;
    return a.id.localeCompare(b.id);
  });
}

function itemBounds(item) {
  return {
    x: item.x,
    y: item.y,
    w: itemTargetWidth(item),
    h: itemTargetHeight(item)
  };
}

function autoSortY(item) {
  return Math.round((Number(item.y) || 0) + itemTargetHeight(item));
}

function itemSortY(item) {
  const value = Number(item.sortY);
  return Number.isFinite(value) ? value : autoSortY(item);
}

function itemHeightPx(item) {
  return coerceNumber(item.heightPx, itemKindPreset(item.kind).heightPx, 0, 160);
}

function itemWallShadowOffsetY(item) {
  return coerceNumber(item.wallShadowOffsetY, 0, -80, 80);
}

function itemWallShadowDepthPx(item) {
  const preset = itemKindPreset(item.kind);
  const fallback = preset.wallDepthPx ?? (itemUsesWallShadowAnchor(item) ? 24 : 0);
  return coerceNumber(item.wallShadowDepthPx, fallback, 0, 160);
}

function itemCastsShadow(item) {
  if (item.castsShadow != null) return item.castsShadow !== false;
  return itemKindPreset(item.kind).castsShadow;
}

function itemShadowOpacity(item) {
  return coerceNumber(item?.shadowOpacity ?? item?.shadowAlpha, state.night.shadowAlpha, 0, 100);
}

function itemShadowLength(item) {
  return coerceNumber(item?.shadowLength, state.night.shadowLength, 0, 120);
}

function itemShadowBlur(item) {
  return coerceNumber(item?.shadowBlur, state.night.shadowBlur, 0, 16);
}

function itemShadowFaceIds(item) {
  return normalizeShadowFaceIds(item?.shadowFaceIds);
}

function normalizeItemModeVisibility(item) {
  item.visibleInDay = item.visibleInDay !== false;
  item.visibleInNight = item.visibleInNight !== false;
  if (!item.visibleInDay && !item.visibleInNight) {
    item.visibleInDay = true;
    item.visibleInNight = true;
  }
  return item;
}

function itemVisibleInMode(item, mode = state.mode) {
  if (!item || item.visible === false) return false;
  normalizeItemModeVisibility(item);
  return normalizeBackgroundMode(mode) === "night" ? item.visibleInNight : item.visibleInDay;
}

function itemUsesWallShadowAnchor(item) {
  const anchor = normalizeShadowAnchor(item.shadowAnchor);
  if (anchor === "wall") return true;
  if (anchor === "floor") return false;

  const kind = normalizeItemKind(item.kind);
  if (kind === "wall_flat" || kind === "wall_mounted") return true;

  const name = `${item.fileName || ""} ${item.name || ""}`;
  return WALL_MOUNTED_NAME_PATTERN.test(name);
}

function itemExplicitlyTargetsFace(item, face) {
  if (!face) return false;
  const ids = itemShadowFaceIds(item);
  return ids.length > 0 && ids.includes(String(face.id));
}

function wallShadowFaces({ includeDisabled = false } = {}) {
  return state.faces.filter((face) => {
    if (!face || isSemanticAreaFace(face) || face.points.length < 3) return false;
    const surface = shadowSurfaceForFace(face);
    if (surface !== "left_wall" && surface !== "right_wall") return false;
    return includeDisabled || face.receivesShadow !== false;
  });
}

function pointToSegmentDistance(point, start, end) {
  const dx = end.x - start.x;
  const dy = end.y - start.y;
  const lengthSquared = dx * dx + dy * dy;
  if (lengthSquared < 0.000001) return distance(point, start);
  const t = clampNumber(((point.x - start.x) * dx + (point.y - start.y) * dy) / lengthSquared, 0, 1);
  return distance(point, { x: start.x + dx * t, y: start.y + dy * t });
}

function pointToPolygonDistance(point, points) {
  if (!points.length) return Number.POSITIVE_INFINITY;
  if (pointInPolygon(point, points)) return 0;
  let best = Number.POSITIVE_INFINITY;
  for (let index = 0; index < points.length; index += 1) {
    best = Math.min(best, pointToSegmentDistance(point, points[index], points[(index + 1) % points.length]));
  }
  return best;
}

function itemBoundsSamplePoints(bounds) {
  const left = bounds.x;
  const centerX = bounds.x + bounds.w * 0.5;
  const right = bounds.x + bounds.w;
  const top = bounds.y;
  const centerY = bounds.y + bounds.h * 0.5;
  const bottom = bounds.y + bounds.h;
  return [
    { x: centerX, y: centerY },
    { x: left, y: top },
    { x: centerX, y: top },
    { x: right, y: top },
    { x: left, y: centerY },
    { x: right, y: centerY },
    { x: left, y: bottom },
    { x: centerX, y: bottom },
    { x: right, y: bottom }
  ];
}

function automaticWallShadowFaceId(item) {
  const candidates = wallShadowFaces();
  if (!candidates.length) return null;
  const bounds = itemBounds(item);
  const center = { x: bounds.x + bounds.w * 0.5, y: bounds.y + bounds.h * 0.5 };
  const samples = itemBoundsSamplePoints(bounds);
  let best = null;
  for (const face of candidates) {
    const points = faceTargetPoints(face);
    const insideCount = samples.reduce((count, point) => count + (pointInPolygon(point, points) ? 1 : 0), 0);
    const distanceToFace = pointToPolygonDistance(center, points);
    const candidate = { id: String(face.id), insideCount, distanceToFace };
    if (!best ||
      candidate.insideCount > best.insideCount ||
      (candidate.insideCount === best.insideCount && candidate.distanceToFace < best.distanceToFace)) {
      best = candidate;
    }
  }
  return best?.id || null;
}

function faceTargetedByAnyShadowItem(face) {
  if (!face) return false;
  return state.items.some((item) => itemExplicitlyTargetsFace(item, face));
}

function itemShouldCastShadowOnSurface(item, surface, face = null) {
  const knownWallFaceIds = new Set(wallShadowFaces({ includeDisabled: true }).map((entry) => String(entry.id)));
  const targetFaceIds = itemShadowFaceIds(item).filter((id) => knownWallFaceIds.has(id));
  const targeted = face && targetFaceIds.includes(String(face.id));
  if (face?.receivesShadow === false && !targeted) return false;
  const isWallSurface = surface === "left_wall" || surface === "right_wall";
  const usesWallAnchor = itemUsesWallShadowAnchor(item);
  if (!usesWallAnchor) return surface === "floor";
  if (targetFaceIds.length > 0) {
    return isWallSurface && targeted;
  }
  if (!face) return surface === "floor";
  const automaticFaceId = automaticWallShadowFaceId(item);
  return isWallSurface && automaticFaceId != null && String(face.id) === automaticFaceId;
}

function itemShadowSummary(item) {
  if (!itemCastsShadow(item)) return "off";
  const usesWallAnchor = itemUsesWallShadowAnchor(item);
  const faceCount = itemShadowFaceIds(item).length;
  const target = usesWallAnchor ? "wall plane" : "2.5D planes";
  return `${target} ${Math.round(itemShadowOpacity(item))}%${faceCount ? `, ${faceCount} face(s)` : ""}`;
}

function updateSelectedShadowMeta(item) {
  const meta = document.getElementById("itemShadowMeta");
  if (meta) meta.textContent = itemShadowSummary(item);
}

function shadowFaceDisplayName(face, index = 0) {
  return String(face?.id || `wall ${index + 1}`);
}

function shadowFaceSelectionLabel(item, faces) {
  const ids = itemShadowFaceIds(item);
  if (!ids.length) return "Auto";
  const knownIds = new Set(faces.map((face) => String(face.id)));
  const selected = faces
    .filter((face) => ids.includes(String(face.id)))
    .map((face, index) => shadowFaceDisplayName(face, index));
  const unknown = ids.filter((id) => !knownIds.has(id));
  const labels = [...selected, ...unknown];
  if (!labels.length) return "Auto";
  if (labels.length <= 2) return labels.join(", ");
  return `${labels.slice(0, 2).join(", ")} +${labels.length - 2}`;
}

function updateShadowFaceDropdownLabel(item, faces) {
  const label = document.getElementById("shadowFaceDropdownLabel");
  if (label) label.textContent = shadowFaceSelectionLabel(item, faces);
}

function itemFootprint(item) {
  return normalizeFootprint(item.footprint);
}

function clampUnit(value) {
  return clampNumber(Number(value) || 0, 0, 1);
}

function normalizeLocalPolygon(value) {
  if (!Array.isArray(value)) return [];
  const points = [];
  for (const raw of value) {
    let x;
    let y;
    if (Array.isArray(raw)) {
      x = raw[0];
      y = raw[1];
    } else if (raw && typeof raw === "object") {
      x = raw.x;
      y = raw.y;
    }
    if (!Number.isFinite(Number(x)) || !Number.isFinite(Number(y))) continue;
    points.push({
      x: Number(clampUnit(x).toFixed(4)),
      y: Number(clampUnit(y).toFixed(4))
    });
  }
  return points.length >= 3 ? points : [];
}

function compactLocalPolygon(value) {
  return normalizeLocalPolygon(value).map((point) => [
    Number(point.x.toFixed(4)),
    Number(point.y.toFixed(4))
  ]);
}

function defaultFootprintPolygon() {
  return [
    { x: 0.18, y: 0.72 },
    { x: 0.82, y: 0.72 },
    { x: 0.96, y: 0.96 },
    { x: 0.04, y: 0.96 }
  ];
}

function defaultShadowPolygon() {
  return [
    { x: 0, y: 0 },
    { x: 1, y: 0 },
    { x: 1, y: 1 },
    { x: 0, y: 1 }
  ];
}

function itemUsesAutoFootprintPolygon(item) {
  if (itemFootprint(item) !== "polygon") return false;
  const kind = normalizeItemKind(item.kind);
  return kind !== "floor_flat";
}

function effectiveFootprintPolygon(item) {
  const custom = normalizeLocalPolygon(item.footprintPolygon);
  if (custom.length) return custom;
  return itemUsesAutoFootprintPolygon(item) ? defaultFootprintPolygon() : [];
}

function polygonToText(value) {
  return normalizeLocalPolygon(value)
    .map((point) => `${Number(point.x.toFixed(4))}, ${Number(point.y.toFixed(4))}`)
    .join("\n");
}

function parsePolygonText(text) {
  const points = [];
  for (const line of String(text || "").split(/\n+/)) {
    const match = line.trim().match(/^(-?\d*\.?\d+)\s*[, ]\s*(-?\d*\.?\d+)$/);
    if (!match) continue;
    points.push({ x: Number(match[1]), y: Number(match[2]) });
  }
  return normalizeLocalPolygon(points);
}

function polygonSummary(value) {
  const count = normalizeLocalPolygon(value).length;
  return count ? `${count} pts` : "";
}

function polygonEditorIds(key) {
  return key === "shadowPolygon"
    ? {
        editor: "shadowPolygonEditor",
        textarea: "itemShadowPolygon",
        drawButton: "drawShadowPolygon",
        draftBar: "shadowPolygonDraftBar",
        draftMeta: "shadowPolygonDraftMeta",
        finishDraft: "finishShadowPolygonDraft",
        exitDraft: "exitShadowPolygonDraft"
      }
    : {
        editor: "footprintPolygonEditor",
        textarea: "itemFootprintPolygon",
        drawButton: "drawFootprintPolygon",
        draftBar: "footprintPolygonDraftBar",
        draftMeta: "footprintPolygonDraftMeta",
        finishDraft: "finishFootprintPolygonDraft",
        exitDraft: "exitFootprintPolygonDraft"
      };
}

function polygonDisplayPoints(item, key) {
  return key === "shadowPolygon" ? normalizeLocalPolygon(item.shadowPolygon) : effectiveFootprintPolygon(item);
}

function itemPolygonEditPoints(item, key) {
  const bounds = itemBounds(item);
  return polygonDisplayPoints(item, key).map((point) => targetToEditPoint({
    x: bounds.x + point.x * bounds.w,
    y: bounds.y + point.y * bounds.h
  }));
}

function setActiveItemPolygon(key) {
  state.activeItemPolygonKey = key === "shadowPolygon" ? "shadowPolygon" : "footprintPolygon";
  updatePolygonEditorState();
}

function syncPolygonEditorField(item, key, options = {}) {
  const updateTextarea = options.updateTextarea !== false;
  const ids = polygonEditorIds(key);
  const textarea = document.getElementById(ids.textarea);
  if (textarea && updateTextarea) textarea.value = polygonToText(item[key]);
  const meta = document.getElementById(ids.meta);
  if (meta) meta.textContent = polygonSummary(item[key]);
  if (key === "footprintPolygon") {
    const footprintInput = document.getElementById("itemFootprint");
    if (footprintInput) footprintInput.value = item.footprint;
  }
}

function updatePolygonEditorState() {
  for (const key of ["footprintPolygon", "shadowPolygon"]) {
    const ids = polygonEditorIds(key);
    const editor = document.getElementById(ids.editor);
    if (editor) editor.classList.toggle("active", state.activeItemPolygonKey === key);
    const drafting = state.itemPolygonDraft.active && state.itemPolygonDraft.key === key;
    const draftBar = document.getElementById(ids.draftBar);
    if (draftBar) draftBar.hidden = !drafting;
    const draftMeta = document.getElementById(ids.draftMeta);
    if (draftMeta && drafting) {
      const count = state.itemPolygonDraft.points.length;
      draftMeta.textContent = count >= 3
        ? `${count} point(s). Click the first point or Finish to close.`
        : `${count} point(s). Add at least 3 points.`;
    }
    const finish = document.getElementById(ids.finishDraft);
    if (finish) finish.disabled = !drafting || state.itemPolygonDraft.points.length < 3;
  }
}

function materializeItemPolygon(item, key) {
  if (key === "shadowPolygon") {
    item.shadowPolygon = normalizeLocalPolygon(item.shadowPolygon);
    return item.shadowPolygon;
  }
  const current = normalizeLocalPolygon(item.footprintPolygon);
  item.footprint = "polygon";
  item.footprintPolygon = current.length ? current : defaultFootprintPolygon();
  return item.footprintPolygon;
}

function isItemPolygonDraftActive(key = null) {
  if (!state.itemPolygonDraft.active) return false;
  if (key && state.itemPolygonDraft.key !== key) return false;
  return Boolean(selectedItem() && selectedItem().id === state.itemPolygonDraft.itemId);
}

function resetItemPolygonDraft() {
  state.itemPolygonDraft = {
    active: false,
    itemId: null,
    key: "footprintPolygon",
    points: [],
    dragging: false,
    dragIndex: null,
    moved: false,
    downPoint: null
  };
}

function itemLocalToEditPoint(item, point) {
  const bounds = itemBounds(item);
  return targetToEditPoint({
    x: bounds.x + point.x * bounds.w,
    y: bounds.y + point.y * bounds.h
  });
}

function editPointToItemLocal(item, point) {
  const target = editToTargetPoint(point);
  const bounds = itemBounds(item);
  return {
    x: Number(clampUnit((target.x - bounds.x) / Math.max(1, bounds.w)).toFixed(4)),
    y: Number(clampUnit((target.y - bounds.y) / Math.max(1, bounds.h)).toFixed(4))
  };
}

function clampEditPointToItem(item, point) {
  return itemLocalToEditPoint(item, editPointToItemLocal(item, point));
}

function itemPolygonDraftLocalPoints() {
  const item = selectedItem();
  if (!item || !isItemPolygonDraftActive()) return [];
  return state.itemPolygonDraft.points.map((point) => editPointToItemLocal(item, point));
}

function startItemPolygonDraft(item, key) {
  if (!item || item.source === "project_sprite") return;
  state.editMode = "furniture";
  state.toolMode = "select";
  state.selectedId = item.id;
  state.selectedFaceId = null;
  state.selectedPointIndex = null;
  state.selectedPointId = null;
  state.lightSelected = false;
  state.dragging = false;
  state.draggingItemPolygon = false;
  state.draggingPoint = false;
  state.drawingFace = false;
  state.activeItemPolygonKey = key === "shadowPolygon" ? "shadowPolygon" : "footprintPolygon";
  state.itemPolygonDraft = {
    active: true,
    itemId: item.id,
    key: state.activeItemPolygonKey,
    points: [],
    dragging: false,
    dragIndex: null,
    moved: false,
    downPoint: null
  };
  setStatus("Click canvas points to draw the polygon. Click the first point to close, or press Exit.");
  updateEditModeButtons();
  updatePolygonEditorState();
  render();
}

function cancelItemPolygonDraft(message = "Polygon drawing exited.") {
  if (!state.itemPolygonDraft.active) return;
  canvas.classList.remove("dragging");
  resetItemPolygonDraft();
  setStatus(message);
  updatePolygonEditorState();
  render();
}

function finishItemPolygonDraft() {
  const item = selectedItem();
  if (!item || !isItemPolygonDraftActive()) return;
  const points = normalizeLocalPolygon(itemPolygonDraftLocalPoints());
  if (points.length < 3) {
    setStatus("Add at least 3 points before closing the polygon.");
    updatePolygonEditorState();
    return;
  }
  const key = state.itemPolygonDraft.key;
  if (key === "footprintPolygon") item.footprint = "polygon";
  item[key] = points;
  resetItemPolygonDraft();
  syncPolygonEditorField(item, key);
  setActiveItemPolygon(key);
  setStatus(`${key === "shadowPolygon" ? "Shadow" : "Footprint"} polygon updated.`);
  refreshList();
  render();
  commitHistory();
}

function hitTestItemPolygonDraftPoint(point) {
  if (!isItemPolygonDraftActive()) return -1;
  const radius = editHandleRadius() + 4;
  for (let index = state.itemPolygonDraft.points.length - 1; index >= 0; --index) {
    if (distance(point, state.itemPolygonDraft.points[index]) <= radius) return index;
  }
  return -1;
}

function handleItemPolygonDraftPointerDown(event) {
  const item = selectedItem();
  if (!item || !isItemPolygonDraftActive()) {
    cancelItemPolygonDraft();
    return;
  }
  const canvasPoint = pointerToCanvas(event);
  const point = clampEditPointToItem(item, canvasPoint);
  const hitIndex = hitTestItemPolygonDraftPoint(canvasPoint);
  if (hitIndex >= 0) {
    state.itemPolygonDraft.dragging = true;
    state.itemPolygonDraft.dragIndex = hitIndex;
    state.itemPolygonDraft.moved = false;
    state.itemPolygonDraft.downPoint = canvasPoint;
    canvas.classList.add("dragging");
    canvas.setPointerCapture(event.pointerId);
    setStatus(hitIndex === 0 && state.itemPolygonDraft.points.length >= 3
      ? "Drag to adjust. Release without moving to close the polygon."
      : "Drag point to adjust.");
    render();
    return;
  }
  state.itemPolygonDraft.points.push(point);
  const count = state.itemPolygonDraft.points.length;
  setStatus(count >= 3
    ? `${count} point(s). Click the first point to close.`
    : `${count} point(s). Add at least ${3 - count} more.`);
  updatePolygonEditorState();
  render();
}

function moveItemPolygonDraftPoint(point) {
  const item = selectedItem();
  const draft = state.itemPolygonDraft;
  if (!item || !isItemPolygonDraftActive() || !draft.dragging || draft.dragIndex == null) return;
  if (!draft.points[draft.dragIndex]) return;
  const nextPoint = clampEditPointToItem(item, point);
  draft.points[draft.dragIndex] = nextPoint;
  if (draft.downPoint && distance(point, draft.downPoint) > 2) {
    draft.moved = true;
  }
  render();
}

function finishItemPolygonDraftPointDrag(event) {
  const draft = state.itemPolygonDraft;
  if (!draft.dragging) return false;
  if (event) moveItemPolygonDraftPoint(pointerToCanvas(event));
  const pointWasMoved = draft.moved;
  const shouldClose = event?.type !== "pointercancel" && draft.dragIndex === 0 && !draft.moved && draft.points.length >= 3;
  draft.dragging = false;
  draft.dragIndex = null;
  draft.moved = false;
  draft.downPoint = null;
  canvas.classList.remove("dragging");
  try {
    if (event) canvas.releasePointerCapture(event.pointerId);
  } catch (_) {}
  if (shouldClose) {
    finishItemPolygonDraft();
  } else {
    if (pointWasMoved) setStatus("Polygon point adjusted.");
    updatePolygonEditorState();
    render();
  }
  return true;
}

function itemHasBehaviorOverrides(item) {
  const preset = itemKindPreset(item.kind);
  const currentCastsShadow = itemCastsShadow(item);
  const currentAnchor = itemUsesWallShadowAnchor(item) ? "wall" : "floor";
  const presetAnchor = normalizeShadowAnchor(preset.shadowAnchor) === "wall" ? "wall" : "floor";
  const compareWallShadow = currentCastsShadow || preset.castsShadow;
  return Math.round(itemHeightPx(item)) !== Math.round(preset.heightPx) ||
    normalizeFootprint(item.footprint) !== normalizeFootprint(preset.footprint) ||
    currentCastsShadow !== preset.castsShadow ||
    (item.receivesShadow !== false) !== preset.receivesShadow ||
    !!item.occludesSprite !== preset.occludesSprite ||
    currentAnchor !== presetAnchor ||
    (compareWallShadow && Math.round(itemWallShadowDepthPx(item)) !== Math.round(preset.wallDepthPx ?? 0)) ||
    (compareWallShadow && Math.round(itemWallShadowOffsetY(item)) !== 0) ||
    (compareWallShadow && itemShadowFaceIds(item).length > 0);
}

function applyItemKindPreset(item, kindValue) {
  const preset = itemKindPreset(kindValue);
  item.kind = preset.value;
  item.heightPx = preset.heightPx;
  item.footprint = preset.footprint;
  item.castsShadow = preset.castsShadow;
  item.receivesShadow = preset.receivesShadow;
  item.occludesSprite = preset.occludesSprite;
  item.slot = preset.slot;
  item.layer = preset.layer;
  item.shadowAnchor = preset.shadowAnchor || "auto";
  item.shadowFaceIds = [];
  item.wallShadowDepthPx = preset.wallDepthPx ?? 0;
  item.wallShadowOffsetY = 0;
  item.sortY = autoSortY(item);
}

function normalizeItemSemantics(item) {
  const inferredKind = String(item.slot || "").toLowerCase() === "wall"
    ? (item.castsShadow === false ? "wall_flat" : "wall_mounted")
    : inferItemKind(item.fileName || item.name, item.furnitureType);
  const preset = itemKindPreset(normalizeItemKindForRecord(item.kind || inferredKind, item));
  item.kind = preset.value;
  normalizeItemModeVisibility(item);
  item.heightPx = coerceNumber(item.heightPx, preset.heightPx, 0, 160);
  item.footprint = normalizeFootprint(item.footprint || preset.footprint);
  item.castsShadow = item.castsShadow ?? preset.castsShadow;
  item.receivesShadow = item.receivesShadow ?? preset.receivesShadow;
  item.occludesSprite = item.occludesSprite ?? preset.occludesSprite;
  item.slot = preset.slot;
  item.layer = preset.layer;
  item.shadowAnchor = normalizeShadowAnchor(item.shadowAnchor || preset.shadowAnchor);
  item.shadowFaceIds = normalizeShadowFaceIds(item.shadowFaceIds);
  item.wallShadowOffsetY = itemWallShadowOffsetY(item);
  item.wallShadowDepthPx = itemWallShadowDepthPx(item);
  item.shadowOpacity = itemShadowOpacity(item);
  item.shadowLength = itemShadowLength(item);
  item.shadowBlur = itemShadowBlur(item);
  item.footprintPolygon = normalizeLocalPolygon(item.footprintPolygon);
  item.shadowPolygon = normalizeLocalPolygon(item.shadowPolygon);
  if (!Number.isFinite(Number(item.sortY))) item.sortY = autoSortY(item);
  return item;
}

function modeLightKey(mode = state.mode) {
  return mode === "night" ? "nightLight" : "dayLight";
}

function copyLightSettings(light) {
  return Object.fromEntries(LIGHT_SETTING_KEYS.map((key) => [key, light[key]]));
}

function assignLightSettings(target, source) {
  const normalized = normalizeLightSettings(source, target || DEFAULT_NIGHT);
  for (const key of LIGHT_SETTING_KEYS) {
    target[key] = normalized[key];
  }
  return target;
}

function previewLightSettings(mode = state.mode) {
  ensureSeparateModeLights();
  return state.night[modeLightKey(mode)];
}

function activeLightSettings(mode = state.mode) {
  return previewLightSettings(mode);
}

function editLightSettings() {
  ensureSeparateModeLights();
  return state.night[modeLightKey(state.mode)];
}

function activeLightPoint(mode = state.mode) {
  const light = activeLightSettings(mode);
  return { x: light.lightX, y: light.lightY };
}

function editLightPointTarget() {
  const light = editLightSettings();
  return { x: light.lightX, y: light.lightY };
}

function readLightInputs() {
  return normalizeLightSettings({
    lightShape: document.getElementById("lightShape").value,
    lightStrength: Number(document.getElementById("lightStrength").value) || 0,
    lightX: Number(document.getElementById("lightX").value) || 0,
    lightY: Number(document.getElementById("lightY").value) || 0,
    lightDepth: Number(document.getElementById("lightDepth").value) || DEFAULT_NIGHT.lightDepth,
    lightRadius: Number(document.getElementById("lightRadius").value) || DEFAULT_NIGHT.lightRadius,
    lightAngle: Number(document.getElementById("lightAngle").value) || 0,
    lightSpread: Number(document.getElementById("lightSpread").value) || DEFAULT_NIGHT.lightSpread
  }, editLightSettings());
}

function commitLightInputsToState() {
  assignLightSettings(editLightSettings(), readLightInputs());
}

function syncLightInputsFromState() {
  const light = editLightSettings();
  document.getElementById("lightShape").value = light.lightShape;
  document.getElementById("lightStrength").value = light.lightStrength;
  document.getElementById("lightX").value = light.lightX;
  document.getElementById("lightY").value = light.lightY;
  document.getElementById("lightDepth").value = light.lightDepth;
  document.getElementById("lightRadius").value = light.lightRadius;
  document.getElementById("lightAngle").value = light.lightAngle;
  document.getElementById("lightSpread").value = light.lightSpread;
  updateLightModeActions();
}

function currentLightProfile() {
  return state.mode === "night" ? "night" : "day";
}

function updateLightModeActions() {
  const mode = currentLightProfile();
  const meta = document.getElementById("lightModeMeta");
  if (meta) meta.textContent = mode === "night" ? "Preview Night" : "Preview Day";
  document.getElementById("copyLightToDay").hidden = mode !== "day";
  document.getElementById("copyLightToNight").hidden = mode !== "night";
}

function ensureSeparateModeLights() {
  if (state.night.separateModeLights) return;
  const shared = copyLightSettings(state.night);
  state.night.dayLight = normalizeLightSettings(state.night.dayLight, shared);
  state.night.nightLight = normalizeLightSettings(state.night.nightLight, shared);
  state.night.separateModeLights = true;
}

function copyActiveLightToProfile(profile) {
  profile = normalizeLightProfile(profile);
  if (profile === "shared") return;
  commitLightInputsToState();
  const source = copyLightSettings(editLightSettings());
  ensureSeparateModeLights();
  assignLightSettings(state.night[modeLightKey(profile)], source);
  state.lightProfile = profile;
  syncLightInputsFromState();
  updateModeButtons();
  setStatus(`Light settings copied to ${profile === "night" ? "night" : "day"}.`);
}

function syncLightProfileWithMode() {
  ensureSeparateModeLights();
  state.lightProfile = currentLightProfile();
  updateLightModeActions();
}

const shadowMaskCache = new WeakMap();

function localPolygonMask(item, polygon) {
  const width = item.img?.naturalWidth || item.img?.width || item.sourceWidth || 1;
  const height = item.img?.naturalHeight || item.img?.height || item.sourceHeight || 1;
  const points = normalizeLocalPolygon(polygon);
  if (!points.length) return null;
  const mask = document.createElement("canvas");
  mask.width = Math.max(1, Math.round(width));
  mask.height = Math.max(1, Math.round(height));
  const maskCtx = mask.getContext("2d");
  maskCtx.fillStyle = "#000000";
  maskCtx.beginPath();
  points.forEach((point, index) => {
    const x = point.x * mask.width;
    const y = point.y * mask.height;
    if (index === 0) maskCtx.moveTo(x, y);
    else maskCtx.lineTo(x, y);
  });
  maskCtx.closePath();
  maskCtx.fill();
  return mask;
}

function itemShadowMask(item) {
  if (!item.img) return null;
  const customMask = localPolygonMask(item, item.shadowPolygon);
  if (customMask) return customMask;
  if (shadowMaskCache.has(item.img)) return shadowMaskCache.get(item.img);
  const width = item.img.naturalWidth || item.img.width || item.sourceWidth || 1;
  const height = item.img.naturalHeight || item.img.height || item.sourceHeight || 1;
  const mask = document.createElement("canvas");
  mask.width = width;
  mask.height = height;
  const maskCtx = mask.getContext("2d");
  maskCtx.imageSmoothingEnabled = false;
  maskCtx.drawImage(item.img, 0, 0, width, height);
  maskCtx.globalCompositeOperation = "source-in";
  maskCtx.fillStyle = "#000000";
  maskCtx.fillRect(0, 0, width, height);
  shadowMaskCache.set(item.img, mask);
  return mask;
}

function activeShadowMode() {
  const mode = normalizeShadowMode(state.night.shadowMode);
  if (mode !== "auto") return mode;
  return normalizeLightShape(activeLightSettings().lightShape) === "cone" ? "directional" : "point";
}

function shadowDirectionForPoint(footX, footY, lightPoint) {
  if (activeShadowMode() === "directional") {
    const angle = angleToRad(activeLightSettings().lightAngle);
    return { ux: Math.cos(angle), uy: Math.sin(angle) };
  }

  let dx = footX - lightPoint.x;
  let dy = footY - lightPoint.y;
  let distance = Math.hypot(dx, dy);
  if (distance < 0.001) {
    dx = 0;
    dy = 1;
    distance = 1;
  }
  return {
    ux: dx / distance,
    uy: dy / distance
  };
}

function shadowLightFactorForPoint(footX, footY, lightPoint, scale = 1) {
  const light = activeLightSettings();
  const strength = clampNumber(Number(light.lightStrength) || 0, 0, 200) / 100;
  if (strength <= 0) return 0;

  const radius = Math.max(1, (Number(light.lightRadius) || DEFAULT_NIGHT.lightRadius) * scale);
  const dx = footX - lightPoint.x;
  const dy = footY - lightPoint.y;
  const dist = Math.hypot(dx, dy);
  if (dist > radius) return 0;

  let coneFactor = 1;
  if (normalizeLightShape(light.lightShape) === "cone" && dist > 0.001) {
    const halfSpread = Math.max(0.001, angleToRad(light.lightSpread) * 0.5);
    const delta = Math.abs(angleDeltaRad(Math.atan2(dy, dx), angleToRad(light.lightAngle)));
    if (delta > halfSpread) return 0;
    coneFactor = 1 - (delta / halfSpread) * 0.28;
  }

  const falloff = Math.max(0, 1 - dist / radius) ** 0.7;
  return Math.sqrt(strength) * (0.45 + 0.55 * falloff) * coneFactor;
}

function faceReceivesShadow(face) {
  return face &&
    !isSemanticAreaFace(face) &&
    face.points.length >= 3 &&
    (face.receivesShadow !== false || faceTargetedByAnyShadowItem(face));
}

function shadowSurfaceForFace(face) {
  return normalizeShadowSurface(face?.shadowSurface, face?.type);
}

function receivingShadowFaces() {
  return state.faces.filter(faceReceivesShadow);
}

function wallShadowSkew(surface) {
  if (surface === "left_wall") return -0.16;
  if (surface === "right_wall") return 0.16;
  return 0;
}

function projectionSharedPoints(pointsA, pointsB) {
  if (!pointsA.length || !pointsB.length) return [];
  const allPoints = [...pointsA, ...pointsB];
  const minX = Math.min(...allPoints.map((point) => point.x));
  const maxX = Math.max(...allPoints.map((point) => point.x));
  const minY = Math.min(...allPoints.map((point) => point.y));
  const maxY = Math.max(...allPoints.map((point) => point.y));
  const tolerance = Math.max(2, Math.hypot(maxX - minX, maxY - minY) * 0.015);
  const shared = [];
  for (const pointA of pointsA) {
    for (const pointB of pointsB) {
      const separation = Math.hypot(pointA.x - pointB.x, pointA.y - pointB.y);
      if (separation <= tolerance) {
        shared.push({ x: (pointA.x + pointB.x) * 0.5, y: (pointA.y + pointB.y) * 0.5 });
      }
    }
  }
  if (shared.length) return shared;
  let closest = null;
  for (const pointA of pointsA) {
    for (const pointB of pointsB) {
      const separation = Math.hypot(pointA.x - pointB.x, pointA.y - pointB.y);
      if (!closest || separation < closest.separation) closest = { pointA, pointB, separation };
    }
  }
  return closest ? [{
    x: (closest.pointA.x + closest.pointB.x) * 0.5,
    y: (closest.pointA.y + closest.pointB.y) * 0.5
  }] : [];
}

function closestProjectionPair(pointsA, pointsB) {
  let closest = null;
  for (const pointA of pointsA) {
    for (const pointB of pointsB) {
      const separation = Math.hypot(pointA.x - pointB.x, pointA.y - pointB.y);
      if (!closest || separation < closest.separation) closest = { pointA, pointB, separation };
    }
  }
  return closest;
}

function roomProjectionModel(pointsForFace = faceTargetPoints) {
  const floorFace = state.faces.find((face) => !isSemanticAreaFace(face) && shadowSurfaceForFace(face) === "floor");
  const leftFace = state.faces.find((face) => !isSemanticAreaFace(face) && shadowSurfaceForFace(face) === "left_wall");
  const rightFace = state.faces.find((face) => !isSemanticAreaFace(face) && shadowSurfaceForFace(face) === "right_wall");
  if (!floorFace || !leftFace || !rightFace) return null;

  const floorPoints = pointsForFace(floorFace);
  const leftPoints = pointsForFace(leftFace);
  const rightPoints = pointsForFace(rightFace);
  const leftFloor = projectionSharedPoints(leftPoints, floorPoints);
  const rightFloor = projectionSharedPoints(rightPoints, floorPoints);
  const wallCornerPair = closestProjectionPair(leftFloor, rightFloor);
  if (!wallCornerPair) return null;
  const origin = {
    x: (wallCornerPair.pointA.x + wallCornerPair.pointB.x) * 0.5,
    y: (wallCornerPair.pointA.y + wallCornerPair.pointB.y) * 0.5
  };
  const farthestFromOrigin = (points) => points.reduce((best, point) =>
    !best || distance(point, origin) > distance(best, origin) ? point : best, null);
  const leftExtent = farthestFromOrigin(leftFloor);
  const rightExtent = farthestFromOrigin(rightFloor);
  const wallShared = projectionSharedPoints(leftPoints, rightPoints);
  const top = farthestFromOrigin(wallShared);
  if (!leftExtent || !rightExtent || !top) return null;

  const axisU = { x: leftExtent.x - origin.x, y: leftExtent.y - origin.y };
  const axisV = { x: rightExtent.x - origin.x, y: rightExtent.y - origin.y };
  const axisZ = { x: top.x - origin.x, y: top.y - origin.y };
  const determinant = axisU.x * axisV.y - axisU.y * axisV.x;
  const axisZLength = Math.hypot(axisZ.x, axisZ.y);
  if (Math.abs(determinant) < 0.001 || axisZLength < 1) return null;
  return {
    type: "corner_local_planes_v1",
    origin,
    axisU,
    axisV,
    axisZ,
    axisZLength,
    determinant,
    faces: {
      floor: floorFace,
      left_wall: leftFace,
      right_wall: rightFace
    },
    points: new Map([
      [String(floorFace.id), floorPoints],
      [String(leftFace.id), leftPoints],
      [String(rightFace.id), rightPoints]
    ])
  };
}

function screenPointToProjectionWorld(model, point, heightPx = 0) {
  const z = heightPx / model.axisZLength;
  const flatX = point.x - model.origin.x - model.axisZ.x * z;
  const flatY = point.y - model.origin.y - model.axisZ.y * z;
  return {
    u: (flatX * model.axisV.y - flatY * model.axisV.x) / model.determinant,
    v: (model.axisU.x * flatY - model.axisU.y * flatX) / model.determinant,
    z
  };
}

function projectionWorldToScreen(model, point) {
  return {
    x: model.origin.x + model.axisU.x * point.u + model.axisV.x * point.v + model.axisZ.x * point.z,
    y: model.origin.y + model.axisU.y * point.u + model.axisV.y * point.v + model.axisZ.y * point.z
  };
}

function itemProjectionFaces(item, model) {
  const explicit = new Set(itemShadowFaceIds(item));
  const validExplicit = [...explicit].some((id) => model.points.has(String(id)));
  return Object.entries(model.faces)
    .map(([surface, face]) => ({ surface, face, points: model.points.get(String(face.id)) || [] }))
    .filter(({ surface, face }) => {
      const targeted = explicit.has(String(face.id));
      if (face.receivesShadow === false && !targeted) return false;
      if (surface === "floor") return true;
      return !validExplicit || targeted;
    });
}

const projectionContourCache = new WeakMap();

function projectionConvexHull(points) {
  if (points.length <= 3) return points;
  const sorted = [...points].sort((a, b) => a.x - b.x || a.y - b.y);
  const cross = (origin, a, b) =>
    (a.x - origin.x) * (b.y - origin.y) - (a.y - origin.y) * (b.x - origin.x);
  const half = [];
  for (const point of sorted) {
    while (half.length >= 2 && cross(half[half.length - 2], half[half.length - 1], point) <= 0) half.pop();
    half.push(point);
  }
  const lower = [...half];
  half.length = 0;
  for (let index = sorted.length - 1; index >= 0; index -= 1) {
    const point = sorted[index];
    while (half.length >= 2 && cross(half[half.length - 2], half[half.length - 1], point) <= 0) half.pop();
    half.push(point);
  }
  lower.pop();
  half.pop();
  return [...lower, ...half];
}

function imageAlphaProjectionContour(item) {
  if (!item.img) return [];
  if (projectionContourCache.has(item.img)) return projectionContourCache.get(item.img);
  try {
    const sourceWidth = item.img.naturalWidth || item.img.width || item.sourceWidth || 1;
    const sourceHeight = item.img.naturalHeight || item.img.height || item.sourceHeight || 1;
    const sampleScale = Math.min(1, 128 / Math.max(sourceWidth, sourceHeight));
    const width = Math.max(1, Math.round(sourceWidth * sampleScale));
    const height = Math.max(1, Math.round(sourceHeight * sampleScale));
    const canvas = document.createElement("canvas");
    canvas.width = width;
    canvas.height = height;
    const sampleCtx = canvas.getContext("2d", { willReadFrequently: true });
    sampleCtx.imageSmoothingEnabled = false;
    sampleCtx.drawImage(item.img, 0, 0, width, height);
    const pixels = sampleCtx.getImageData(0, 0, width, height).data;
    const points = [];
    for (let y = 0; y < height; y += 1) {
      for (let x = 0; x < width; x += 1) {
        if (pixels[(y * width + x) * 4 + 3] > 8) points.push({ x, y });
      }
    }
    const contour = projectionConvexHull(points).map((point) => ({
      x: clampUnit((point.x + 0.5) / width),
      y: clampUnit((point.y + 0.5) / height)
    }));
    projectionContourCache.set(item.img, contour);
    return contour;
  } catch (_) {
    projectionContourCache.set(item.img, []);
    return [];
  }
}

function itemProjectionContour(item) {
  const custom = normalizeLocalPolygon(item.shadowPolygon);
  if (custom.length) return custom;
  const footprint = itemFootprint(item);
  if (footprint === "none") return [];
  if (footprint === "polygon") return effectiveFootprintPolygon(item);
  if (footprint === "rect") return [
    { x: 0.12, y: 0.72 },
    { x: 0.88, y: 0.72 },
    { x: 0.88, y: 0.96 },
    { x: 0.12, y: 0.96 }
  ];
  if (footprint === "ellipse") {
    return Array.from({ length: 12 }, (_, index) => {
      const angle = Math.PI * 2 * index / 12;
      return { x: 0.5 + Math.cos(angle) * 0.38, y: 0.84 + Math.sin(angle) * 0.13 };
    });
  }
  const alphaContour = imageAlphaProjectionContour(item);
  return alphaContour.length >= 3 ? alphaContour : defaultShadowPolygon();
}

function projectionRayDirection(model, casterWorld, screenPoint, heightPx, lightPoint, scale) {
  if (activeShadowMode() === "directional") {
    const angle = angleToRad(activeLightSettings().lightAngle);
    const targetScreen = {
      x: screenPoint.x + Math.cos(angle) * heightPx,
      y: screenPoint.y + Math.sin(angle) * heightPx
    };
    const targetWorld = screenPointToProjectionWorld(model, targetScreen, 0);
    return {
      u: targetWorld.u - casterWorld.u,
      v: targetWorld.v - casterWorld.v,
      z: targetWorld.z - casterWorld.z
    };
  }
  const lightWorld = screenPointToProjectionWorld(
    model,
    lightPoint,
    activeLightSettings().lightDepth * scale
  );
  return {
    u: casterWorld.u - lightWorld.u,
    v: casterWorld.v - lightWorld.v,
    z: casterWorld.z - lightWorld.z
  };
}

function projectionPlaneIntersection(origin, direction, surface) {
  const coordinate = surface === "floor" ? "z" : surface === "left_wall" ? "v" : "u";
  const denominator = direction[coordinate];
  const value = origin[coordinate];
  if (Math.abs(denominator) < 0.000001) {
    return Math.abs(value) < 0.000001 ? { ...origin, t: 0 } : null;
  }
  const t = -value / denominator;
  if (t < -0.000001) return null;
  return {
    u: origin.u + direction.u * t,
    v: origin.v + direction.v * t,
    z: origin.z + direction.z * t,
    t
  };
}

function nearestProjectionReceiver(model, origin, direction, receivers, scale) {
  let best = null;
  const tolerance = Math.max(1, scale * 1.5);
  for (const receiver of receivers) {
    const hit = projectionPlaneIntersection(origin, direction, receiver.surface);
    if (!hit) continue;
    const screen = projectionWorldToScreen(model, hit);
    if (!pointInPolygon(screen, receiver.points) && pointToPolygonDistance(screen, receiver.points) > tolerance) continue;
    if (!best || hit.t < best.t) best = { ...hit, screen, receiver };
  }
  return best;
}

const PROJECTION_GUIDE_COLORS = {
  u: "#ff9b73",
  v: "#63d6ff",
  z: "#91e68a",
  floor: "#f2bd72",
  left_wall: "#ff9b73",
  right_wall: "#63d6ff"
};

function projectionGuideAutoActive() {
  return state.editMode === "shape" ||
    state.lightSelected ||
    document.getElementById("shadowAdvancedGroup")?.open === true;
}

function projectionGuidesVisible() {
  return state.showProjectionGuides || projectionGuideAutoActive();
}

function updateProjectionGuideButton(model = null) {
  const button = document.getElementById("projectionGuides");
  if (!button) return;
  const automatic = projectionGuideAutoActive();
  const hasProjection = Boolean(model);
  button.classList.toggle("active", state.showProjectionGuides);
  button.classList.toggle("auto-active", !state.showProjectionGuides && automatic && hasProjection);
  button.setAttribute("aria-pressed", state.showProjectionGuides ? "true" : "false");
  button.setAttribute("aria-label", hasProjection
    ? `2.5D guides: ${state.showProjectionGuides ? "pinned" : automatic ? "automatic" : "off"}`
    : "2.5D guides: draw floor, left wall, and right wall faces first");
}

function projectionGuideCanvasScale() {
  return 1 / Math.max(0.25, state.editZoom || 1);
}

function unitVector(vector) {
  const length = Math.hypot(vector.x, vector.y);
  if (length < 0.000001) return { x: 0, y: 0 };
  return { x: vector.x / length, y: vector.y / length };
}

function drawProjectionArrow(targetCtx, start, end, color, { dashed = false, alpha = 1 } = {}) {
  const scale = projectionGuideCanvasScale();
  const direction = unitVector({ x: end.x - start.x, y: end.y - start.y });
  if (!direction.x && !direction.y) return;
  const headLength = 7 * scale;
  const headWidth = 4 * scale;
  const base = {
    x: end.x - direction.x * headLength,
    y: end.y - direction.y * headLength
  };
  const normal = { x: -direction.y, y: direction.x };

  targetCtx.save();
  targetCtx.globalAlpha = alpha;
  targetCtx.strokeStyle = color;
  targetCtx.fillStyle = color;
  targetCtx.lineWidth = 1.35 * scale;
  if (dashed) targetCtx.setLineDash([5 * scale, 4 * scale]);
  targetCtx.beginPath();
  targetCtx.moveTo(start.x, start.y);
  targetCtx.lineTo(base.x, base.y);
  targetCtx.stroke();
  targetCtx.setLineDash([]);
  targetCtx.beginPath();
  targetCtx.moveTo(end.x, end.y);
  targetCtx.lineTo(base.x + normal.x * headWidth, base.y + normal.y * headWidth);
  targetCtx.lineTo(base.x - normal.x * headWidth, base.y - normal.y * headWidth);
  targetCtx.closePath();
  targetCtx.fill();
  targetCtx.restore();
}

function drawProjectionLabel(targetCtx, text, point, color, offset = { x: 0, y: 0 }) {
  const scale = projectionGuideCanvasScale();
  const fontSize = 10 * scale;
  const paddingX = 5 * scale;
  const paddingY = 3 * scale;
  targetCtx.save();
  targetCtx.font = `600 ${fontSize}px sans-serif`;
  targetCtx.textBaseline = "top";
  const width = targetCtx.measureText(text).width + paddingX * 2;
  const height = fontSize + paddingY * 2;
  const desiredX = point.x + offset.x * scale;
  const desiredY = point.y + offset.y * scale;
  const x = clampNumber(desiredX, 2 * scale, Math.max(2 * scale, state.editWidth - width - 2 * scale));
  const y = clampNumber(desiredY, 2 * scale, Math.max(2 * scale, state.editHeight - height - 2 * scale));
  targetCtx.fillStyle = "rgba(9, 12, 17, 0.82)";
  targetCtx.fillRect(x, y, width, height);
  targetCtx.strokeStyle = color;
  targetCtx.lineWidth = scale;
  targetCtx.strokeRect(x + 0.5 * scale, y + 0.5 * scale, width - scale, height - scale);
  targetCtx.fillStyle = color;
  targetCtx.fillText(text, x + paddingX, y + paddingY);
  targetCtx.restore();
}

function drawProjectionInfoPanel(targetCtx, lines, point) {
  if (!lines.length) return;
  const scale = projectionGuideCanvasScale();
  const fontSize = 9 * scale;
  const lineHeight = 13 * scale;
  const padding = 6 * scale;
  targetCtx.save();
  targetCtx.font = `${fontSize}px ui-monospace, SFMono-Regular, Menlo, monospace`;
  const width = Math.max(...lines.map((line) => targetCtx.measureText(line).width)) + padding * 2;
  const height = lineHeight * lines.length + padding * 2 - 2 * scale;
  const x = clampNumber(point.x, 3 * scale, Math.max(3 * scale, state.editWidth - width - 3 * scale));
  const y = clampNumber(point.y, 3 * scale, Math.max(3 * scale, state.editHeight - height - 3 * scale));
  targetCtx.fillStyle = "rgba(9, 12, 17, 0.88)";
  targetCtx.fillRect(x, y, width, height);
  targetCtx.strokeStyle = "rgba(217, 248, 255, 0.68)";
  targetCtx.lineWidth = scale;
  targetCtx.strokeRect(x + 0.5 * scale, y + 0.5 * scale, width - scale, height - scale);
  targetCtx.fillStyle = "#eef2f7";
  targetCtx.textBaseline = "top";
  lines.forEach((line, index) => targetCtx.fillText(line, x + padding, y + padding + index * lineHeight));
  targetCtx.restore();
}

function drawProjectionMiniGizmo(model) {
  const scale = projectionGuideCanvasScale();
  const panel = {
    x: 8 * scale,
    y: 8 * scale,
    w: 72 * scale,
    h: 48 * scale
  };
  const origin = { x: panel.x + 35 * scale, y: panel.y + 27 * scale };
  const axes = [
    { key: "u", label: "U", vector: model.axisU, length: 16 },
    { key: "v", label: "V", vector: model.axisV, length: 16 },
    { key: "z", label: "Z", vector: model.axisZ, length: 18 }
  ];

  ctx.save();
  ctx.fillStyle = "rgba(9, 12, 17, 0.76)";
  ctx.fillRect(panel.x, panel.y, panel.w, panel.h);
  ctx.strokeStyle = "rgba(255, 255, 255, 0.22)";
  ctx.lineWidth = scale;
  ctx.strokeRect(panel.x + 0.5 * scale, panel.y + 0.5 * scale, panel.w - scale, panel.h - scale);
  ctx.fillStyle = "rgba(238, 242, 247, 0.78)";
  ctx.font = `600 ${8 * scale}px sans-serif`;
  ctx.textBaseline = "top";
  ctx.fillText("2.5D", panel.x + 4 * scale, panel.y + 4 * scale);
  for (const axis of axes) {
    const direction = unitVector(axis.vector);
    const end = {
      x: origin.x + direction.x * axis.length * scale,
      y: origin.y + direction.y * axis.length * scale
    };
    drawProjectionArrow(ctx, origin, end, PROJECTION_GUIDE_COLORS[axis.key]);
    ctx.fillStyle = PROJECTION_GUIDE_COLORS[axis.key];
    ctx.font = `700 ${8 * scale}px sans-serif`;
    ctx.fillText(axis.label, end.x + direction.x * 2 * scale - 2 * scale, end.y + direction.y * 2 * scale - 4 * scale);
  }
  ctx.fillStyle = "#ffffff";
  ctx.beginPath();
  ctx.arc(origin.x, origin.y, 2.2 * scale, 0, Math.PI * 2);
  ctx.fill();
  ctx.restore();
}

function projectionReceiverLabel(receiver) {
  if (!receiver) return "None";
  if (receiver.surface === "left_wall") return `Left ${receiver.face.id}`;
  if (receiver.surface === "right_wall") return `Right ${receiver.face.id}`;
  return `Floor ${receiver.face.id}`;
}

function floorItemProjectionDiagnostics(item, bounds, lightPoint, scale, model) {
  const receivers = itemProjectionFaces(item, model);
  const contour = itemProjectionContour(item);
  const rays = [];
  for (const point of contour) {
    const casterPoint = { x: bounds.x + point.x * bounds.w, y: bounds.y + point.y * bounds.h };
    const heightPx = localCasterHeight(item, point, scale);
    const casterWorld = screenPointToProjectionWorld(model, casterPoint, heightPx);
    const direction = projectionRayDirection(model, casterWorld, casterPoint, heightPx, lightPoint, scale);
    const hit = nearestProjectionReceiver(model, casterWorld, direction, receivers, scale);
    rays.push({ casterPoint, heightPx, hit });
  }
  return rays;
}

function wallItemProjectionReceivers(item, model) {
  const explicit = new Set(itemShadowFaceIds(item));
  const automaticId = automaticWallShadowFaceId(item);
  return [model.faces.left_wall, model.faces.right_wall]
    .filter(Boolean)
    .filter((face) => explicit.size ? explicit.has(String(face.id)) : String(face.id) === String(automaticId))
    .map((face) => ({
      face,
      surface: shadowSurfaceForFace(face),
      points: model.points.get(String(face.id)) || []
    }));
}

function drawProjectionReceiverHighlights(receivers) {
  const scale = projectionGuideCanvasScale();
  ctx.save();
  for (const receiver of receivers) {
    if (!receiver?.points?.length) continue;
    const color = PROJECTION_GUIDE_COLORS[receiver.surface] || "#ffffff";
    drawPolygonPath(ctx, receiver.points);
    ctx.fillStyle = color;
    ctx.globalAlpha = 0.11;
    ctx.fill();
    ctx.globalAlpha = 0.92;
    ctx.strokeStyle = color;
    ctx.lineWidth = 1.4 * scale;
    ctx.setLineDash([6 * scale, 4 * scale]);
    ctx.stroke();
    ctx.setLineDash([]);
  }
  ctx.restore();
}

function drawSelectedItemProjectionDiagnostics(model) {
  const item = selectedItem();
  if (!item || item.source === "project_sprite" || !itemVisibleInMode(item)) return;
  const scale = Math.max(editScaleX(), editScaleY());
  const targetBounds = itemBounds(item);
  const bounds = targetRectToEdit(targetBounds.x, targetBounds.y, targetBounds.w, targetBounds.h);
  const anchor = { x: bounds.x + bounds.w * 0.5, y: bounds.y + bounds.h * 0.88 };
  const wallAnchor = itemUsesWallShadowAnchor(item);
  const anchorWorld = screenPointToProjectionWorld(model, anchor, 0);
  const effectiveHeight = itemHeightPx(item) * scale;
  const top = wallAnchor
    ? (() => {
        const zDirection = unitVector(model.axisZ);
        return {
          x: anchor.x + zDirection.x * effectiveHeight,
          y: anchor.y + zDirection.y * effectiveHeight
        };
      })()
    : projectionWorldToScreen(model, {
        u: anchorWorld.u,
        v: anchorWorld.v,
        z: effectiveHeight / model.axisZLength
      });
  const lightPoint = lightEditPoint();
  const scaleCss = projectionGuideCanvasScale();
  const rays = wallAnchor ? [] : floorItemProjectionDiagnostics(item, bounds, lightPoint, scale, model);
  const receivers = wallAnchor
    ? wallItemProjectionReceivers(item, model)
    : [...new Map(rays.filter((ray) => ray.hit).map((ray) => [
        String(ray.hit.receiver.face.id),
        ray.hit.receiver
      ])).values()];
  const shadowActive = state.night.castShadows &&
    itemCastsShadow(item) &&
    itemShadowOpacity(item) > 0 &&
    itemShadowLength(item) > 0 &&
    shadowLightFactorForPoint(anchor.x, anchor.y, lightPoint, scale) > 0;

  if (shadowActive) drawProjectionReceiverHighlights(receivers);

  ctx.save();
  ctx.lineWidth = 1.5 * scaleCss;
  ctx.strokeStyle = PROJECTION_GUIDE_COLORS.z;
  ctx.setLineDash([5 * scaleCss, 3 * scaleCss]);
  ctx.beginPath();
  ctx.moveTo(anchor.x, anchor.y);
  ctx.lineTo(top.x, top.y);
  ctx.stroke();
  ctx.setLineDash([]);
  ctx.fillStyle = PROJECTION_GUIDE_COLORS.z;
  for (const point of [anchor, top]) {
    ctx.beginPath();
    ctx.arc(point.x, point.y, 3.2 * scaleCss, 0, Math.PI * 2);
    ctx.fill();
  }
  ctx.restore();

  if (shadowActive && !wallAnchor && rays.length) {
    const representative = rays.reduce((best, ray) =>
      !best || ray.heightPx > best.heightPx ? ray : best, null);
    if (representative) {
      if (activeShadowMode() !== "directional") {
        drawProjectionArrow(ctx, lightPoint, representative.casterPoint, "#fff1c2", { dashed: true, alpha: 0.42 });
      }
      if (representative.hit) {
        const color = PROJECTION_GUIDE_COLORS[representative.hit.receiver.surface] || "#ffffff";
        drawProjectionArrow(ctx, representative.casterPoint, representative.hit.screen, color, { dashed: true, alpha: 0.94 });
      }
    }
  }

  const receiverText = shadowActive
    ? (receivers.length ? receivers.map(projectionReceiverLabel).join(" + ") : "No receiver")
    : "Shadow inactive";
  const coordinateText = wallAnchor
    ? "Wall anchor"
    : `U ${anchorWorld.u.toFixed(2)} · V ${anchorWorld.v.toFixed(2)} · Z 0.00`;
  drawProjectionInfoPanel(ctx, [
    coordinateText,
    `Height ${Math.round(itemHeightPx(item))}px`,
    `${wallAnchor ? "Target" : "Hit"} ${receiverText}`
  ], {
    x: bounds.x + bounds.w + 10 * scaleCss,
    y: Math.min(anchor.y, top.y) - 4 * scaleCss
  });
}

function drawFullRoomProjectionGuides(model) {
  const axes = [
    { label: "Floor U", vector: model.axisU, color: PROJECTION_GUIDE_COLORS.u, offset: { x: -8, y: 5 } },
    { label: "Floor V", vector: model.axisV, color: PROJECTION_GUIDE_COLORS.v, offset: { x: 5, y: 5 } },
    { label: "Height Z", vector: model.axisZ, color: PROJECTION_GUIDE_COLORS.z, offset: { x: 5, y: -14 } }
  ];
  const scale = projectionGuideCanvasScale();
  ctx.save();
  for (const axis of axes) {
    const end = { x: model.origin.x + axis.vector.x, y: model.origin.y + axis.vector.y };
    drawProjectionArrow(ctx, model.origin, end, axis.color, { dashed: true, alpha: 0.78 });
    drawProjectionLabel(ctx, axis.label, end, axis.color, axis.offset);
  }
  ctx.fillStyle = "#ffffff";
  ctx.strokeStyle = "rgba(9, 12, 17, 0.86)";
  ctx.lineWidth = 2 * scale;
  ctx.beginPath();
  ctx.arc(model.origin.x, model.origin.y, 4 * scale, 0, Math.PI * 2);
  ctx.fill();
  ctx.stroke();
  drawProjectionLabel(ctx, "O", model.origin, "#ffffff", { x: -18, y: 5 });
  ctx.restore();
}

function drawProjectionGuidesOverlay() {
  const model = roomProjectionModel();
  updateProjectionGuideButton(model);
  if (!model) return;
  drawProjectionMiniGizmo(model);
  if (!projectionGuidesVisible()) return;
  drawFullRoomProjectionGuides(model);
  drawSelectedItemProjectionDiagnostics(model);
}

function drawUnifiedFloorItemShadow(
  targetCtx,
  item,
  bounds,
  lightPoint,
  scale,
  model
) {
  if (!model || itemUsesWallShadowAnchor(item)) return false;
  if (!itemCastsShadow(item) || !itemVisibleInMode(item) || itemRenderOpacity(item) <= 0) return true;
  if (itemShadowOpacity(item) <= 0 || itemShadowLength(item) <= 0) return true;
  const contour = itemProjectionContour(item);
  if (contour.length < 3) return true;
  const receivers = itemProjectionFaces(item, model);
  if (!receivers.length) return true;

  const footX = bounds.x + bounds.w * 0.5;
  const footY = bounds.y + bounds.h * 0.88;
  const lightFactor = shadowLightFactorForPoint(footX, footY, lightPoint, scale);
  if (lightFactor <= 0) return true;
  const projected = contour.map((point) => {
    const screenPoint = { x: bounds.x + point.x * bounds.w, y: bounds.y + point.y * bounds.h };
    const heightPx = localCasterHeight(item, point, scale);
    const casterWorld = screenPointToProjectionWorld(model, screenPoint, heightPx);
    const direction = projectionRayDirection(model, casterWorld, screenPoint, heightPx, lightPoint, scale);
    const hit = nearestProjectionReceiver(model, casterWorld, direction, receivers, scale);
    if (hit) return hit.screen;
    const floorHit = projectionPlaneIntersection(casterWorld, direction, "floor");
    return floorHit ? projectionWorldToScreen(model, floorHit) : screenPoint;
  });
  if (projected.length < 3) return true;

  const heightFactor = Math.max(0.25, Math.min(2.2, itemHeightPx(item) / 32));
  targetCtx.save();
  targetCtx.beginPath();
  for (const receiver of receivers) {
    if (receiver.points.length < 3) continue;
    targetCtx.moveTo(receiver.points[0].x, receiver.points[0].y);
    for (let index = 1; index < receiver.points.length; index += 1) {
      targetCtx.lineTo(receiver.points[index].x, receiver.points[index].y);
    }
    targetCtx.closePath();
  }
  targetCtx.clip();
  targetCtx.globalAlpha = baseShadowOpacity(item, lightFactor, heightFactor);
  targetCtx.globalCompositeOperation = "multiply";
  const blur = Math.max(0, itemShadowBlur(item) * scale);
  targetCtx.filter = blur > 0 ? `blur(${blur}px)` : "none";
  targetCtx.fillStyle = "#000000";
  targetCtx.beginPath();
  projected.forEach((point, index) => {
    if (index === 0) targetCtx.moveTo(point.x, point.y);
    else targetCtx.lineTo(point.x, point.y);
  });
  targetCtx.closePath();
  targetCtx.fill();
  targetCtx.restore();
  return true;
}

function faceProjectionBasis(points) {
  if (!points || points.length < 3) return null;
  const origin = points[0];
  const bottomLeft = points[1];
  const topRight = points[points.length - 1];
  const rawU = { x: topRight.x - origin.x, y: topRight.y - origin.y };
  const rawV = { x: bottomLeft.x - origin.x, y: bottomLeft.y - origin.y };
  const uLength = Math.hypot(rawU.x, rawU.y);
  const vLength = Math.hypot(rawV.x, rawV.y);
  if (uLength < 0.001 || vLength < 0.001) return null;
  const u = { x: rawU.x / uLength, y: rawU.y / uLength };
  const v = { x: rawV.x / vLength, y: rawV.y / vLength };
  const det = u.x * v.y - u.y * v.x;
  if (Math.abs(det) < 0.001) return null;
  return { origin, u, v, det, uLength, vLength };
}

function screenToFaceLocal(basis, point) {
  const dx = point.x - basis.origin.x;
  const dy = point.y - basis.origin.y;
  return {
    u: (dx * basis.v.y - dy * basis.v.x) / basis.det,
    v: (basis.u.x * dy - basis.u.y * dx) / basis.det
  };
}

function faceLocalToScreen(basis, point) {
  return {
    x: basis.origin.x + basis.u.x * point.u + basis.v.x * point.v,
    y: basis.origin.y + basis.u.y * point.u + basis.v.y * point.v
  };
}

function projectWallLocalPoint(point, light, lightDepth, casterDepth) {
  const safeDepth = Math.min(Math.max(0, casterDepth), Math.max(0, lightDepth - 1));
  if (safeDepth <= 0) return { ...point };
  const factor = lightDepth / Math.max(1, lightDepth - safeDepth);
  return {
    u: light.u + (point.u - light.u) * factor,
    v: light.v + (point.v - light.v) * factor
  };
}

function itemWallShadowDepthScaled(item, scale) {
  const depthScale = Math.max(0.2, itemShadowLength(item) / DEFAULT_NIGHT.shadowLength);
  return itemWallShadowDepthPx(item) * scale * depthScale;
}

function wallShadowOpacity(item, lightFactor) {
  const heightFactor = Math.max(0.25, Math.min(2.2, itemHeightPx(item) / 32));
  const modeAlpha = state.mode === "night" ? 1 : 0.42;
  const alpha = (itemShadowOpacity(item) / 100) *
    modeAlpha *
    lightFactor *
    Math.max(0, Math.min(1, item.opacity ?? 1)) *
    Math.min(1.15, 0.65 + heightFactor * 0.22) *
    0.94;
  return clampNumber(alpha, 0, 1);
}

function baseShadowOpacity(item, lightFactor, heightFactor, isWallSurface = false) {
  const modeAlpha = state.mode === "night" ? 1 : 0.42;
  const alpha = (itemShadowOpacity(item) / 100) *
    modeAlpha *
    lightFactor *
    Math.max(0, Math.min(1, item.opacity ?? 1)) *
    Math.min(1.15, 0.65 + heightFactor * 0.22) *
    (isWallSurface ? 0.92 : 1);
  return clampNumber(alpha, 0, 1);
}

function shadowHeightGain(item) {
  return itemShadowLength(item) / Math.max(1, DEFAULT_NIGHT.shadowLength);
}

function localCasterHeight(item, point, scale) {
  const verticalRatio = clampUnit(1 - point.y);
  return itemHeightPx(item) * scale * shadowHeightGain(item) * verticalRatio;
}

function projectElevatedPointToReceiver(screenPoint, z, lightPoint, scale) {
  if (z <= 0.001) return { ...screenPoint };
  if (activeShadowMode() === "directional") {
    const angle = angleToRad(activeLightSettings().lightAngle);
    return {
      x: screenPoint.x + Math.cos(angle) * z,
      y: screenPoint.y + Math.sin(angle) * z
    };
  }

  const lightZ = Math.max(z + 1, activeLightSettings().lightDepth * scale);
  const factor = lightZ / Math.max(1, lightZ - z);
  return {
    x: lightPoint.x + (screenPoint.x - lightPoint.x) * factor,
    y: lightPoint.y + (screenPoint.y - lightPoint.y) * factor
  };
}

function projectedShadowPolygon2D5(item, bounds, lightPoint, scale) {
  const polygon = normalizeLocalPolygon(item.shadowPolygon);
  if (polygon.length < 3) return [];
  return polygon.map((point) => {
    const screenPoint = {
      x: bounds.x + point.x * bounds.w,
      y: bounds.y + point.y * bounds.h
    };
    return projectElevatedPointToReceiver(
      screenPoint,
      localCasterHeight(item, point, scale),
      lightPoint,
      scale
    );
  });
}

function drawProjectedShadow2D5(targetCtx, item, bounds, lightPoint, scale, lightFactor, heightFactor, isWallSurface) {
  const points = projectedShadowPolygon2D5(item, bounds, lightPoint, scale);
  if (points.length < 3) return false;
  const alpha = baseShadowOpacity(item, lightFactor, heightFactor, isWallSurface);
  if (alpha <= 0) return true;
  targetCtx.save();
  targetCtx.globalAlpha = alpha;
  targetCtx.globalCompositeOperation = "multiply";
  const blur = Math.max(0, itemShadowBlur(item) * scale);
  targetCtx.filter = blur > 0 ? `blur(${blur}px)` : "none";
  targetCtx.fillStyle = "#000000";
  targetCtx.beginPath();
  points.forEach((point, index) => {
    if (index === 0) targetCtx.moveTo(point.x, point.y);
    else targetCtx.lineTo(point.x, point.y);
  });
  targetCtx.closePath();
  targetCtx.fill();
  targetCtx.restore();
  return true;
}

function drawFloorFootprintShadow(targetCtx, item, bounds, ux, uy, lightFactor, scale, heightFactor) {
  const footprint = itemFootprint(item);
  if (footprint === "none") return true;
  const polygon = footprint === "rect" ? [
    { x: 0.12, y: 0.72 },
    { x: 0.88, y: 0.72 },
    { x: 0.88, y: 0.96 },
    { x: 0.12, y: 0.96 }
  ] : footprint === "polygon" ? effectiveFootprintPolygon(item) : [];
  if (polygon.length) {
    const shadowLength = itemShadowLength(item) * scale * heightFactor;
    const offsetX = ux * shadowLength * 0.72;
    const offsetY = uy * shadowLength * 0.32;
    const alpha = baseShadowOpacity(item, lightFactor, heightFactor);
    if (alpha <= 0) return true;
    targetCtx.save();
    targetCtx.globalAlpha = alpha;
    targetCtx.globalCompositeOperation = "multiply";
    const blur = itemShadowBlur(item);
    targetCtx.filter = blur > 0 ? `blur(${Math.max(0.2 * scale, blur * scale)}px)` : "none";
    targetCtx.fillStyle = "#000000";
    targetCtx.beginPath();
    polygon.forEach((point, index) => {
      const x = bounds.x + point.x * bounds.w + offsetX;
      const y = bounds.y + point.y * bounds.h + offsetY;
      if (index === 0) targetCtx.moveTo(x, y);
      else targetCtx.lineTo(x, y);
    });
    targetCtx.closePath();
    targetCtx.fill();
    targetCtx.restore();
    return true;
  }
  if (footprint === "polygon" || footprint === "rect") return true;
  if (footprint !== "ellipse") return false;
  const shadowLength = itemShadowLength(item) * scale * heightFactor;
  const footX = bounds.x + bounds.w * 0.5;
  const footY = bounds.y + bounds.h * 0.88;
  const shadowWidth = Math.max(1, bounds.w * (0.72 + Math.min(0.22, itemShadowLength(item) / 220) * heightFactor));
  const shadowHeight = Math.max(1, bounds.h * (0.16 + Math.min(0.12, itemHeightPx(item) / 360)));
  const centerX = footX + ux * shadowLength * 0.72;
  const centerY = footY + uy * shadowLength * 0.32;
  const blur = Math.max(0.2 * scale, itemShadowBlur(item) * scale);

  targetCtx.save();
  targetCtx.globalAlpha = baseShadowOpacity(item, lightFactor, heightFactor);
  targetCtx.globalCompositeOperation = "multiply";
  targetCtx.filter = blur > 0 ? `blur(${blur}px)` : "none";
  targetCtx.translate(centerX, centerY);
  targetCtx.rotate(Math.atan2(uy, ux) * 0.04);
  targetCtx.fillStyle = "#000000";
  targetCtx.beginPath();
  targetCtx.ellipse(0, 0, shadowWidth * 0.5, shadowHeight * 0.5, 0, 0, Math.PI * 2);
  targetCtx.fill();
  targetCtx.restore();
  return true;
}

function drawWallPlaneShadow(targetCtx, item, bounds, lightPoint, scale, facePoints) {
  const basis = faceProjectionBasis(facePoints);
  const mask = itemShadowMask(item);
  if (!basis || !mask) return false;
  const lightSettings = activeLightSettings();
  const center = { x: bounds.x + bounds.w * 0.5, y: bounds.y + bounds.h * 0.5 };
  const lightFactor = shadowLightFactorForPoint(center.x, center.y, lightPoint, scale);
  if (lightFactor <= 0) return true;

  const lightLocal = screenToFaceLocal(basis, lightPoint);
  const lightDepth = Math.max(2, lightSettings.lightDepth * scale);
  const casterDepth = Math.min(itemWallShadowDepthScaled(item, scale), lightDepth - 1);
  if (casterDepth <= 0) return false;

  const offsetV = itemWallShadowOffsetY(item) * scale;
  const cornerLocal = [
    screenToFaceLocal(basis, { x: bounds.x, y: bounds.y }),
    screenToFaceLocal(basis, { x: bounds.x + bounds.w, y: bounds.y }),
    screenToFaceLocal(basis, { x: bounds.x, y: bounds.y + bounds.h })
  ].map((point) => ({ u: point.u, v: point.v + offsetV }));
  const projected = cornerLocal
    .map((point) => projectWallLocalPoint(point, lightLocal, lightDepth, casterDepth))
    .map((point) => faceLocalToScreen(basis, point));

  targetCtx.save();
  targetCtx.globalCompositeOperation = "multiply";
  targetCtx.globalAlpha = wallShadowOpacity(item, lightFactor);
  const blur = itemShadowBlur(item);
  targetCtx.filter = blur > 0 ? `blur(${blur * scale}px)` : "none";
  targetCtx.transform(
    (projected[1].x - projected[0].x) / mask.width,
    (projected[1].y - projected[0].y) / mask.width,
    (projected[2].x - projected[0].x) / mask.height,
    (projected[2].y - projected[0].y) / mask.height,
    projected[0].x,
    projected[0].y
  );
  targetCtx.drawImage(mask, 0, 0);
  targetCtx.restore();
  return true;
}

function drawProjectedShadow(targetCtx, item, bounds, lightPoint, scale = 1, surface = "floor", facePoints = null, face = null) {
  if (!state.night.castShadows || itemShadowOpacity(item) <= 0 || itemShadowLength(item) <= 0) return;
  if (!itemCastsShadow(item)) return;
  if (!itemVisibleInMode(item) || item.opacity <= 0 || bounds.w <= 0 || bounds.h <= 0) return;
  const isWallSurface = surface === "left_wall" || surface === "right_wall";
  if (!itemShouldCastShadowOnSurface(item, surface, face)) return;
  const mask = itemShadowMask(item);
  if (!mask) return;

  const footX = bounds.x + bounds.w * 0.5;
  const footY = bounds.y + bounds.h * 0.88;
  const { ux, uy } = shadowDirectionForPoint(footX, footY, lightPoint);
  const lightFactor = shadowLightFactorForPoint(footX, footY, lightPoint, scale);
  if (lightFactor <= 0) return;
  const heightFactor = Math.max(0.25, Math.min(2.2, itemHeightPx(item) / 32));
  const shadowLength = itemShadowLength(item) * scale * heightFactor;
  const usesWallAnchor = isWallSurface && itemUsesWallShadowAnchor(item);
  if (usesWallAnchor && facePoints && drawWallPlaneShadow(targetCtx, item, bounds, lightPoint, scale, facePoints)) {
    return;
  }
  if ((!isWallSurface || usesWallAnchor) &&
      drawProjectedShadow2D5(targetCtx, item, bounds, lightPoint, scale, lightFactor, heightFactor, isWallSurface)) {
    return;
  }
  if (!isWallSurface && drawFloorFootprintShadow(targetCtx, item, bounds, ux, uy, lightFactor, scale, heightFactor)) {
    return;
  }
  const shadowWidth = isWallSurface ?
    bounds.w * (0.72 + Math.min(0.28, heightFactor * 0.12)) :
    bounds.w * (1 + Math.min(0.55, itemShadowLength(item) / 180) * heightFactor);
  const shadowHeight = isWallSurface ?
    Math.max(1, bounds.h * (0.52 + Math.min(0.32, itemHeightPx(item) / 200))) :
    Math.max(1, bounds.h * (0.22 + Math.min(0.18, itemHeightPx(item) / 320)));
  const wallAnchorY = usesWallAnchor ? bounds.y + bounds.h * 0.86 : bounds.y + bounds.h * 0.34;
  const wallShadowLift = usesWallAnchor ? shadowHeight * 0.08 : shadowHeight * 0.36 + itemHeightPx(item) * scale * 0.22;
  const wallLightShift = uy * shadowLength * (usesWallAnchor ? 0.68 : 0.28);
  const wallOffsetY = usesWallAnchor ? itemWallShadowOffsetY(item) * scale : 0;
  const destX = isWallSurface ?
    footX - shadowWidth * 0.5 + ux * shadowLength * 0.82 :
    footX - shadowWidth * 0.5 + ux * shadowLength;
  const destY = isWallSurface ?
    wallAnchorY - wallShadowLift + wallLightShift + wallOffsetY :
    footY - shadowHeight * 0.5 + uy * shadowLength * 0.45;
  const blur = itemShadowBlur(item) * scale;

  targetCtx.save();
  targetCtx.globalAlpha = baseShadowOpacity(item, lightFactor, heightFactor, isWallSurface);
  targetCtx.globalCompositeOperation = "multiply";
  targetCtx.filter = blur > 0 ? `blur(${blur}px)` : "none";
  targetCtx.translate(destX + shadowWidth * 0.5, destY + shadowHeight * 0.5);
  if (isWallSurface) {
    targetCtx.transform(1, 0, wallShadowSkew(surface), 1, 0, 0);
    targetCtx.rotate(Math.atan2(uy, ux) * 0.05);
  } else {
    targetCtx.rotate(Math.atan2(uy, ux) * 0.12);
  }
  targetCtx.drawImage(mask, -shadowWidth * 0.5, -shadowHeight * 0.5, shadowWidth, shadowHeight);
  targetCtx.restore();
}

function drawItemShadows(
  targetCtx,
  rectForItem,
  lightPoint,
  scale = 1,
  pointsForFace = (face) => face.points,
  items = sortedItems()
) {
  const faces = receivingShadowFaces();
  const projectionModel = roomProjectionModel(pointsForFace);
  const unifiedItems = new Set();
  for (const item of items) {
    if (drawUnifiedFloorItemShadow(
      targetCtx,
      item,
      rectForItem(item),
      lightPoint,
      scale,
      projectionModel
    )) {
      unifiedItems.add(item);
    }
  }
  if (!faces.length) {
    const hasRoomFaces = state.faces.some((face) => !isSemanticAreaFace(face) && face.points.length >= 3);
    if (hasRoomFaces) return;
    for (const item of items) {
      if (unifiedItems.has(item)) continue;
      drawProjectedShadow(targetCtx, item, rectForItem(item), lightPoint, scale, "floor");
    }
    return;
  }

  for (const face of faces) {
    const points = pointsForFace(face);
    if (!points || points.length < 3) continue;
    targetCtx.save();
    drawPolygonPath(targetCtx, points);
    targetCtx.clip();
    for (const item of items) {
      if (unifiedItems.has(item)) continue;
      drawProjectedShadow(targetCtx, item, rectForItem(item), lightPoint, scale, shadowSurfaceForFace(face), points, face);
    }
    targetCtx.restore();
  }
}

function createItemShadowLayers(targetCtx, rectForItem, lightPoint, scale = 1, pointsForFace = (face) => face.points) {
  if (!state.night.castShadows) return [];
  const casters = sortedItems().filter((item) =>
    itemVisibleInMode(item) &&
    itemCastsShadow(item) &&
    itemRenderOpacity(item) > 0 &&
    itemShadowOpacity(item) > 0 &&
    itemShadowLength(item) > 0
  );
  return casters.map((item) => {
    const layer = document.createElement("canvas");
    layer.width = targetCtx.canvas.width;
    layer.height = targetCtx.canvas.height;
    drawItemShadows(layer.getContext("2d"), rectForItem, lightPoint, scale, pointsForFace, [item]);
    return { item, layer };
  });
}

function drawShadowLayers(targetCtx, shadowLayers) {
  targetCtx.save();
  targetCtx.globalAlpha = 1;
  targetCtx.globalCompositeOperation = "source-over";
  for (const { layer } of shadowLayers) {
    targetCtx.drawImage(layer, 0, 0);
  }
  targetCtx.restore();
}

function drawBase() {
  ctx.clearRect(0, 0, state.editWidth, state.editHeight);
  ctx.fillStyle = "#111318";
  ctx.fillRect(0, 0, state.editWidth, state.editHeight);

  const layers = backgroundLayersForMode();
  const fallback = layers.length ? null : backgroundForMode();
  if (!layers.length && !fallback) {
    ctx.fillStyle = "#202734";
    ctx.fillRect(0, 0, state.editWidth, state.editHeight);
    ctx.strokeStyle = "#526070";
    ctx.strokeRect(0.5, 0.5, state.editWidth - 1, state.editHeight - 1);
    ctx.fillStyle = "#a8b0bf";
    ctx.font = "10px sans-serif";
    ctx.fillText("Load reference image", 12, 24);
    return;
  }

  const prepared = preparedRoomMetrics();
  const edit = editRoomMetrics(prepared);
  ctx.imageSmoothingEnabled = false;
  ctx.globalAlpha = state.baseOpacity;
  for (const background of layers.length ? layers : [fallback]) {
    ctx.drawImage(
      background.img,
      prepared.trim.x,
      prepared.trim.y,
      prepared.trim.width,
      prepared.trim.height,
      0,
      edit.y,
      edit.width,
      edit.height
    );
  }
  ctx.globalAlpha = 1;
}

function drawPreviewBackground() {
  previewCtx.clearRect(0, 0, state.width, state.height);
  previewCtx.fillStyle = "#05070c";
  previewCtx.fillRect(0, 0, state.width, state.height);

  const layers = backgroundLayersForMode();
  const fallback = layers.length ? null : backgroundForMode();
  if (!layers.length && !fallback) {
    renderRoomGeometry(previewCtx, state.width, state.height);
    return;
  }

  const prepared = preparedRoomMetrics();
  previewCtx.save();
  previewCtx.imageSmoothingEnabled = false;
  for (const background of layers.length ? layers : [fallback]) {
    previewCtx.drawImage(
      background.img,
      prepared.trim.x,
      prepared.trim.y,
      prepared.trim.width,
      prepared.trim.height,
      0,
      prepared.y,
      prepared.width,
      prepared.height
    );
  }
  previewCtx.restore();
}

function itemRenderOpacity(item) {
  return clampNumber(Number(item?.opacity), 1, 0, 1);
}

function drawReceiverShadows(
  targetCtx,
  receiver,
  receiverBounds,
  shadowLayers,
  receiverLayer
) {
  if (receiver.receivesShadow === false || itemRenderOpacity(receiver) <= 0 || !state.night.castShadows) return;
  const applicable = shadowLayers.filter(({ item }) => item !== receiver);
  if (!applicable.length) return;

  const layerCtx = receiverLayer.getContext("2d");
  layerCtx.clearRect(0, 0, receiverLayer.width, receiverLayer.height);
  layerCtx.save();
  layerCtx.globalAlpha = 1;
  layerCtx.globalCompositeOperation = "source-over";
  for (const { layer } of applicable) {
    layerCtx.drawImage(layer, 0, 0);
  }
  layerCtx.restore();

  layerCtx.save();
  layerCtx.globalCompositeOperation = "destination-in";
  layerCtx.globalAlpha = itemRenderOpacity(receiver);
  layerCtx.imageSmoothingEnabled = false;
  layerCtx.drawImage(receiver.img, receiverBounds.x, receiverBounds.y, receiverBounds.w, receiverBounds.h);
  layerCtx.restore();

  targetCtx.save();
  targetCtx.globalAlpha = 1;
  targetCtx.globalCompositeOperation = "source-over";
  targetCtx.drawImage(receiverLayer, 0, 0);
  targetCtx.restore();
}

function drawSceneItems(targetCtx, rectForItem, shadowLayers) {
  const items = sortedItems().filter((item) => itemVisibleInMode(item));
  const receiverLayer = document.createElement("canvas");
  receiverLayer.width = targetCtx.canvas.width;
  receiverLayer.height = targetCtx.canvas.height;
  for (const item of items) {
    const bounds = rectForItem(item);
    targetCtx.save();
    targetCtx.globalAlpha = itemRenderOpacity(item);
    targetCtx.imageSmoothingEnabled = false;
    targetCtx.drawImage(item.img, bounds.x, bounds.y, bounds.w, bounds.h);
    targetCtx.restore();
    drawReceiverShadows(targetCtx, item, bounds, shadowLayers, receiverLayer);
  }
}

function faceFillColor(type, alpha = 0.35) {
  const normalized = normalizeFaceType(type);
  if (normalized === "wall") return `rgba(120, 214, 238, ${alpha})`;
  if (normalized === FACE_TYPE_SPRITE_AREA) return `rgba(112, 231, 157, ${alpha})`;
  if (normalized === FACE_TYPE_DOORWAY) return `rgba(255, 111, 97, ${alpha})`;
  return `rgba(242, 189, 114, ${alpha})`;
}

function faceStrokeColor(type) {
  const normalized = normalizeFaceType(type);
  if (normalized === "wall") return "#78d6ee";
  if (normalized === FACE_TYPE_SPRITE_AREA) return "#70e79d";
  if (normalized === FACE_TYPE_DOORWAY) return "#ff6f61";
  return "#f2bd72";
}

function drawPolygonPath(targetCtx, points) {
  if (!points.length) return;
  targetCtx.beginPath();
  targetCtx.moveTo(points[0].x, points[0].y);
  for (let i = 1; i < points.length; i++) {
    targetCtx.lineTo(points[i].x, points[i].y);
  }
  targetCtx.closePath();
}

function drawPointHandle(point, selected) {
  const radius = editHandleRadius();
  ctx.beginPath();
  ctx.fillStyle = selected ? "#ffffff" : "#111318";
  ctx.strokeStyle = selected ? "#ffffff" : "#78d6ee";
  ctx.lineWidth = selected ? 2 : 1;
  ctx.arc(point.x, point.y, radius, 0, Math.PI * 2);
  ctx.fill();
  ctx.stroke();
}

function drawFacesOverlay() {
  ctx.save();
  for (const face of state.faces) {
    if (!faceVisible(face)) continue;
    if (face.points.length < 3) continue;
    drawPolygonPath(ctx, face.points);
    ctx.fillStyle = faceFillColor(face.type, face.id === state.selectedFaceId ? 0.42 : 0.24);
    ctx.fill();
    ctx.strokeStyle = face.id === state.selectedFaceId ? "#ffffff" : faceStrokeColor(face.type);
    ctx.lineWidth = face.id === state.selectedFaceId ? 2 : 1;
    if (isDoorwayFace(face)) ctx.setLineDash([8, 4]);
    ctx.stroke();
    ctx.setLineDash([]);
  }

  if (state.draftPoints.length) {
    ctx.strokeStyle = "#ffffff";
    ctx.fillStyle = "#ffffff";
    ctx.lineWidth = 1;
    ctx.setLineDash([3, 2]);
    ctx.beginPath();
    ctx.moveTo(state.draftPoints[0].x, state.draftPoints[0].y);
    for (let i = 1; i < state.draftPoints.length; i++) {
      ctx.lineTo(state.draftPoints[i].x, state.draftPoints[i].y);
    }
    ctx.stroke();
    ctx.setLineDash([]);
    const first = state.draftPoints[0];
    ctx.strokeStyle = "#78d6ee";
    ctx.strokeRect(first.x - 4, first.y - 4, 8, 8);
  }
  const selected = selectedFace();
  const visiblePointIds = new Set([
    ...state.draftPoints.map((point) => point.id),
    ...(state.drawingFace
      ? state.faces.filter(faceVisible).flatMap((face) => face.points.map((point) => point.id))
      : (selected && faceVisible(selected) ? selected.points.map((point) => point.id) : []))
  ]);
  for (const point of state.points) {
    if (!visiblePointIds.has(point.id)) continue;
    drawPointHandle(point, point.id === state.selectedPointId);
  }
  ctx.restore();
}

function renderRoomGeometry(targetCtx, width, height) {
  targetCtx.clearRect(0, 0, width, height);
  targetCtx.fillStyle = "#05070c";
  targetCtx.fillRect(0, 0, width, height);
  for (const face of state.faces) {
    if (!faceVisible(face)) continue;
    if (face.points.length < 3) continue;
    if (isSemanticAreaFace(face)) continue;
    drawPolygonPath(targetCtx, faceTargetPoints(face));
    targetCtx.fillStyle = face.type === "wall" ? "#b6916c" : "#d99752";
    targetCtx.fill();
    targetCtx.strokeStyle = "#4e352a";
    targetCtx.lineWidth = 1;
    targetCtx.stroke();
  }
}

function renderPreview() {
  if (previewCanvas.width !== state.width || previewCanvas.height !== state.height) {
    previewCanvas.width = state.width;
    previewCanvas.height = state.height;
  }
  const lightPoint = activeLightPoint();
  drawPreviewBackground();
  const shadowLayers = createItemShadowLayers(previewCtx, itemBounds, lightPoint, 1, faceTargetPoints);
  drawShadowLayers(previewCtx, shadowLayers);
  drawSceneItems(previewCtx, itemBounds, shadowLayers);

  drawLightingOverlay(previewCtx, state.width, state.height, lightPoint, 1);
  drawScreenFrame(previewCtx);
}

function angleToRad(angleDeg) {
  return (Number(angleDeg) || 0) * Math.PI / 180;
}

function angleDeltaRad(a, b) {
  return ((a - b + Math.PI) % (Math.PI * 2)) - Math.PI;
}

function fillLightShapePath(targetCtx, lightPoint, radius, angleDeg = 0, spreadDeg = 45, shapeValue = "radial") {
  const shape = normalizeLightShape(shapeValue);
  targetCtx.beginPath();
  if (shape === "cone") {
    const angle = angleToRad(angleDeg);
    const half = angleToRad(spreadDeg) * 0.5;
    targetCtx.moveTo(lightPoint.x, lightPoint.y);
    targetCtx.arc(lightPoint.x, lightPoint.y, radius, angle - half, angle + half);
    targetCtx.closePath();
  } else {
    targetCtx.arc(lightPoint.x, lightPoint.y, radius, 0, Math.PI * 2);
  }
}

function drawLightShape(targetCtx, width, height, lightPoint, radiusScale) {
  const light = activeLightSettings();
  const strength = clampNumber(Number(light.lightStrength) || 0, 0, 200) / 100;
  if (strength <= 0) return;

  const radius = Math.max(1, light.lightRadius * radiusScale);
  const alpha = Math.min(1, strength * (state.mode === "night" ? 0.72 : 0.42));
  const gradient = targetCtx.createRadialGradient(
    lightPoint.x,
    lightPoint.y,
    Math.max(1, radius * 0.04),
    lightPoint.x,
    lightPoint.y,
    radius
  );
  gradient.addColorStop(0, `rgba(255, 255, 238, ${alpha})`);
  gradient.addColorStop(0.52, `rgba(255, 248, 214, ${alpha * 0.24})`);
  gradient.addColorStop(1, "rgba(255, 248, 214, 0)");

  targetCtx.save();
  fillLightShapePath(targetCtx, lightPoint, radius, light.lightAngle, light.lightSpread, light.lightShape);
  targetCtx.clip();
  targetCtx.globalCompositeOperation = "screen";
  targetCtx.fillStyle = gradient;
  targetCtx.fillRect(0, 0, width, height);
  targetCtx.restore();
}

function drawLightingOverlay(targetCtx, width, height, lightPoint, radiusScale) {
  drawLightShape(targetCtx, width, height, lightPoint, radiusScale);
}

function drawScreenFrame(targetCtx, toEdit = false) {
  const frame = screenFrameRect();
  const rect = toEdit ? targetRectToEdit(frame.x, frame.y, frame.w, frame.h) : frame;
  const x = Math.round(rect.x) + 0.5;
  const y = Math.round(rect.y) + 0.5;
  const w = Math.max(1, Math.round(rect.w) - 1);
  const h = Math.max(1, Math.round(rect.h) - 1);

  targetCtx.save();
  targetCtx.strokeStyle = "rgba(255, 255, 255, 0.94)";
  targetCtx.lineWidth = 1;
  targetCtx.setLineDash([4, 3]);
  targetCtx.strokeRect(x, y, w, h);
  targetCtx.setLineDash([]);
  targetCtx.strokeStyle = "rgba(120, 214, 238, 0.92)";
  targetCtx.strokeRect(x + 1, y + 1, Math.max(1, w - 2), Math.max(1, h - 2));
  if (rect.w >= 64 && rect.h >= 18) {
    targetCtx.fillStyle = "rgba(5, 7, 12, 0.62)";
    targetCtx.fillRect(x + 3, y + 3, 55, 11);
    targetCtx.fillStyle = "rgba(255, 255, 255, 0.94)";
    targetCtx.font = "7px sans-serif";
    targetCtx.fillText(`${GAME_SCREEN_WIDTH}x${GAME_SCREEN_HEIGHT}`, x + 6, y + 11);
  }
  targetCtx.restore();
}

function drawEditorLightOverlay() {
  drawLightingOverlay(
    ctx,
    state.editWidth,
    state.editHeight,
    targetToEditPoint(activeLightPoint()),
    Math.max(editScaleX(), editScaleY())
  );
}

function lightEditPoint() {
  return targetToEditPoint(editLightPointTarget());
}

function coneControlDistanceRange() {
  const scale = Math.max(editScaleX(), editScaleY());
  return {
    min: 18 * scale,
    max: 90 * scale
  };
}

function coneControlDistance() {
  const range = coneControlDistanceRange();
  const t = (editLightSettings().lightSpread - 5) / (160 - 5);
  return range.min + Math.max(0, Math.min(1, t)) * (range.max - range.min);
}

function coneControlPoint() {
  const origin = lightEditPoint();
  const angle = angleToRad(editLightSettings().lightAngle);
  const dist = coneControlDistance();
  return {
    x: origin.x + Math.cos(angle) * dist,
    y: origin.y + Math.sin(angle) * dist
  };
}

function updateConeControlFromCanvasPoint(point) {
  const origin = lightEditPoint();
  const dx = point.x - origin.x;
  const dy = point.y - origin.y;
  const dist = Math.max(0.001, Math.hypot(dx, dy));
  const angle = (Math.atan2(dy, dx) * 180 / Math.PI + 360) % 360;
  const range = coneControlDistanceRange();
  const t = Math.max(0, Math.min(1, (dist - range.min) / Math.max(1, range.max - range.min)));
  const spread = 5 + t * (160 - 5);
  const light = editLightSettings();
  light.lightAngle = Math.round(angle);
  light.lightSpread = Math.round(spread);
  document.getElementById("lightAngle").value = light.lightAngle;
  document.getElementById("lightSpread").value = light.lightSpread;
}

function drawLightHandle() {
  const point = lightEditPoint();
  const radius = LIGHT_HANDLE_RADIUS;
  const scale = Math.max(editScaleX(), editScaleY());
  const light = editLightSettings();
  const lightRadius = Math.max(1, light.lightRadius * scale);
  const angle = angleToRad(light.lightAngle);

  ctx.save();
  ctx.strokeStyle = state.mode === "night" ? "rgba(242, 189, 114, 0.48)" : "rgba(255, 244, 194, 0.58)";
  ctx.lineWidth = 1;
  ctx.setLineDash([4, 4]);
  fillLightShapePath(ctx, point, lightRadius, light.lightAngle, light.lightSpread, light.lightShape);
  ctx.stroke();
  ctx.setLineDash([]);
  if (normalizeLightShape(light.lightShape) === "cone") {
    ctx.strokeStyle = "rgba(255, 241, 194, 0.86)";
    ctx.beginPath();
    ctx.moveTo(point.x, point.y);
    ctx.lineTo(point.x + Math.cos(angle) * Math.min(lightRadius, 42), point.y + Math.sin(angle) * Math.min(lightRadius, 42));
    ctx.stroke();
    if (state.lightSelected) {
      const control = coneControlPoint();
      ctx.strokeStyle = "rgba(120, 214, 238, 0.86)";
      ctx.fillStyle = "rgba(120, 214, 238, 0.16)";
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(point.x, point.y);
      ctx.lineTo(control.x, control.y);
      ctx.stroke();
      ctx.beginPath();
      ctx.arc(control.x, control.y, LIGHT_HANDLE_RADIUS - 1, 0, Math.PI * 2);
      ctx.fill();
      ctx.strokeStyle = "#d9f8ff";
      ctx.stroke();
    }
  }
  ctx.fillStyle = "#f2bd72";
  ctx.strokeStyle = "#111318";
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.arc(point.x, point.y, radius, 0, Math.PI * 2);
  ctx.fill();
  ctx.stroke();
  ctx.strokeStyle = "#fff1c2";
  ctx.lineWidth = 1;
  ctx.beginPath();
  ctx.moveTo(point.x - radius - 4, point.y);
  ctx.lineTo(point.x + radius + 4, point.y);
  ctx.moveTo(point.x, point.y - radius - 4);
  ctx.lineTo(point.x, point.y + radius + 4);
  ctx.stroke();
  ctx.fillStyle = "rgba(17, 19, 24, 0.78)";
  ctx.font = "9px sans-serif";
  const profileText = currentLightProfile() === "night" ? "Night" : "Day";
  const labelW = Math.ceil(ctx.measureText(profileText).width + 12);
  ctx.fillRect(point.x + 9, point.y - 11, labelW, 13);
  ctx.fillStyle = "#fff1c2";
  ctx.fillText(profileText, point.x + 12, point.y - 2);
  ctx.restore();
}

function drawGrid() {
  if (!state.showGrid) return;
  ctx.save();
  ctx.lineWidth = 1;
  const step = 8;
  const alpha = clampNumber(state.gridOpacity, 0, 1);
  for (let x = 0; x <= state.width; x += step) {
    const editX = x * editScaleX();
    ctx.strokeStyle = `rgba(255, 255, 255, ${x % 32 === 0 ? Math.min(1, alpha * 1.55) : alpha})`;
    ctx.beginPath();
    ctx.moveTo(editX + 0.5, 0);
    ctx.lineTo(editX + 0.5, state.editHeight);
    ctx.stroke();
  }
  for (let y = 0; y <= state.height; y += step) {
    const editY = y * editScaleY();
    ctx.strokeStyle = `rgba(255, 255, 255, ${y % 32 === 0 ? Math.min(1, alpha * 1.55) : alpha})`;
    ctx.beginPath();
    ctx.moveTo(0, editY + 0.5);
    ctx.lineTo(state.editWidth, editY + 0.5);
    ctx.stroke();
  }
  ctx.restore();
}

function drawGuides() {
  if (!state.guides.show) return;
  ctx.save();
  ctx.lineWidth = 1;
  ctx.setLineDash([3, 2]);

  ctx.strokeStyle = "rgba(255, 190, 110, 0.9)";
  const hudGuide = targetRectToEdit(
    state.guides.hudX,
    state.guides.hudY,
    state.guides.hudW,
    state.guides.hudH
  );
  ctx.strokeRect(
    hudGuide.x + 0.5,
    hudGuide.y + 0.5,
    hudGuide.w,
    hudGuide.h
  );

  ctx.setLineDash([]);
  ctx.fillStyle = "rgba(255, 190, 110, 0.9)";
  ctx.font = "6px sans-serif";
  ctx.fillText("hud", hudGuide.x, hudGuide.y + hudGuide.h + 7);
  ctx.restore();
}

function drawSelection() {
  const item = selectedItem();
  if (!item) return;
  const b = itemBounds(item);
  const editBounds = targetRectToEdit(b.x, b.y, b.w, b.h);
  ctx.save();
  ctx.strokeStyle = "#78d6ee";
  ctx.lineWidth = 1;
  ctx.setLineDash([2, 2]);
  ctx.strokeRect(Math.round(editBounds.x) + 0.5, Math.round(editBounds.y) + 0.5, Math.round(editBounds.w), Math.round(editBounds.h));
  const advancedOpen = document.getElementById("shadowAdvancedGroup")?.open === true;
  if (advancedOpen || isItemPolygonDraftActive()) {
    const key = isItemPolygonDraftActive()
      ? state.itemPolygonDraft.key
      : state.activeItemPolygonKey;
    if (key === "shadowPolygon") {
      drawItemLocalPolygonOverlay(item, "shadowPolygon", "rgba(205, 154, 255, 0.82)", "rgba(205, 154, 255, 0.10)");
    } else {
      drawItemLocalPolygonOverlay(item, "footprintPolygon", "rgba(112, 231, 157, 0.82)", "rgba(112, 231, 157, 0.12)");
    }
    drawItemPolygonDraftOverlay(item);
  }
  ctx.restore();
}

function drawItemLocalPolygonOverlay(item, key, stroke, fill) {
  const points = polygonDisplayPoints(item, key);
  if (!points.length) return;
  const editPoints = itemPolygonEditPoints(item, key);
  const active = state.activeItemPolygonKey === key;
  ctx.beginPath();
  editPoints.forEach((point, index) => {
    if (index === 0) ctx.moveTo(point.x, point.y);
    else ctx.lineTo(point.x, point.y);
  });
  ctx.closePath();
  ctx.fillStyle = fill;
  ctx.strokeStyle = stroke;
  ctx.lineWidth = active ? 2 : 1;
  ctx.setLineDash([4, 2]);
  ctx.fill();
  ctx.stroke();
  ctx.setLineDash([]);
  editPoints.forEach((point, index) => {
    const radius = active ? 5 : 4;
    ctx.beginPath();
    ctx.arc(point.x, point.y, radius, 0, Math.PI * 2);
    ctx.fillStyle = active ? "#ffffff" : "rgba(255, 255, 255, 0.86)";
    ctx.fill();
    ctx.strokeStyle = stroke;
    ctx.lineWidth = 1.5;
    ctx.stroke();
    if (active) {
      ctx.fillStyle = "#11151d";
      ctx.font = "7px sans-serif";
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";
      ctx.fillText(String(index + 1), point.x, point.y + 0.5);
    }
  });
}

function drawItemPolygonDraftOverlay(item) {
  if (!item || !isItemPolygonDraftActive() || state.itemPolygonDraft.points.length === 0) return;
  const key = state.itemPolygonDraft.key;
  const points = state.itemPolygonDraft.points;
  const stroke = key === "shadowPolygon" ? "rgba(205, 154, 255, 0.96)" : "rgba(112, 231, 157, 0.96)";
  const fill = key === "shadowPolygon" ? "rgba(205, 154, 255, 0.12)" : "rgba(112, 231, 157, 0.14)";
  ctx.save();
  ctx.beginPath();
  points.forEach((point, index) => {
    if (index === 0) ctx.moveTo(point.x, point.y);
    else ctx.lineTo(point.x, point.y);
  });
  if (points.length >= 3) {
    ctx.closePath();
    ctx.fillStyle = fill;
    ctx.fill();
  }
  ctx.strokeStyle = stroke;
  ctx.lineWidth = 2;
  ctx.setLineDash([5, 3]);
  ctx.stroke();
  ctx.setLineDash([]);
  points.forEach((point, index) => {
    const radius = index === 0 ? 6 : 5;
    const active = state.itemPolygonDraft.dragging && state.itemPolygonDraft.dragIndex === index;
    ctx.beginPath();
    if (index === 0) {
      ctx.rect(point.x - radius, point.y - radius, radius * 2, radius * 2);
    } else {
      ctx.arc(point.x, point.y, radius, 0, Math.PI * 2);
    }
    ctx.fillStyle = active ? "#fff1c2" : "#ffffff";
    ctx.fill();
    ctx.strokeStyle = stroke;
    ctx.lineWidth = active ? 2.2 : 1.5;
    ctx.stroke();
    ctx.fillStyle = "#11151d";
    ctx.font = "7px sans-serif";
    ctx.textAlign = "center";
    ctx.textBaseline = "middle";
    ctx.fillText(String(index + 1), point.x, point.y + 0.5);
  });
  ctx.restore();
}

function drawMeasureOverlay() {
  const measurement = currentMeasurement();
  if (!measurement) return;

  const x = Math.round(measurement.start.x) + 0.5;
  const y1 = Math.round(measurement.start.y) + 0.5;
  const y2 = Math.round(measurement.end.y) + 0.5;
  const top = Math.round(measurement.top) + 0.5;
  const bottom = Math.round(measurement.bottom) + 0.5;
  const label = `H ${formatPixelValue(measurement.canvasHeight)} / game ${formatPixelValue(measurement.gameHeight)}`;

  ctx.save();
  ctx.lineWidth = 1;
  ctx.setLineDash([4, 3]);
  ctx.strokeStyle = "rgba(255, 236, 139, 0.42)";
  ctx.beginPath();
  ctx.moveTo(0.5, top);
  ctx.lineTo(state.editWidth + 0.5, top);
  ctx.moveTo(0.5, bottom);
  ctx.lineTo(state.editWidth + 0.5, bottom);
  ctx.stroke();

  ctx.setLineDash([]);
  ctx.strokeStyle = "#ffec8b";
  ctx.fillStyle = "#ffec8b";
  ctx.beginPath();
  ctx.moveTo(x, y1);
  ctx.lineTo(x, y2);
  ctx.moveTo(x - 6, y1);
  ctx.lineTo(x + 6, y1);
  ctx.moveTo(x - 6, y2);
  ctx.lineTo(x + 6, y2);
  ctx.stroke();

  ctx.beginPath();
  ctx.arc(x, y1, 3, 0, Math.PI * 2);
  ctx.arc(x, y2, 3, 0, Math.PI * 2);
  ctx.fill();

  ctx.font = "10px sans-serif";
  const textWidth = ctx.measureText(label).width;
  const labelW = Math.ceil(textWidth + 10);
  const labelH = 16;
  const labelX = clampNumber(x + 8, 2, Math.max(2, state.editWidth - labelW - 2));
  const labelY = clampNumber((top + bottom) / 2 - labelH / 2, 2, Math.max(2, state.editHeight - labelH - 2));
  ctx.fillStyle = "rgba(17, 19, 24, 0.86)";
  ctx.fillRect(labelX, labelY, labelW, labelH);
  ctx.strokeStyle = "rgba(255, 236, 139, 0.72)";
  ctx.strokeRect(labelX + 0.5, labelY + 0.5, labelW - 1, labelH - 1);
  ctx.fillStyle = "#fff5b8";
  ctx.fillText(label, labelX + 5, labelY + 11);
  ctx.restore();
}

function render() {
  state.width = GAME_SCREEN_WIDTH;
  state.height = Math.max(GAME_SCREEN_HEIGHT, Number(document.getElementById("canvasH").value) || GAME_SCREEN_HEIGHT);
  state.baseFit = BASE_FIT_WIDTH;
  syncCanvasSizeToPreparedRoom();
  state.baseOpacity = (Number(document.getElementById("baseOpacity").value) || 0) / 100;
  state.sourceScale = Number(document.getElementById("sourceScale").value) || 1;
  state.furnitureImportMode = document.getElementById("furnitureImportMode").value;
  state.aspectLocked = document.getElementById("aspectLock").checked;
  state.guides.show = document.getElementById("showGuides").checked;
  state.guides.spriteX = Number(document.getElementById("spriteX").value) || 0;
  state.guides.spriteY = Number(document.getElementById("spriteY").value) || 0;
  state.guides.spriteW = Number(document.getElementById("spriteW").value) || 64;
  state.guides.spriteH = Number(document.getElementById("spriteH").value) || 64;
  commitLightInputsToState();
  state.night.castShadows = document.getElementById("castShadows").checked;
  state.night.shadowMode = normalizeShadowMode(document.getElementById("shadowMode").value);
  state.showGrid = document.getElementById("showGrid").checked;
  state.gridOpacity = clampNumber((Number(document.getElementById("gridOpacity").value) || 0) / 100, 0, 1);
  state.snapToGrid = document.getElementById("snapToGrid").checked;
  state.editZoom = normalizedEditZoom(document.getElementById("editZoom")?.value || Math.round(state.editZoom * 100));
  updateGridOpacityLabel();

  if (canvas.width !== state.editWidth || canvas.height !== state.editHeight) {
    canvas.width = state.editWidth;
    canvas.height = state.editHeight;
  }
  updateCanvasDisplaySize();

  drawBase();
  const lightPoint = activeLightPoint();
  const editRectForItem = (item) => {
    const bounds = itemBounds(item);
    return targetRectToEdit(bounds.x, bounds.y, bounds.w, bounds.h);
  };
  const shadowLayers = createItemShadowLayers(
    ctx,
    editRectForItem,
    targetToEditPoint(lightPoint),
    Math.max(editScaleX(), editScaleY())
  );
  drawShadowLayers(ctx, shadowLayers);
  drawSceneItems(ctx, editRectForItem, shadowLayers);
  drawEditorLightOverlay();
  drawGrid();
  if (state.editMode === "shape") drawFacesOverlay();
  drawGuides();
  drawProjectionGuidesOverlay();
  drawSelection();
  drawLightHandle();
  drawMeasureOverlay();
  drawScreenFrame(ctx, true);
  renderPreview();
  updateMeasurePanel();
  updateWorkflowStatus();
  updateSharedPointHint();
}

function refreshList() {
  assetList.innerHTML = "";
  const backgroundTitle = document.createElement("div");
  backgroundTitle.className = "asset-section-title";
  backgroundTitle.textContent = `Background layers (${backgroundItems().length})`;
  assetList.appendChild(backgroundTitle);

  for (const background of backgroundItems()) {
    const active = backgroundVisibleInMode(background);
    const el = document.createElement("div");
    el.className = `asset-item background${active ? " active-background" : ""}`;
    const modes = [
      background.visibleInDay !== false ? "Day" : "",
      background.visibleInNight !== false ? "Night" : ""
    ].filter(Boolean).join(" / ") || "No mode";
    const hidden = background.visible === false ? ", hidden" : "";
    const meta = `${background.width}x${background.height} · ${modes}${hidden}`;
    el.innerHTML = `
      <img class="asset-thumb" alt="" src="${background.dataUrl}">
      <div class="asset-details">
        <div class="asset-kind">Background${active ? " · previewing" : ""}</div>
        <div class="asset-name">${escapeHtml(background.name)}</div>
        <div class="asset-meta">${escapeHtml(meta)}</div>
      </div>
      <div class="asset-actions">
        <label class="asset-visibility" title="${background.visible !== false ? "Hide" : "Show"} ${escapeAttr(background.name)}">
          <input class="background-visible" type="checkbox" ${background.visible !== false ? "checked" : ""}>
          <span>${background.visible !== false ? "On" : "Off"}</span>
        </label>
        <div class="asset-mode-toggles">
          <label><input class="background-day" type="checkbox" ${background.visibleInDay !== false ? "checked" : ""}> Day</label>
          <label><input class="background-night" type="checkbox" ${background.visibleInNight !== false ? "checked" : ""}> Night</label>
        </div>
        <button class="background-delete danger" type="button">Delete</button>
      </div>
    `;
    el.querySelector(".background-visible").addEventListener("change", (event) => {
      event.stopPropagation();
      background.visible = event.target.checked;
      setLegacyBaseFromBackgrounds();
      updateBaseImageMeta();
      refreshList();
      render();
      commitHistory();
    });
    el.querySelector(".background-day").addEventListener("change", (event) => {
      event.stopPropagation();
      background.visibleInDay = event.target.checked;
      if (!background.visibleInDay && !background.visibleInNight) {
        background.visibleInNight = true;
      }
      setLegacyBaseFromBackgrounds();
      updateBaseImageMeta();
      refreshList();
      render();
      commitHistory();
    });
    el.querySelector(".background-night").addEventListener("change", (event) => {
      event.stopPropagation();
      background.visibleInNight = event.target.checked;
      if (!background.visibleInDay && !background.visibleInNight) {
        background.visibleInDay = true;
      }
      setLegacyBaseFromBackgrounds();
      updateBaseImageMeta();
      refreshList();
      render();
      commitHistory();
    });
    el.querySelector(".background-delete").addEventListener("click", (event) => {
      event.stopPropagation();
      const wasPrimary = primaryBackground()?.id === background.id;
      state.backgroundItems = backgroundItems().filter((item) => item.id !== background.id);
      setLegacyBaseFromBackgrounds();
      if (!state.backgroundItems.length) {
        state.baseTrim = null;
      } else if (wasPrimary && primaryBackground()?.img) {
        state.baseTrim = detectTrimBox(primaryBackground().img);
      }
      syncCanvasSizeToPreparedRoom();
      updateBaseImageMeta();
      refreshList();
      render();
      commitHistory();
    });
    assetList.appendChild(el);
  }

  if (!backgroundItems().length) {
    const empty = document.createElement("div");
    empty.className = "hint";
    empty.textContent = "No background layers loaded.";
    assetList.appendChild(empty);
  }

  const itemTitle = document.createElement("div");
  itemTitle.className = "asset-section-title";
  itemTitle.textContent = `Scene layers (${state.items.length})`;
  assetList.appendChild(itemTitle);

  for (const item of sortedItems()) {
    const el = document.createElement("div");
    el.className = `asset-item${item.id === state.selectedId ? " selected" : ""}`;
    const isSprite = item.source === "project_sprite";
    const type = isSprite ? "Project sprite" : "Furniture";
    const bounds = itemBounds(item);
    const meta = isSprite
      ? `${item.action || "-"}/${item.frame || "-"} · ${Math.round(bounds.w)}x${Math.round(bounds.h)} · z${item.z}`
      : `${itemKindLabel(item.kind)} · ${Math.round(bounds.w)}x${Math.round(bounds.h)} · z${item.z}`;
    el.setAttribute("role", "button");
    el.setAttribute("tabindex", "0");
    el.setAttribute("aria-pressed", item.id === state.selectedId ? "true" : "false");
    el.innerHTML = `
      <img class="asset-thumb" alt="" src="${item.dataUrl}">
      <div class="asset-details">
        <div class="layer-name-row">
          <span class="asset-kind-inline">${escapeHtml(type)}</span>
          <span class="asset-name">${escapeHtml(item.name)}</span>
        </div>
        <div class="asset-meta">${escapeHtml(meta)}</div>
      </div>
      <label class="asset-visibility" title="${item.visible ? "Hide" : "Show"} ${escapeAttr(item.name)}">
        <input type="checkbox" ${item.visible ? "checked" : ""}>
        <span>${item.visible ? "On" : "Off"}</span>
      </label>
    `;
    const selectLayerItem = () => {
      state.selectedId = item.id;
      state.selectedFaceId = null;
      state.selectedPointIndex = null;
      state.selectedPointId = null;
      state.lightSelected = false;
      state.editMode = "furniture";
      updateEditModeButtons();
      refreshList();
      refreshSelectedPanel();
      refreshFaceList();
      refreshSelectedFacePanel();
      render();
    };
    el.addEventListener("click", selectLayerItem);
    el.addEventListener("keydown", (event) => {
      if (event.target !== el) return;
      if (event.key !== "Enter" && event.key !== " ") return;
      event.preventDefault();
      selectLayerItem();
    });
    el.querySelector(".asset-visibility").addEventListener("click", (event) => {
      event.stopPropagation();
    });
    el.querySelector(".asset-visibility input").addEventListener("change", (event) => {
      event.stopPropagation();
      item.visible = event.target.checked;
      refreshList();
      refreshSelectedPanel();
      render();
      commitHistory();
    });
    assetList.appendChild(el);
  }
  if (state.items.length === 0) {
    const empty = document.createElement("div");
    empty.className = "hint";
    empty.textContent = "No furniture or sprite previews loaded.";
    assetList.appendChild(empty);
  }
}

function buildSpriteSelectorHTML(item) {
  const catalog = projectSpriteCatalog();
  const entry = catalog.find((e) => e.speciesId === item.speciesId) || catalog[0];
  const speciesOptions = catalog.map((e) =>
    `<option value="${e.speciesId}"${e.speciesId === item.speciesId ? " selected" : ""}>${escapeHtml(e.name)}</option>`
  ).join("");

  const actions = entry?.actions || {};
  const actionOptions = Object.keys(actions).map((action) =>
    `<option value="${escapeAttr(action)}"${action === item.action ? " selected" : ""}>${escapeHtml(action)}</option>`
  ).join("");

  const frames = actions[item.action] || [];
  const frameOptions = frames.map((frame) =>
    `<option value="${escapeAttr(frame.path)}"${frame.kindName === item.frame ? " selected" : ""}>${escapeHtml(frame.kindName)}</option>`
  ).join("");

  return `
    <label class="field">Species<select id="spriteItemSpecies">${speciesOptions}</select></label>
    <div class="row">
      <label class="field">Action<select id="spriteItemAction">${actionOptions}</select></label>
      <label class="field">Frame<select id="spriteItemFrame">${frameOptions}</select></label>
    </div>
  `;
}

function bindSpriteSelectorInputs(item) {
  const speciesInput = document.getElementById("spriteItemSpecies");
  const actionInput = document.getElementById("spriteItemAction");
  const frameInput = document.getElementById("spriteItemFrame");
  if (!speciesInput || !actionInput || !frameInput) return;

  function rebuildActionOptions() {
    const entry = projectSpriteCatalog().find((e) => e.speciesId === Number(speciesInput.value));
    const actions = entry?.actions || {};
    actionInput.innerHTML = Object.keys(actions).map((action) =>
      `<option value="${escapeAttr(action)}">${escapeHtml(action)}</option>`
    ).join("");
    rebuildFrameOptions();
  }

  function rebuildFrameOptions() {
    const entry = projectSpriteCatalog().find((e) => e.speciesId === Number(speciesInput.value));
    const frames = entry?.actions?.[actionInput.value] || [];
    frameInput.innerHTML = frames.map((frame) =>
      `<option value="${escapeAttr(frame.path)}">${escapeHtml(frame.kindName)}</option>`
    ).join("");
  }

  speciesInput.addEventListener("change", () => {
    rebuildActionOptions();
    updateProjectSpriteFrame(item, Number(speciesInput.value), actionInput.value, frameInput.value).then(() => {
      refreshList();
      refreshSelectedPanel();
    });
  });
  actionInput.addEventListener("change", () => {
    rebuildFrameOptions();
    updateProjectSpriteFrame(item, Number(speciesInput.value), actionInput.value, frameInput.value).then(() => {
      refreshList();
      refreshSelectedPanel();
    });
  });
  frameInput.addEventListener("change", () => {
    updateProjectSpriteFrame(item, Number(speciesInput.value), actionInput.value, frameInput.value).then(() => {
      refreshList();
      refreshSelectedPanel();
    });
  });
}

function refreshSelectedPanel() {
  const item = selectedItem();
  if (!item) {
    selectedPanel.innerHTML = '<div class="hint">Select furniture or a sprite preview to edit transform and layer values.</div>';
    updateEditModeButtons();
    return;
  }

  const isSprite = item.source === "project_sprite";
  normalizeItemSemantics(item);
  const furnitureTypeOptions = FURNITURE_TYPES.map((type) => (
    `<option value="${type.value}" ${normalizeFurnitureType(item.furnitureType) === type.value ? "selected" : ""}>${type.label}</option>`
  )).join("");
  const itemKindOptions = ITEM_KINDS.map((kind) => (
    `<option value="${kind.value}" ${normalizeItemKind(item.kind) === kind.value ? "selected" : ""}>${kind.label}</option>`
  )).join("");
  const footprintOptions = FOOTPRINT_TYPES.map((type) => (
    `<option value="${type.value}" ${normalizeFootprint(item.footprint) === type.value ? "selected" : ""}>${type.label}</option>`
  )).join("");
  const shadowAnchorOptions = SHADOW_ANCHORS.map((anchor) => (
    `<option value="${anchor.value}" ${normalizeShadowAnchor(item.shadowAnchor) === anchor.value ? "selected" : ""}>${anchor.label}</option>`
  )).join("");

  const bounds = itemBounds(item);
  const behaviorPreset = itemKindPreset(item.kind);
  const castsShadow = !isSprite && itemCastsShadow(item);
  const usesWallShadow = !isSprite && castsShadow && itemUsesWallShadowAnchor(item);
  const selectedShadowFaceIds = new Set(itemShadowFaceIds(item));
  const shadowMeta = !isSprite ? itemShadowSummary(item) : "";
  const wallShadowFaces = state.faces.filter((face) =>
    normalizeFaceType(face.type) === "wall" &&
    !isSemanticAreaFace(face) &&
    face.points.length >= 3
  );
  const showWallShadowFaces = usesWallShadow || selectedShadowFaceIds.size > 0;
  const wallShadowFaceControls = wallShadowFaces.length
    ? `
      <div class="shadow-face-targets">
        <div class="field shadow-face-field">
          <span>Wall shadow faces</span>
          <div class="shadow-face-control-row">
            <details class="shadow-face-dropdown">
              <summary><span id="shadowFaceDropdownLabel">${escapeHtml(shadowFaceSelectionLabel(item, wallShadowFaces))}</span></summary>
              <div class="shadow-face-list">
                ${wallShadowFaces.map((face, index) => {
                  const surface = shadowSurfaceForFace(face);
                  const muted = !faceVisible(face) || face.receivesShadow === false;
                  const meta = `${surface}${face.receivesShadow === false ? ", global off" : ""}${faceVisible(face) ? "" : ", hidden"}`;
                  return `
                    <label class="shadow-face-row${muted ? " muted" : ""}">
                      <input class="shadow-face-target" type="checkbox" data-face-id="${escapeAttr(face.id)}" ${selectedShadowFaceIds.has(String(face.id)) ? "checked" : ""}>
                      <span>${escapeHtml(shadowFaceDisplayName(face, index))}</span>
                      <small>${escapeHtml(meta)}</small>
                    </label>
                  `;
                }).join("")}
              </div>
            </details>
          </div>
        </div>
        <div class="shadow-face-actions">
          <button id="clearShadowFaceTargets" type="button">Auto</button>
        </div>
        <div class="hint">None selected = automatic. Selected faces override global Receive shadows for this item.</div>
      </div>
    `
    : '<div class="hint">No wall faces yet. Add wall faces in Shape mode to target wall shadows.</div>';
  let html = `
    <details class="property-group" open>
      <summary>
        <span>Basic</span>
        <span class="section-meta">${Math.round(bounds.w)}x${Math.round(bounds.h)}</span>
      </summary>
      <div class="property-group-body stack">
        <label class="field">Name<input id="itemName" type="text" value="${escapeAttr(item.name)}"></label>
        ${!isSprite ? `
          <label class="field">Behavior<select id="itemKind">${itemKindOptions}</select></label>
          <div class="behavior-summary">${escapeHtml(behaviorPreset.summary)}</div>
        ` : ""}
        <div class="row">
          <label class="field">X<input id="itemX" type="number" value="${Math.round(item.x)}"></label>
          <label class="field">Y<input id="itemY" type="number" value="${Math.round(item.y)}"></label>
        </div>
        <div class="row">
          <label class="field">W<input id="itemW" type="number" step="1" min="1" value="${Math.round(bounds.w)}"></label>
          <label class="field">H<input id="itemH" type="number" step="1" min="1" value="${Math.round(bounds.h)}"></label>
        </div>
        <div class="basic-option-row">
          <label class="field compact-field">Opacity<input id="itemOpacity" type="number" step="0.05" min="0" max="1" value="${item.opacity}"></label>
          <label class="check-row inline-check"><input id="itemAspectLock" type="checkbox" ${itemAspectLocked(item) ? "checked" : ""}> Lock ratio</label>
          ${!isSprite ? `
            <div class="mode-visibility-row">
              <label class="check-row"><input id="itemVisibleDay" type="checkbox" ${item.visibleInDay !== false ? "checked" : ""}> Day</label>
              <label class="check-row"><input id="itemVisibleNight" type="checkbox" ${item.visibleInNight !== false ? "checked" : ""}> Night</label>
            </div>
          ` : ""}
        </div>
        ${isSprite ? '<div id="itemSizeHint" class="hint compact-source-meta"></div>' : ""}
      </div>
    </details>
  `;

  if (isSprite) {
    html += `
      <details class="property-group" id="itemSourceGroup">
        <summary>
          <span>Sprite source</span>
          <span class="section-meta">${escapeHtml(item.action || "-")}</span>
        </summary>
        <div class="property-group-body stack">
          ${buildSpriteSelectorHTML(item)}
        </div>
      </details>
    `;
  } else {
    html += `
      <details class="property-group" id="itemMetadataGroup">
        <summary>
          <span>Metadata</span>
          <span class="section-meta">${escapeHtml(furnitureTypeLabel(item.furnitureType))}</span>
        </summary>
        <div class="property-group-body stack">
          <label class="field">Library category<select id="itemFurnitureType">${furnitureTypeOptions}</select></label>
          <div class="hint">Used for library filtering and gameplay identity. It does not change shadows or placement behavior.</div>
          <div id="itemSizeHint" class="hint compact-source-meta"></div>
        </div>
      </details>
    `;
    html += `
      <details class="property-group">
        <summary>
          <span>Shadow</span>
          <span class="section-meta" id="itemShadowMeta">${shadowMeta}</span>
        </summary>
        <div class="property-group-body stack">
          <label class="check-row"><input id="itemCastsShadow" type="checkbox" ${itemCastsShadow(item) ? "checked" : ""}> Cast shadow</label>
          <label class="check-row"><input id="itemReceivesShadow" type="checkbox" ${item.receivesShadow !== false ? "checked" : ""}> Receive shadow</label>
          ${castsShadow ? `
            <div class="row-3">
              <label class="field">Opacity<input id="itemShadowOpacity" type="number" min="0" max="100" step="1" value="${Math.round(itemShadowOpacity(item))}"></label>
              <label class="field">Length<input id="itemShadowLength" type="number" min="0" max="120" step="1" value="${Math.round(itemShadowLength(item))}"></label>
              <label class="field">Blur<input id="itemShadowBlur" type="number" min="0" max="16" step="0.1" value="${Number(itemShadowBlur(item).toFixed(1))}"></label>
            </div>
            <label class="field">Shadow target<select id="itemShadowAnchor">${shadowAnchorOptions}</select></label>
            ${showWallShadowFaces ? wallShadowFaceControls : ""}
            <div class="hint">Use Auto for normal floor props. Use Wall for shelves and wall-mounted furniture.</div>
          ` : ""}
        </div>
      </details>
    `;
    html += `
      <details class="property-group" id="shadowAdvancedGroup">
        <summary>
          <span>Shadow Advanced</span>
          <span class="section-meta">${Math.round(itemHeightPx(item))}px</span>
        </summary>
        <div class="property-group-body stack">
          ${castsShadow ? `
            <div class="row">
              <label class="field">Height<input id="itemHeightPx" type="number" min="0" max="160" step="1" value="${Math.round(itemHeightPx(item))}"></label>
              <label class="field">Footprint<select id="itemFootprint">${footprintOptions}</select></label>
            </div>
            <div id="footprintPolygonEditor" class="polygon-editor${state.activeItemPolygonKey === "footprintPolygon" ? " active" : ""}">
              <div class="section-title-row">
                <div class="section-title">Footprint polygon</div>
                <button id="drawFootprintPolygon" class="polygon-draw-button" type="button">Draw face</button>
              </div>
              <textarea id="itemFootprintPolygon" rows="4" spellcheck="false" placeholder="0.18, 0.72&#10;0.82, 0.72&#10;0.96, 0.96&#10;0.04, 0.96">${escapeHtml(polygonToText(item.footprintPolygon))}</textarea>
              <div id="footprintPolygonDraftBar" class="polygon-draft-bar" hidden>
                <span id="footprintPolygonDraftMeta">0 point(s). Add at least 3 points.</span>
                <button id="finishFootprintPolygonDraft" type="button">Finish</button>
                <button id="exitFootprintPolygonDraft" type="button">Exit</button>
              </div>
              <div class="toolbar polygon-actions">
                <button id="setFootprintBase" type="button">Base shape</button>
                <button id="setFootprintBounds" type="button">Bounds</button>
                <button id="clearFootprintPolygon" type="button">Clear</button>
              </div>
            </div>
            <div id="shadowPolygonEditor" class="polygon-editor${state.activeItemPolygonKey === "shadowPolygon" ? " active" : ""}">
              <div class="section-title-row">
                <div class="section-title">Shadow polygon</div>
                <button id="drawShadowPolygon" class="polygon-draw-button" type="button">Draw face</button>
              </div>
              <textarea id="itemShadowPolygon" rows="4" spellcheck="false" placeholder="0, 0&#10;1, 0&#10;1, 1&#10;0, 1">${escapeHtml(polygonToText(item.shadowPolygon))}</textarea>
              <div id="shadowPolygonDraftBar" class="polygon-draft-bar" hidden>
                <span id="shadowPolygonDraftMeta">0 point(s). Add at least 3 points.</span>
                <button id="finishShadowPolygonDraft" type="button">Finish</button>
                <button id="exitShadowPolygonDraft" type="button">Exit</button>
              </div>
              <div class="toolbar polygon-actions">
                <button id="setShadowBounds" type="button">Bounds</button>
                <button id="copyFootprintToShadow" type="button">Use footprint</button>
                <button id="clearShadowPolygon" type="button">Clear</button>
              </div>
            </div>
            ${usesWallShadow ? `
              <div class="row">
                <label class="field">Wall depth<input id="itemWallShadowDepthPx" type="number" min="0" max="160" step="1" value="${Math.round(itemWallShadowDepthPx(item))}"></label>
                <label class="field">Wall shadow Y<input id="itemWallShadowOffsetY" type="number" min="-80" max="80" step="1" value="${Math.round(itemWallShadowOffsetY(item))}"></label>
              </div>
            ` : ""}
            <div class="hint">Advanced controls tune how the shadow shape is generated. Most furniture can keep these defaults.</div>
          ` : ""}
        </div>
      </details>
    `;
  }

  html += `
    <details class="property-group" id="itemDrawOrderGroup">
      <summary>
        <span>Draw order</span>
        <span class="section-meta">sortY ${Math.round(itemSortY(item))}</span>
      </summary>
      <div class="property-group-body stack">
        <div class="row">
          <label class="field">Z<input id="itemZ" type="number" value="${item.z}"></label>
          <label class="field">Sort Y<input id="itemSortY" type="number" value="${Math.round(itemSortY(item))}"></label>
        </div>
        ${!isSprite ? `
          <label class="check-row"><input id="itemOccludesSprite" type="checkbox" ${item.occludesSprite ? "checked" : ""}> Occlude sprite</label>
        ` : ""}
        <div class="hint">Z controls the main draw order. Items with the same Z use Sort Y, normally the object's bottom edge.</div>
      </div>
    </details>
  `;

  selectedPanel.innerHTML = html;

  document.getElementById("shadowAdvancedGroup")?.addEventListener("toggle", () => render());

  if (isSprite) {
    bindSpriteSelectorInputs(item);
  }

  const bind = (id, fn) => {
    const input = document.getElementById(id);
    if (!input) return;
    input.addEventListener("input", () => {
      fn(input);
      refreshList();
      render();
      commitHistory();
    });
  };
  const setInputValue = (id, value, exceptId = "") => {
    if (id === exceptId) return;
    const input = document.getElementById(id);
    if (input) input.value = value;
  };
  const updateSizeFields = (exceptId = "") => {
    const nextBounds = itemBounds(item);
    setInputValue("itemScale", Number(itemScaleX(item).toFixed(4)), exceptId);
    setInputValue("itemW", Math.round(nextBounds.w), exceptId);
    setInputValue("itemH", Math.round(nextBounds.h), exceptId);
    setInputValue("itemSortY", Math.round(itemSortY(item)), exceptId);
    const hint = document.getElementById("itemSizeHint");
    if (hint) {
      const source = `${item.sourceWidth}x${item.sourceHeight}`;
      const target = `${Math.round(nextBounds.w)}x${Math.round(nextBounds.h)}`;
      const scale = `${Math.round(itemScaleX(item) * 100)}% x ${Math.round(itemScaleY(item) * 100)}%`;
      hint.textContent = `Source ${source} · Target ${target} · ${scale}`;
      hint.title = `${item.fileName}: source ${source}, target ${target}, scale ${scale}`;
    }
  };
  const bindSize = (id, fn) => {
    const input = document.getElementById(id);
    if (!input) return;
    input.addEventListener("input", () => {
      fn(input);
      item.sortY = autoSortY(item);
      updateSizeFields(id);
      refreshList();
      render();
      commitHistory();
    });
  };
  bind("itemName", (input) => { item.name = input.value; });
  bind("itemX", (input) => { item.x = Number(input.value) || 0; });
  bind("itemY", (input) => {
    item.y = Number(input.value) || 0;
    item.sortY = autoSortY(item);
  });
  bind("itemZ", (input) => { item.z = Number(input.value) || 0; });
  bindSize("itemScale", (input) => { setItemUniformScale(item, input.value); });
  bindSize("itemW", (input) => { setItemTargetWidth(item, input.value); });
  bindSize("itemH", (input) => { setItemTargetHeight(item, input.value); });
  if (!isSprite) {
    const furnitureTypeInput = document.getElementById("itemFurnitureType");
    if (furnitureTypeInput) furnitureTypeInput.addEventListener("change", (event) => {
      item.furnitureType = normalizeFurnitureType(event.target.value);
      refreshList();
      render();
      commitHistory();
    });
  }
  document.getElementById("itemAspectLock").addEventListener("change", (event) => {
    setItemAspectLocked(item, event.target.checked);
    updateSizeFields();
    refreshList();
    render();
    commitHistory();
  });
  bind("itemOpacity", (input) => { item.opacity = Math.max(0, Math.min(1, Number(input.value))); });
  bind("itemSortY", (input) => { item.sortY = Number(input.value) || autoSortY(item); });
  if (!isSprite) {
    const itemKindInput = document.getElementById("itemKind");
    if (itemKindInput) itemKindInput.addEventListener("change", (event) => {
      if (itemHasBehaviorOverrides(item) && !window.confirm(
        "Changing Behavior resets height, footprint, shadow target, shadow flags, sprite occlusion, and wall depth. Custom polygon points are kept. Continue?"
      )) {
        event.target.value = normalizeItemKind(item.kind);
        return;
      }
      applyItemKindPreset(item, event.target.value);
      refreshList();
      refreshSelectedPanel();
      render();
      commitHistory();
    });
    const bindModeVisibility = (id, key) => {
      const input = document.getElementById(id);
      if (!input) return;
      input.addEventListener("change", (event) => {
        item[key] = event.target.checked;
        if (!item.visibleInDay && !item.visibleInNight) {
          item[key] = true;
          event.target.checked = true;
          setStatus("Furniture must be visible in at least one mode.");
        }
        refreshList();
        render();
        commitHistory();
      });
    };
    bindModeVisibility("itemVisibleDay", "visibleInDay");
    bindModeVisibility("itemVisibleNight", "visibleInNight");
    bind("itemHeightPx", (input) => { item.heightPx = coerceNumber(input.value, itemHeightPx(item), 0, 160); });
    bind("itemShadowOpacity", (input) => {
      item.shadowOpacity = coerceNumber(input.value, itemShadowOpacity(item), 0, 100);
      updateSelectedShadowMeta(item);
    });
    bind("itemShadowLength", (input) => { item.shadowLength = coerceNumber(input.value, itemShadowLength(item), 0, 120); });
    bind("itemShadowBlur", (input) => { item.shadowBlur = coerceNumber(input.value, itemShadowBlur(item), 0, 16); });
    const footprintInput = document.getElementById("itemFootprint");
    if (footprintInput) footprintInput.addEventListener("change", (event) => {
      item.footprint = normalizeFootprint(event.target.value);
      refreshList();
      render();
      commitHistory();
    });
    const shadowAnchorInput = document.getElementById("itemShadowAnchor");
    if (shadowAnchorInput) shadowAnchorInput.addEventListener("change", (event) => {
      item.shadowAnchor = normalizeShadowAnchor(event.target.value);
      refreshList();
      refreshSelectedPanel();
      render();
      commitHistory();
    });
    for (const input of selectedPanel.querySelectorAll(".shadow-face-target")) {
      input.addEventListener("change", (event) => {
        const ids = new Set(itemShadowFaceIds(item));
        const faceId = String(event.target.dataset.faceId || "");
        if (event.target.checked) ids.add(faceId);
        else ids.delete(faceId);
        item.shadowFaceIds = [...ids].filter(Boolean);
        updateSelectedShadowMeta(item);
        updateShadowFaceDropdownLabel(item, wallShadowFaces);
        render();
        commitHistory();
      });
    }
    document.getElementById("clearShadowFaceTargets")?.addEventListener("click", (event) => {
      event.preventDefault();
      item.shadowFaceIds = [];
      for (const input of selectedPanel.querySelectorAll(".shadow-face-target")) input.checked = false;
      updateSelectedShadowMeta(item);
      updateShadowFaceDropdownLabel(item, wallShadowFaces);
      render();
      commitHistory();
    });
    const bindPolygonTextarea = (id, key) => {
      const input = document.getElementById(id);
      if (!input) return;
      input.addEventListener("focus", () => {
        setActiveItemPolygon(key);
        render();
      });
      input.addEventListener("input", () => {
        setActiveItemPolygon(key);
        item[key] = parsePolygonText(input.value);
        syncPolygonEditorField(item, key, { updateTextarea: false });
        render();
        commitHistory();
      });
    };
    const setPolygon = (key, value) => {
      setActiveItemPolygon(key);
      item[key] = normalizeLocalPolygon(value);
      syncPolygonEditorField(item, key);
      render();
      commitHistory();
    };
    const bindPolygonDraftControls = (key) => {
      const ids = polygonEditorIds(key);
      document.getElementById(ids.drawButton)?.addEventListener("click", (event) => {
        event.preventDefault();
        startItemPolygonDraft(item, key);
      });
      document.getElementById(ids.finishDraft)?.addEventListener("click", (event) => {
        event.preventDefault();
        finishItemPolygonDraft();
      });
      document.getElementById(ids.exitDraft)?.addEventListener("click", (event) => {
        event.preventDefault();
        cancelItemPolygonDraft();
      });
    };
    bindPolygonTextarea("itemFootprintPolygon", "footprintPolygon");
    bindPolygonTextarea("itemShadowPolygon", "shadowPolygon");
    bindPolygonDraftControls("footprintPolygon");
    bindPolygonDraftControls("shadowPolygon");
    document.getElementById("setFootprintBase")?.addEventListener("click", (event) => {
      event.preventDefault();
      item.footprint = "polygon";
      setPolygon("footprintPolygon", defaultFootprintPolygon());
    });
    document.getElementById("setFootprintBounds")?.addEventListener("click", (event) => {
      event.preventDefault();
      item.footprint = "polygon";
      setPolygon("footprintPolygon", defaultShadowPolygon());
    });
    document.getElementById("clearFootprintPolygon")?.addEventListener("click", (event) => {
      event.preventDefault();
      setPolygon("footprintPolygon", []);
    });
    document.getElementById("setShadowBounds")?.addEventListener("click", (event) => {
      event.preventDefault();
      setPolygon("shadowPolygon", defaultShadowPolygon());
    });
    document.getElementById("copyFootprintToShadow")?.addEventListener("click", (event) => {
      event.preventDefault();
      setPolygon("shadowPolygon", effectiveFootprintPolygon(item));
    });
    document.getElementById("clearShadowPolygon")?.addEventListener("click", (event) => {
      event.preventDefault();
      setPolygon("shadowPolygon", []);
    });
    bind("itemWallShadowDepthPx", (input) => { item.wallShadowDepthPx = coerceNumber(input.value, itemWallShadowDepthPx(item), 0, 160); });
    bind("itemWallShadowOffsetY", (input) => { item.wallShadowOffsetY = coerceNumber(input.value, 0, -80, 80); });
    const castsShadowInput = document.getElementById("itemCastsShadow");
    if (castsShadowInput) castsShadowInput.addEventListener("change", (event) => {
      item.castsShadow = event.target.checked;
      refreshList();
      refreshSelectedPanel();
      render();
      commitHistory();
    });
    bind("itemReceivesShadow", (input) => { item.receivesShadow = input.checked; });
    bind("itemOccludesSprite", (input) => { item.occludesSprite = input.checked; });
  }
  updateSizeFields();
  updatePolygonEditorState();
  updateEditModeButtons();
}

function pointRowsHtml(points, selectedPointId, options = {}) {
  const canDeleteBelowThree = options.canDeleteBelowThree === true;
  return points.map((point, index) => `
    <div class="point-row${point.id === selectedPointId ? " selected" : ""}" data-point-index="${index}">
      <button class="point-select" type="button" data-point-index="${index}">${index + 1}</button>
      <label class="field point-coordinate"><span>X</span><input class="point-x" data-point-index="${index}" type="number" value="${Math.round(point.x)}"></label>
      <label class="field point-coordinate"><span>Y</span><input class="point-y" data-point-index="${index}" type="number" value="${Math.round(point.y)}"></label>
      <button class="point-delete danger" type="button" data-point-index="${index}" title="${canDeleteBelowThree || points.length > 3 ? `Delete point ${index + 1}` : "A closed face needs at least 3 points"}" aria-label="Delete point ${index + 1}" ${canDeleteBelowThree || points.length > 3 ? "" : "disabled"}>&times;</button>
    </div>
  `).join("");
}

function faceDetailHtml(face) {
  const type = normalizeFaceType(face.type);
  const isSpriteArea = type === FACE_TYPE_SPRITE_AREA;
  const isDoorway = type === FACE_TYPE_DOORWAY;
  const isSemanticArea = isSpriteArea || isDoorway;
  const surfaceOptions = SHADOW_SURFACES.map((surface) => (
    `<option value="${surface.value}" ${normalizeShadowSurface(face.shadowSurface, face.type) === surface.value ? "selected" : ""}>${surface.label}</option>`
  )).join("");
  return `
    <div class="face-detail">
      <label class="field">
        Type
        <select class="face-type-select">
          <option value="floor" ${type === "floor" ? "selected" : ""}>floor</option>
          <option value="wall" ${type === "wall" ? "selected" : ""}>wall</option>
          <option value="${FACE_TYPE_SPRITE_AREA}" ${isSpriteArea ? "selected" : ""}>sprite area</option>
          <option value="${FACE_TYPE_DOORWAY}" ${isDoorway ? "selected" : ""}>门口</option>
        </select>
      </label>
      ${isSemanticArea ? `
        <div class="hint">${isDoorway
          ? "该多边形会作为 doorway 过渡区域导出，供后续进门/出门动画使用；不参与背景与阴影绘制。"
          : "This polygon is exported as a sprite movement area. It does not receive furniture shadows."}</div>
      ` : `
        <div class="row">
        <label class="field">
          Surface
          <select class="face-shadow-surface">${surfaceOptions}</select>
        </label>
        <label class="check-row inline-check">
          <input class="face-receives-shadow" type="checkbox" ${face.receivesShadow !== false ? "checked" : ""}>
          Receive shadows
        </label>
        </div>
      `}
      <div class="toolbar">
        <button class="insert-point" type="button">Add point</button>
      </div>
      <div class="point-list">
        ${pointRowsHtml(face.points, state.selectedPointId)}
      </div>
      <div class="hint">${escapeHtml(face.id)} ${escapeHtml(faceTypeLabel(face.type))} points. Export scales these to game pixels.</div>
    </div>
  `;
}

function draftDetailHtml() {
  return `
    <div class="face-detail">
      <div class="hint">Click the first point on the canvas to close this draft face.</div>
      <div class="point-list">
        ${pointRowsHtml(state.draftPoints, state.selectedPointId, { canDeleteBelowThree: true })}
      </div>
      <div class="hint">Draft points can be dragged, edited, deleted, or reused with closed-face points.</div>
    </div>
  `;
}

function selectFaceItem(face, options = {}) {
  state.drawingFace = false;
  state.selectedFaceId = face.id;
  state.selectedPointIndex = null;
  state.selectedPointId = null;
  state.selectedId = null;
  state.editMode = "shape";
  if (options.expand !== false) state.expandedFaceIds.add(face.id);
  updateEditModeButtons();
  refreshFaceList();
  refreshList();
  refreshSelectedPanel();
  render();
}

function selectDraftItem(options = {}) {
  state.selectedFaceId = null;
  state.selectedId = null;
  state.editMode = "shape";
  if (state.selectedPointIndex == null && state.draftPoints.length) selectDraftPoint(0);
  if (options.expand !== false) state.draftExpanded = true;
  updateEditModeButtons();
  refreshFaceList();
  refreshList();
  refreshSelectedPanel();
  render();
}

function toggleFaceExpanded(face) {
  if (state.expandedFaceIds.has(face.id)) {
    state.expandedFaceIds.delete(face.id);
  } else {
    state.expandedFaceIds.add(face.id);
  }
  selectFaceItem(face, { expand: false });
}

function toggleDraftExpanded() {
  state.draftExpanded = !state.draftExpanded;
  selectDraftItem({ expand: false });
}

function toggleFaceVisible(face) {
  face.visible = faceVisible(face) ? false : true;
  refreshFaceList();
  refreshSelectedPanel();
  render();
  commitHistory();
}

function bindFaceDetailControls(face, root) {
  const detail = root.querySelector(".face-detail");
  if (!detail) return;
  detail.addEventListener("click", (event) => event.stopPropagation());

  detail.querySelector(".face-type-select").addEventListener("input", (event) => {
    const previousSurface = normalizeShadowSurface(face.shadowSurface, face.type);
    const previousType = normalizeFaceType(face.type);
    face.type = normalizeFaceType(event.target.value);
    const nextIsSemantic = face.type === FACE_TYPE_SPRITE_AREA || face.type === FACE_TYPE_DOORWAY;
    const previousWasSemantic = previousType === FACE_TYPE_SPRITE_AREA || previousType === FACE_TYPE_DOORWAY;
    if (nextIsSemantic) {
      face.shadowSurface = "floor";
      face.receivesShadow = false;
    } else if (previousWasSemantic) {
      face.receivesShadow = true;
      face.shadowSurface = normalizeShadowSurface(null, face.type);
    } else if ((previousSurface === "floor" && face.type === "wall") ||
        (previousSurface !== "floor" && face.type === "floor")) {
      face.shadowSurface = normalizeShadowSurface(null, face.type);
    }
    refreshFaceList();
    refreshSelectedPanel();
    render();
    commitHistory();
  });
  const surfaceSelect = detail.querySelector(".face-shadow-surface");
  if (surfaceSelect) surfaceSelect.addEventListener("input", (event) => {
    face.shadowSurface = normalizeShadowSurface(event.target.value, face.type);
    refreshFaceList();
    refreshSelectedPanel();
    render();
    commitHistory();
  });
  const receivesShadow = detail.querySelector(".face-receives-shadow");
  if (receivesShadow) receivesShadow.addEventListener("change", (event) => {
    face.receivesShadow = event.target.checked;
    refreshFaceList();
    refreshSelectedPanel();
    render();
    commitHistory();
  });
  detail.querySelector(".insert-point").addEventListener("click", insertPointAfterSelected);

  for (let index = 0; index < face.points.length; ++index) {
    detail.querySelector(`.point-select[data-point-index="${index}"]`).addEventListener("click", () => {
      selectPoint(index);
    });
    detail.querySelector(`.point-x[data-point-index="${index}"]`).addEventListener("input", (event) => {
      face.points[index].x = Math.round(Number(event.target.value) || 0);
      selectPointInFace(face, index);
      render();
      commitHistory();
    });
    detail.querySelector(`.point-y[data-point-index="${index}"]`).addEventListener("input", (event) => {
      face.points[index].y = Math.round(Number(event.target.value) || 0);
      selectPointInFace(face, index);
      render();
      commitHistory();
    });
    detail.querySelector(`.point-delete[data-point-index="${index}"]`).addEventListener("click", (event) => {
      event.stopPropagation();
      deleteFacePoint(face, index);
    });
  }
}

function bindDraftDetailControls(root) {
  const detail = root.querySelector(".face-detail");
  if (!detail) return;
  detail.addEventListener("click", (event) => event.stopPropagation());

  for (let index = 0; index < state.draftPoints.length; ++index) {
    detail.querySelector(`.point-select[data-point-index="${index}"]`).addEventListener("click", () => {
      selectDraftPoint(index);
      refreshFaceList();
      render();
    });
    detail.querySelector(`.point-x[data-point-index="${index}"]`).addEventListener("input", (event) => {
      state.draftPoints[index].x = Math.round(Number(event.target.value) || 0);
      selectDraftPoint(index);
      render();
      commitHistory();
    });
    detail.querySelector(`.point-y[data-point-index="${index}"]`).addEventListener("input", (event) => {
      state.draftPoints[index].y = Math.round(Number(event.target.value) || 0);
      selectDraftPoint(index);
      render();
      commitHistory();
    });
    detail.querySelector(`.point-delete[data-point-index="${index}"]`).addEventListener("click", (event) => {
      event.stopPropagation();
      deleteDraftPoint(index);
    });
  }
}

function deleteFaceById(faceId) {
  const face = state.faces.find((entry) => entry.id === faceId);
  if (!face) return;
  if (!window.confirm(`Delete ${face.id}?`)) return;
  state.faces = state.faces.filter((entry) => entry.id !== face.id);
  for (const item of state.items) {
    item.shadowFaceIds = itemShadowFaceIds(item).filter((id) => id !== String(face.id));
  }
  state.expandedFaceIds.delete(face.id);
  if (state.selectedFaceId === face.id) {
    state.selectedFaceId = null;
    state.selectedPointIndex = null;
    state.selectedPointId = null;
  }
  pruneUnusedPoints();
  refreshFaceList();
  refreshSelectedPanel();
  render();
  commitHistory();
}

function refreshFaceList() {
  faceList.innerHTML = "";
  const hasFaceData = state.faces.length > 0 || state.draftPoints.length > 0;
  const panelAvailable = hasFaceData || state.editMode === "shape";
  if (roomFacesPanel) {
    roomFacesPanel.hidden = !panelAvailable;
    if (panelAvailable && state.editMode === "shape" && !state.lightSelected) {
      roomFacesPanel.open = true;
    }
  }
  updateRoomFaceAddButton();
  if (!hasFaceData) {
    faceList.innerHTML = '<div class="hint room-face-empty">No room faces yet. Click + to add one.</div>';
    selectedFacePanel.innerHTML = "";
    selectedFacePanel.hidden = true;
    return;
  }

  const validFaceIds = new Set(state.faces.map((face) => face.id));
  for (const faceId of [...state.expandedFaceIds]) {
    if (!validFaceIds.has(faceId)) state.expandedFaceIds.delete(faceId);
  }
  for (const face of state.faces) {
    const selected = face.id === state.selectedFaceId;
    const expanded = state.expandedFaceIds.has(face.id);
    const el = document.createElement("div");
    const visible = faceVisible(face);
    el.className = `face-item${selected ? " selected" : ""}${visible ? "" : " face-hidden"}`;
    el.dataset.faceId = face.id;
    el.innerHTML = `
      <div class="face-summary">
        <div class="face-color" style="background:${faceStrokeColor(face.type)}"></div>
        <div>
          <div class="asset-name">${escapeHtml(face.id)}</div>
          <div class="asset-meta">${visible ? "" : "hidden, "}${escapeHtml(faceTypeLabel(face.type))}, ${isSemanticAreaFace(face) ? semanticFaceSummary(face) : `${escapeHtml(shadowSurfaceForFace(face))}, ${face.receivesShadow === false ? "no shadow" : "receives shadow"}`}, ${face.points.length} points</div>
        </div>
        <button class="face-visibility${visible ? "" : " is-hidden"}" type="button" title="${visible ? "Hide face on canvas" : "Show face on canvas"}" aria-label="${visible ? "Hide face on canvas" : "Show face on canvas"}"></button>
        <button class="face-toggle" type="button" title="${expanded ? "Collapse points" : "Expand points"}" aria-label="${expanded ? "Collapse" : "Expand"} ${escapeAttr(face.id)} points">${expanded ? "^" : "v"}</button>
        <button class="face-delete danger" type="button" title="Delete face" aria-label="Delete ${escapeAttr(face.id)}">x</button>
      </div>
      ${expanded ? faceDetailHtml(face) : ""}
    `;
    el.querySelector(".face-summary").addEventListener("click", () => selectFaceItem(face));
    el.querySelector(".face-toggle").addEventListener("click", (event) => {
      event.stopPropagation();
      toggleFaceExpanded(face);
    });
    el.querySelector(".face-visibility").addEventListener("click", (event) => {
      event.stopPropagation();
      toggleFaceVisible(face);
    });
    el.querySelector(".face-delete").addEventListener("click", (event) => {
      event.stopPropagation();
      deleteFaceById(face.id);
    });
    if (expanded) bindFaceDetailControls(face, el);
    faceList.appendChild(el);
  }

  if (state.draftPoints.length > 0) {
    const selected = !state.selectedFaceId;
    const expanded = state.draftExpanded;
    const el = document.createElement("div");
    el.className = `face-item draft${selected ? " selected" : ""}`;
    el.innerHTML = `
      <div class="face-summary">
        <div class="face-color" style="background:#ffffff"></div>
        <div>
          <div class="asset-name">Draft points</div>
          <div class="asset-meta">${state.draftPoints.length} unclosed point(s)</div>
        </div>
        <div></div>
        <button class="face-toggle" type="button" title="${expanded ? "Collapse points" : "Expand points"}">${expanded ? "^" : "v"}</button>
        <div></div>
      </div>
      ${expanded ? draftDetailHtml() : ""}
    `;
    el.querySelector(".face-summary").addEventListener("click", () => selectDraftItem());
    el.querySelector(".face-toggle").addEventListener("click", (event) => {
      event.stopPropagation();
      toggleDraftExpanded();
    });
    if (expanded) bindDraftDetailControls(el);
    faceList.appendChild(el);
  }

}

function selectPoint(index) {
  const face = selectedFace();
  selectPointInFace(face, index);
  refreshSelectedFacePanel();
  render();
}

function insertPointAfterSelected() {
  const face = selectedFace();
  if (!face) return;
  const index = state.selectedPointIndex == null ? face.points.length - 1 : state.selectedPointIndex;
  const nextIndex = (index + 1) % face.points.length;
  const a = face.points[index];
  const b = face.points[nextIndex];
  const point = {
    x: Math.round((a.x + b.x) / 2),
    y: Math.round((a.y + b.y) / 2)
  };
  const createdPoint = createShapePoint(point.x, point.y);
  face.points.splice(index + 1, 0, createdPoint);
  selectPointInFace(face, index + 1);
  refreshFaceList();
  refreshSelectedFacePanel();
  render();
  commitHistory();
}

function deleteFacePoint(face, index) {
  if (!face || face.points.length <= 3 || index < 0 || index >= face.points.length) return;
  face.points.splice(index, 1);
  pruneUnusedPoints();
  selectPointInFace(face, Math.min(index, face.points.length - 1));
  refreshFaceList();
  refreshSelectedFacePanel();
  render();
  commitHistory();
}

function deleteDraftPoint(index) {
  if (index < 0 || index >= state.draftPoints.length) return;
  state.draftPoints.splice(index, 1);
  if (state.draftPoints.length === 0) state.drawingFace = false;
  pruneUnusedPoints();
  selectDraftPoint(Math.min(index, state.draftPoints.length - 1));
  refreshFaceList();
  refreshSelectedFacePanel();
  render();
  commitHistory();
}

function refreshSelectedFacePanel() {
  selectedFacePanel.innerHTML = "";
  selectedFacePanel.hidden = true;
  refreshFaceList();
}

function pointerToCanvas(event) {
  const rect = canvas.getBoundingClientRect();
  return {
    x: (event.clientX - rect.left) * canvas.width / rect.width,
    y: (event.clientY - rect.top) * canvas.height / rect.height
  };
}

function pointerToTarget(event) {
  return editToTargetPoint(pointerToCanvas(event));
}

function hitTest(x, y) {
  const items = sortedItems().reverse();
  for (const item of items) {
    if (!itemVisibleInMode(item)) continue;
    const b = itemBounds(item);
    if (x >= b.x && x <= b.x + b.w && y >= b.y && y <= b.y + b.h) {
      return item;
    }
  }
  return null;
}

function polygonHitKeys() {
  const active = state.activeItemPolygonKey === "shadowPolygon" ? "shadowPolygon" : "footprintPolygon";
  return active === "shadowPolygon"
    ? ["shadowPolygon", "footprintPolygon"]
    : ["footprintPolygon", "shadowPolygon"];
}

function hitTestItemPolygonPoint(x, y) {
  const item = selectedItem();
  if (!item || item.source === "project_sprite") return null;
  const radius = editHandleRadius() + 4;
  let best = null;
  for (const key of polygonHitKeys()) {
    const points = itemPolygonEditPoints(item, key);
    for (let index = points.length - 1; index >= 0; --index) {
      const hitDistance = distance({ x, y }, points[index]);
      if (hitDistance <= radius && (!best || hitDistance < best.distance)) {
        best = { item, key, index, distance: hitDistance };
      }
    }
  }
  return best;
}

function updateDraggedItemPolygon(point) {
  const item = selectedItem();
  if (!item || !state.dragPolygonKey || state.dragPolygonIndex == null) return;
  const polygon = materializeItemPolygon(item, state.dragPolygonKey);
  const bounds = itemBounds(item);
  if (!polygon[state.dragPolygonIndex] || bounds.w <= 0 || bounds.h <= 0) return;
  const target = editToTargetPoint(point);
  polygon[state.dragPolygonIndex] = {
    x: Number(clampUnit((target.x - bounds.x) / bounds.w).toFixed(4)),
    y: Number(clampUnit((target.y - bounds.y) / bounds.h).toFixed(4))
  };
  syncPolygonEditorField(item, state.dragPolygonKey);
}

function selectedFace() {
  return state.faces.find((face) => face.id === state.selectedFaceId) || null;
}

function distance(a, b) {
  const dx = a.x - b.x;
  const dy = a.y - b.y;
  return Math.sqrt(dx * dx + dy * dy);
}

function pointInPolygon(point, points) {
  let inside = false;
  for (let i = 0, j = points.length - 1; i < points.length; j = i++) {
    const xi = points[i].x;
    const yi = points[i].y;
    const xj = points[j].x;
    const yj = points[j].y;
    const intersects = ((yi > point.y) !== (yj > point.y)) &&
      (point.x < (xj - xi) * (point.y - yi) / ((yj - yi) || 1e-6) + xi);
    if (intersects) inside = !inside;
  }
  return inside;
}

function hitTestFace(x, y) {
  const point = { x, y };
  for (let i = state.faces.length - 1; i >= 0; --i) {
    if (!faceVisible(state.faces[i])) continue;
    if (state.faces[i].points.length < 3) continue;
    if (pointInPolygon(point, state.faces[i].points)) return state.faces[i];
  }
  return null;
}

function hitTestPoint(x, y) {
  const radius = editHandleRadius() + 3;
  for (let pointIndex = state.points.length - 1; pointIndex >= 0; --pointIndex) {
    const point = state.points[pointIndex];
    if (!pointVisibleOnCanvas(point)) continue;
    if (distance({ x, y }, point) <= radius) {
      return { point };
    }
  }
  return null;
}

function hitTestLight(x, y) {
  return distance({ x, y }, lightEditPoint()) <= LIGHT_HANDLE_RADIUS + 5;
}

function hitTestConeControl(x, y) {
  if (!state.lightSelected || normalizeLightShape(editLightSettings().lightShape) !== "cone") return false;
  return distance({ x, y }, coneControlPoint()) <= LIGHT_HANDLE_RADIUS + 5;
}

function setLightPositionFromCanvasPoint(point) {
  const targetPoint = editToTargetPoint(point);
  const x = Math.round(clampNumber(targetPoint.x, 0, state.width));
  const y = Math.round(clampNumber(targetPoint.y, 0, state.height));
  const light = editLightSettings();
  light.lightX = x;
  light.lightY = y;
  document.getElementById("lightX").value = x;
  document.getElementById("lightY").value = y;
}

function closeDraftFace() {
  const uniquePointIds = new Set(state.draftPoints.map((point) => point.id));
  if (state.draftPoints.length < 3 || uniquePointIds.size < 3) return;
  const face = {
    id: `face${state.nextFaceId++}`,
    type: "floor",
    shadowSurface: "floor",
    receivesShadow: true,
    visible: true,
    points: state.draftPoints.slice()
  };
  state.faces.push(face);
  state.selectedFaceId = face.id;
  state.selectedPointIndex = 0;
  state.selectedPointId = face.points[0].id;
  state.expandedFaceIds.add(face.id);
  state.draftPoints = [];
  state.drawingFace = false;
  setStatus(`Closed ${face.id}. Set its type on the right.`);
  refreshFaceList();
  refreshSelectedFacePanel();
  pruneUnusedPoints();
  render();
}

function handleShapePointerDown(event) {
  const p = pointerToCanvas(event);
  const point = { x: Math.round(p.x), y: Math.round(p.y) };

  const pointHit = hitTestPoint(point.x, point.y);
  if (state.drawingFace) {
    if (pointHit) {
      const draftIndex = state.draftPoints.findIndex((entry) => entry.id === pointHit.point.id);
      if (draftIndex === 0 && state.draftPoints.length >= 3) {
        closeDraftFace();
        return;
      }
      if (draftIndex >= 0) {
        selectDraftPoint(draftIndex);
        state.selectedId = null;
        state.draggingPoint = true;
        state.dragPointId = pointHit.point.id;
        canvas.classList.add("dragging");
        canvas.setPointerCapture(event.pointerId);
      } else {
        addDraftPoint(pointHit.point);
        setStatus(`${state.draftPoints.length} point(s). Existing endpoint reused.`);
      }
    } else {
      addDraftPoint(createShapePoint(point.x, point.y));
      setStatus(`${state.draftPoints.length} point(s). Click the first point to close the face.`);
    }
    state.selectedId = null;
    state.selectedFaceId = null;
    updateEditModeButtons();
    refreshFaceList();
    refreshSelectedFacePanel();
    refreshList();
    refreshSelectedPanel();
    render();
    return;
  }

  if (pointHit) {
    const membership = findPointMembership(pointHit.point);
    setSelectedPoint(pointHit.point, membership);
    state.selectedId = null;
    state.draggingPoint = true;
    state.dragPointId = pointHit.point.id;
    canvas.classList.add("dragging");
    canvas.setPointerCapture(event.pointerId);
    refreshFaceList();
    refreshSelectedFacePanel();
    refreshList();
    refreshSelectedPanel();
    render();
    return;
  }

  if (!state.drawingFace && state.draftPoints.length === 0) {
    const face = hitTestFace(point.x, point.y);
    if (face) {
      state.selectedFaceId = face.id;
      state.selectedPointIndex = null;
      state.selectedPointId = null;
      state.selectedId = null;
      refreshFaceList();
      refreshSelectedFacePanel();
      render();
      return;
    }
  }

  state.selectedFaceId = null;
  state.selectedPointIndex = null;
  state.selectedPointId = null;
  state.selectedId = null;
  setStatus(state.draftPoints.length
    ? "Room face draft is paused. Click Add to continue drawing."
    : "Click Add in Room faces before drawing a new face.");
  refreshFaceList();
  refreshSelectedFacePanel();
  render();
}

function updateModeButtons() {
  document.getElementById("dayMode").classList.toggle("active", state.mode === "day");
  document.getElementById("nightMode").classList.toggle("active", state.mode === "night");
  state.lightProfile = currentLightProfile();
  updateLightModeActions();
}

function updateRoomFaceAddButton() {
  if (!addRoomFaceButton) return;
  const inShapeMode = state.editMode === "shape";
  const drawing = inShapeMode && state.drawingFace;
  const hasDraft = state.draftPoints.length > 0;
  addRoomFaceButton.hidden = false;
  addRoomFaceButton.classList.toggle("active", drawing);
  addRoomFaceButton.textContent = drawing ? "×" : "+";
  addRoomFaceButton.setAttribute("aria-pressed", drawing ? "true" : "false");
  addRoomFaceButton.setAttribute("aria-label", drawing
    ? "Pause room face drawing"
    : hasDraft ? "Continue room face draft" : "Add room face");
  canvas.classList.toggle("shape-tool", inShapeMode && !drawing);
  canvas.classList.toggle("face-draw-tool", drawing);
}

function setRoomFaceDrawing(active) {
  state.drawingFace = Boolean(active);
  state.draggingPoint = false;
  state.dragPointId = null;
  canvas.classList.remove("dragging");

  if (state.drawingFace) {
    state.editMode = "shape";
    state.toolMode = "select";
    state.selectedId = null;
    state.selectedFaceId = null;
    state.lightSelected = false;
    resetItemPolygonDraft();
    if (state.draftPoints.length) {
      if (!state.selectedPointId || !state.draftPoints.some((point) => point.id === state.selectedPointId)) {
        selectDraftPoint(0);
      }
      state.draftExpanded = true;
      setStatus(`Continuing room face draft with ${state.draftPoints.length} point(s). Click the first point to close.`);
    } else {
      state.selectedPointIndex = null;
      state.selectedPointId = null;
      setStatus("Room face drawing active. Click the canvas to add the first point.");
    }
  } else {
    setStatus(state.draftPoints.length
      ? "Room face drawing paused. Click Add to continue the existing draft."
      : "Room face drawing stopped. Click Add to draw another face.");
  }

  updateToolButtons();
  updateEditModeButtons();
  refreshFaceList();
  refreshSelectedFacePanel();
  refreshList();
  refreshSelectedPanel();
  render();
}

function toggleRoomFaceDrawing() {
  setRoomFaceDrawing(!state.drawingFace);
  commitHistory();
}

function updateEditModeButtons() {
  const isFurniture = state.editMode === "furniture";
  const hasSelectedItem = Boolean(selectedItem());
  document.getElementById("furnitureMode").classList.toggle("active", isFurniture);
  document.getElementById("shapeMode").classList.toggle("active", !isFurniture);

  document.getElementById("duplicateItem").disabled = !isFurniture || !hasSelectedItem;
  document.getElementById("deleteItem").disabled = !isFurniture || !hasSelectedItem;
  syncInspectorWorkflow();
  updateRoomFaceAddButton();
}

function setDetailsOpen(panel, open) {
  if (!panel || panel.hidden) return;
  panel.open = Boolean(open);
}

function syncInspectorWorkflow() {
  const isShape = state.editMode === "shape";
  const isLight = state.lightSelected;
  if (propertiesPanel) propertiesPanel.hidden = isShape || isLight;

  if (isLight) {
    setDetailsOpen(lightShadowsPanel, true);
    setDetailsOpen(roomFacesPanel, false);
    setDetailsOpen(layersPanel, false);
    return;
  }

  if (isShape) {
    setDetailsOpen(roomFacesPanel, true);
    setDetailsOpen(layersPanel, false);
    setDetailsOpen(lightShadowsPanel, false);
    return;
  }

  setDetailsOpen(layersPanel, !selectedItem());
  setDetailsOpen(roomFacesPanel, false);
  setDetailsOpen(lightShadowsPanel, false);
}

function updatePreviewZoom() {
  previewCanvas.classList.toggle("zoomed", state.previewZoomed);
}

function updateUndoButtons() {
  document.getElementById("undoAction").disabled = state.historyIndex <= 0;
  document.getElementById("redoAction").disabled = state.historyIndex >= state.history.length - 1;
}

function takeSnapshot() {
  const pointMap = new Map();
  const copyPoint = (p) => {
    if (!p) return p;
    if (pointMap.has(p.id)) return pointMap.get(p.id);
    const copy = { ...p };
    pointMap.set(p.id, copy);
    return copy;
  };

  return {
    width: state.width,
    height: state.height,
    editWidth: state.editWidth,
    editHeight: state.editHeight,
    editZoom: state.editZoom,
    pointCoordinateSpace: "edit",
    mode: state.mode,
    lightProfile: state.lightProfile,
    base: state.base ? { ...state.base } : null,
    backgroundItems: backgroundItems().map((background) => ({ ...background })),
    nextBackgroundId: state.nextBackgroundId,
    backgrounds: {
      day: state.backgrounds?.day ? { ...state.backgrounds.day } : null,
      night: state.backgrounds?.night ? { ...state.backgrounds.night } : null
    },
    baseFit: BASE_FIT_WIDTH,
    baseOpacity: state.baseOpacity,
    sourceScale: state.sourceScale,
    baseTrim: state.baseTrim ? { ...state.baseTrim } : null,
    furnitureImportMode: state.furnitureImportMode,
    aspectLocked: state.aspectLocked,
    aspectRatio: state.aspectRatio,
    items: state.items.map((item) => {
      const { originalDataUrl, ...snapshotItem } = item;
      return snapshotItem;
    }),
    selectedId: state.selectedId,
    nextId: state.nextId,
    points: state.points.map(copyPoint),
    faces: state.faces.map((face) => ({
      ...face,
      points: face.points.map(copyPoint)
    })),
    selectedFaceId: state.selectedFaceId,
    selectedPointIndex: state.selectedPointIndex,
    selectedPointId: state.selectedPointId,
    expandedFaceIds: [...state.expandedFaceIds],
    draftExpanded: state.draftExpanded,
    nextFaceId: state.nextFaceId,
    nextPointId: state.nextPointId,
    draftPoints: state.draftPoints.map(copyPoint),
    drawingFace: state.drawingFace,
    guides: { ...state.guides },
    night: JSON.parse(JSON.stringify(state.night)),
    showProjectionGuides: state.showProjectionGuides,
    showGrid: state.showGrid,
    gridOpacity: state.gridOpacity,
    snapToGrid: state.snapToGrid
  };
}

function createSessionSnapshot() {
  const snapshot = JSON.parse(JSON.stringify(takeSnapshot(), (key, value) => {
    if (key === "img") return undefined;
    return value;
  }));
  return compactSessionSnapshot(snapshot);
}

function compactSessionSnapshot(snapshot) {
  const compacted = snapshot || {};
  const baseDataUrl = compacted.base?.dataUrl || "";
  for (const mode of BACKGROUND_MODES) {
    const background = compacted.backgrounds?.[mode];
    if (background?.dataUrl && baseDataUrl && background.dataUrl === baseDataUrl) {
      delete background.dataUrl;
      background.usesBaseDataUrl = true;
    }
  }
  for (const item of compacted.items || []) {
    if (item.source === "furniture" && item.libraryId && item.dataUrl) {
      delete item.dataUrl;
      delete item.originalDataUrl;
      item.usesLibraryImage = true;
    }
  }
  return compacted;
}

function createSessionRecord() {
  const snapshot = createSessionSnapshot();
  return {
    version: 1,
    type: "stickmon-room-editor-session",
    savedAt: new Date().toISOString(),
    files: {
      base: snapshot.base?.name || "",
      backgrounds: {
        day: snapshot.backgrounds?.day?.name || "",
        night: snapshot.backgrounds?.night?.name || ""
      },
      backgroundItems: (snapshot.backgroundItems || []).map((background) => ({
        id: background.id,
        name: background.name,
        fileName: background.fileName || background.name,
        visible: background.visible !== false,
        visibleInDay: background.visibleInDay !== false,
        visibleInNight: background.visibleInNight !== false
      })),
      items: (snapshot.items || []).map((item) => ({
        id: item.id,
        name: item.name,
        fileName: item.fileName,
        source: item.source || "furniture",
        furnitureType: item.furnitureType || "",
        kind: item.kind || ""
      }))
    },
    snapshot
  };
}

function createSessionFileName(record) {
  const stamp = String(record.savedAt || new Date().toISOString())
    .replace(/[:.]/g, "-")
    .replace("T", "_")
    .replace("Z", "");
  return `stickmon_room_editor_session_${stamp}.json`;
}

function isFilePickerAbort(error) {
  return error?.name === "AbortError";
}

function openSessionDb() {
  if (!window.indexedDB) return Promise.reject(new Error("IndexedDB is not available."));
  return new Promise((resolve, reject) => {
    const request = indexedDB.open(SESSION_DB_NAME, 1);
    request.onupgradeneeded = () => {
      const db = request.result;
      if (!db.objectStoreNames.contains(SESSION_DB_STORE)) {
        db.createObjectStore(SESSION_DB_STORE);
      }
    };
    request.onerror = () => reject(request.error);
    request.onsuccess = () => resolve(request.result);
  });
}

async function writeSessionToIndexedDb(record) {
  const db = await openSessionDb();
  try {
    await new Promise((resolve, reject) => {
      const transaction = db.transaction(SESSION_DB_STORE, "readwrite");
      transaction.objectStore(SESSION_DB_STORE).put(record, SESSION_DB_KEY);
      transaction.oncomplete = resolve;
      transaction.onerror = () => reject(transaction.error);
      transaction.onabort = () => reject(transaction.error);
    });
  } finally {
    db.close();
  }
}

async function readSessionFromIndexedDb() {
  const db = await openSessionDb();
  try {
    return await new Promise((resolve, reject) => {
      const transaction = db.transaction(SESSION_DB_STORE, "readonly");
      const request = transaction.objectStore(SESSION_DB_STORE).get(SESSION_DB_KEY);
      request.onsuccess = () => resolve(request.result || null);
      request.onerror = () => reject(request.error);
    });
  } finally {
    db.close();
  }
}

async function clearSessionFromIndexedDb() {
  const db = await openSessionDb();
  try {
    await new Promise((resolve, reject) => {
      const transaction = db.transaction(SESSION_DB_STORE, "readwrite");
      transaction.objectStore(SESSION_DB_STORE).delete(SESSION_DB_KEY);
      transaction.oncomplete = resolve;
      transaction.onerror = () => reject(transaction.error);
      transaction.onabort = () => reject(transaction.error);
    });
  } finally {
    db.close();
  }
}

async function saveSessionRecord(record) {
  try {
    await writeSessionToIndexedDb(record);
    try {
      localStorage.removeItem(SESSION_STORAGE_KEY);
    } catch (_) {}
    return "IndexedDB draft";
  } catch (dbError) {
    try {
      localStorage.setItem(SESSION_STORAGE_KEY, JSON.stringify(record));
      return "localStorage draft";
    } catch (storageError) {
      throw new Error(`Could not save session. IndexedDB: ${dbError.message}; localStorage: ${storageError.message}`);
    }
  }
}

async function readSessionDraftRecord() {
  try {
    const record = await readSessionFromIndexedDb();
    if (record) return { record, backend: "IndexedDB draft" };
  } catch (_) {}
  try {
    const raw = localStorage.getItem(SESSION_STORAGE_KEY);
    return raw ? { record: JSON.parse(raw), backend: "localStorage draft" } : null;
  } catch (_) {
    return null;
  }
}

async function readSessionRecord() {
  const draft = await readSessionDraftRecord();
  return draft?.record || null;
}

async function clearSessionRecord() {
  try {
    await clearSessionFromIndexedDb();
  } catch (_) {}
  try {
    localStorage.removeItem(SESSION_STORAGE_KEY);
  } catch (_) {}
}

function updateSessionExportMeta(text) {
  if (sessionExportMeta) sessionExportMeta.textContent = text;
}

async function verifyFileHandlePermission(handle, mode = "readwrite") {
  if (!handle?.queryPermission || !handle?.requestPermission) return false;
  const options = { mode };
  if (await handle.queryPermission(options) === "granted") return true;
  return await handle.requestPermission(options) === "granted";
}

async function writeSessionFile(record, forcePick = false) {
  const text = JSON.stringify(record, null, 2);
  if (!window.showDirectoryPicker) {
    downloadText(createSessionFileName(record), text, "application/json");
    return "downloaded";
  }

  const rootHandle = await ensureRoomEditorRootHandle(forcePick);
  if (!rootHandle) {
    downloadText(createSessionFileName(record), text, "application/json");
    return "downloaded";
  }

  const sessionsDirectory = await getOrCreateDirectoryHandle(rootHandle, ["sessions"]);
  await writeFileText(sessionsDirectory, SESSION_DEFAULT_FILE_NAME, `${text}\n`);
  return `updated sessions/${SESSION_DEFAULT_FILE_NAME}`;
}

function parseSessionRecordText(text, sourceLabel) {
  const record = JSON.parse(text);
  if (!record?.snapshot) {
    throw new Error(`${sourceLabel} is not a StickMon room editor session.`);
  }
  return record;
}

async function readProjectSessionFromHandle(forcePick = false) {
  if (!window.showDirectoryPicker) return null;

  let handle = null;
  if (forcePick) {
    handle = await ensureRoomEditorRootHandle(true);
  } else {
    try {
      handle = await readRoomEditorRootHandle();
    } catch (_) {}
    if (!handle) return null;
    try {
      const hasReadPermission = !handle.queryPermission ||
        await handle.queryPermission({ mode: "read" }) === "granted" ||
        await handle.queryPermission({ mode: "readwrite" }) === "granted";
      if (!hasReadPermission) return null;
      await validateRoomEditorRootHandle(handle);
      roomEditorRootHandle = handle;
    } catch (_) {
      roomEditorRootHandle = null;
      return null;
    }
  }

  try {
    const fileHandle = await getExistingFileHandle(handle, ["sessions", SESSION_DEFAULT_FILE_NAME]);
    const text = await readFileText(fileHandle);
    return {
      record: parseSessionRecordText(text, `sessions/${SESSION_DEFAULT_FILE_NAME}`),
      backend: `project sessions/${SESSION_DEFAULT_FILE_NAME}`
    };
  } catch (error) {
    if (error?.name === "NotFoundError") return null;
    throw error;
  }
}

async function readProjectSessionFromFetch() {
  if (!window.fetch) return null;
  try {
    const response = await fetch(`sessions/${SESSION_DEFAULT_FILE_NAME}?t=${Date.now()}`, { cache: "no-store" });
    if (!response.ok) return null;
    const text = await response.text();
    return {
      record: parseSessionRecordText(text, `sessions/${SESSION_DEFAULT_FILE_NAME}`),
      backend: `project sessions/${SESSION_DEFAULT_FILE_NAME}`
    };
  } catch (error) {
    if (error instanceof SyntaxError) throw error;
    return null;
  }
}

async function readProjectSessionRecord(options = {}) {
  const fromHandle = await readProjectSessionFromHandle(options.forcePick === true);
  if (fromHandle) return fromHandle;
  if (options.allowFetch !== false) {
    const fromFetch = await readProjectSessionFromFetch();
    if (fromFetch) return fromFetch;
  }
  return null;
}

async function hydrateBackgroundSnapshot(background) {
  if (!background?.dataUrl) return background || null;
  const hydrated = { ...background };
  const img = await loadImageSource(hydrated.dataUrl);
  hydrated.img = img;
  hydrated.width = hydrated.width || img.naturalWidth;
  hydrated.height = hydrated.height || img.naturalHeight;
  return hydrated;
}

async function hydrateBackgroundItemSnapshot(background, index = 0) {
  if (!background?.dataUrl) return null;
  const hydrated = await hydrateBackgroundSnapshot(background);
  return {
    ...hydrated,
    id: hydrated.id || `b${index + 1}`,
    name: hydrated.name || hydrated.fileName || `background_${index + 1}.png`,
    fileName: hydrated.fileName || hydrated.name || "",
    visible: hydrated.visible !== false,
    visibleInDay: hydrated.visibleInDay !== false,
    visibleInNight: hydrated.visibleInNight !== false,
    z: Number(hydrated.z ?? index)
  };
}

async function hydrateLegacyBackgroundItems(snapshot) {
  const legacy = [];
  const day = snapshot.backgrounds?.day || null;
  const night = snapshot.backgrounds?.night || null;
  if (day?.dataUrl && night?.dataUrl && day.dataUrl === night.dataUrl) {
    legacy.push(await hydrateBackgroundItemSnapshot({
      ...day,
      id: "b1",
      visibleInDay: true,
      visibleInNight: true,
      z: 0
    }, 0));
  } else {
    if (day?.dataUrl) {
      legacy.push(await hydrateBackgroundItemSnapshot({
        ...day,
        id: "b1",
        visibleInDay: true,
        visibleInNight: false,
        z: 0
      }, legacy.length));
    }
    if (night?.dataUrl) {
      legacy.push(await hydrateBackgroundItemSnapshot({
        ...night,
        id: `b${legacy.length + 1}`,
        visibleInDay: false,
        visibleInNight: true,
        z: legacy.length
      }, legacy.length));
    }
  }
  return legacy.filter(Boolean);
}

async function hydrateSessionSnapshot(snapshot) {
  const hydrated = JSON.parse(JSON.stringify(snapshot));
  if (hydrated.base?.dataUrl) {
    hydrated.base = await hydrateBackgroundSnapshot(hydrated.base);
  }
  for (const mode of BACKGROUND_MODES) {
    const background = hydrated.backgrounds?.[mode];
    if (background?.usesBaseDataUrl && hydrated.base?.dataUrl) {
      background.dataUrl = hydrated.base.dataUrl;
    }
  }
  hydrated.backgrounds = {
    day: await hydrateBackgroundSnapshot(hydrated.backgrounds?.day || null),
    night: await hydrateBackgroundSnapshot(hydrated.backgrounds?.night || null)
  };
  if (!hydrated.backgrounds.day && !hydrated.backgrounds.night && hydrated.base) {
    hydrated.backgrounds.day = hydrated.base;
  }
  if (!hydrated.base) {
    hydrated.base = hydrated.backgrounds.day || hydrated.backgrounds.night || null;
  }

  if (Array.isArray(hydrated.backgroundItems) && hydrated.backgroundItems.length) {
    hydrated.backgroundItems = (await Promise.all(
      hydrated.backgroundItems.map((background, index) => hydrateBackgroundItemSnapshot(background, index))
    )).filter(Boolean);
  } else {
    hydrated.backgroundItems = await hydrateLegacyBackgroundItems(hydrated);
  }
  hydrated.nextBackgroundId = Math.max(
    Number(hydrated.nextBackgroundId) || 1,
    ...hydrated.backgroundItems.map((background) => {
      const match = String(background.id || "").match(/(\d+)$/);
      return match ? Number(match[1]) + 1 : 1;
    })
  );

  const hydratedItems = await Promise.all((hydrated.items || []).map(async (item) => {
    const next = { ...item };
    if (!next.dataUrl) {
      const libraryEntry = (next.libraryId ? furnitureLibraryEntryById(next.libraryId) : null) ||
        furnitureLibraryEntryByItemReference(next);
      if (libraryEntry?.dataUrl || libraryEntry?.image) {
        next.dataUrl = libraryEntry.dataUrl || libraryEntry.image;
        next.fileName = next.fileName || libraryEntry.fileName;
        next.sourceWidth = next.sourceWidth || libraryEntry.sourceWidth;
        next.sourceHeight = next.sourceHeight || libraryEntry.sourceHeight;
        next.libraryId = next.libraryId || libraryEntry.id;
      } else if (next.visible === false) {
        return null;
      } else {
        throw new Error(`Saved item is missing image data: ${next.fileName || next.name || next.id}`);
      }
    }
    const img = await loadImageSource(next.dataUrl);
    next.img = img;
    next.sourceWidth = next.sourceWidth || img.naturalWidth;
    next.sourceHeight = next.sourceHeight || img.naturalHeight;
    next.scaleX = next.scaleX ?? next.scale ?? 1;
    next.scaleY = next.scaleY ?? next.scale ?? 1;
    next.aspectLocked = next.aspectLocked ?? Math.abs(next.scaleX - next.scaleY) < 0.0001;
    if (next.source === "furniture") {
      next.furnitureType = normalizeFurnitureType(next.furnitureType || inferFurnitureType(next.fileName || next.name));
    }
    normalizeItemSemantics(next);
    return next;
  }));
  hydrated.items = hydratedItems.filter(Boolean);
  return hydrated;
}

function refreshAfterStateRestore() {
  syncInputsFromState();
  refreshList();
  refreshSelectedPanel();
  refreshFaceList();
  refreshSelectedFacePanel();
  updateBaseImageMeta();
  updateModeButtons();
  updateEditModeButtons();
  updateToolButtons();
  updatePreviewZoom();
  updateZoomControls();
  updateGridOpacityLabel();
  render();
  updateUndoButtons();
}

function sessionSavedAtLabel(record) {
  return record?.savedAt ? new Date(record.savedAt).toLocaleString() : "unknown time";
}

async function saveEditorSession(options = {}) {
  const record = createSessionRecord();
  let exportResult = "";
  try {
    exportResult = await writeSessionFile(record, options.forcePick === true);
  } catch (error) {
    if (isFilePickerAbort(error)) {
      exportResult = "export location not selected";
    } else {
      console.error(error);
      exportResult = `export failed: ${error.message || error}`;
    }
  }
  const draftBackend = await saveSessionRecord(record);
  const itemCount = record.snapshot.items?.length || 0;
  const dayName = record.files.backgrounds?.day || record.files.base || "no day background";
  const nightName = record.files.backgrounds?.night || "no night background";
  updateSessionExportMeta(`Last save: project ${exportResult}; draft ${draftBackend}.`);
  setStatus(`Session saved: project ${exportResult}; draft ${draftBackend}; day ${dayName}, night ${nightName}, ${itemCount} item(s).`);
}

async function loadEditorSession() {
  const projectSession = await readProjectSessionRecord();
  if (projectSession?.record?.snapshot) {
    await restoreSessionRecord(projectSession.record);
    updateSessionExportMeta(`Loaded ${projectSession.backend} saved at ${sessionSavedAtLabel(projectSession.record)}.`);
    setStatus(`Session loaded from ${projectSession.backend}.`);
    return;
  }

  const draftSession = await readSessionDraftRecord();
  if (!draftSession?.record?.snapshot) {
    setStatus("No saved session found.");
    return;
  }
  await restoreSessionRecord(draftSession.record);
  updateSessionExportMeta(`Recovered ${draftSession.backend} saved at ${sessionSavedAtLabel(draftSession.record)}. Save session to update the project session JSON.`);
  setStatus(`Unsaved draft recovered from ${draftSession.backend}.`);
}

async function restoreSessionRecord(record) {
  const snapshot = await hydrateSessionSnapshot(record.snapshot);
  restoreSnapshot(snapshot);
  state.drawingFace = false;
  state.history = [];
  state.historyIndex = -1;
  refreshAfterStateRestore();
  commitHistory();
}

async function tryAutoLoadEditorSession() {
  try {
    const projectSession = await readProjectSessionRecord();
    if (!projectSession?.record?.snapshot) {
      const draftSession = await readSessionDraftRecord();
      if (draftSession?.record?.snapshot) {
        updateSessionExportMeta(`Project session not auto-loaded. Unsaved draft available from ${draftSession.backend}, saved at ${sessionSavedAtLabel(draftSession.record)}. Click Load session to recover it.`);
      }
      return false;
    }
    await restoreSessionRecord(projectSession.record);
    updateSessionExportMeta(`Auto-loaded ${projectSession.backend} saved at ${sessionSavedAtLabel(projectSession.record)}.`);
    return true;
  } catch (error) {
    console.warn("Project session auto-load failed; keeping empty room editor.", error);
    return false;
  }
}

async function clearEditorSession() {
  await clearSessionRecord();
  updateSessionExportMeta("Unsaved browser draft cleared. Project session JSON is unchanged.");
  setStatus("Unsaved browser draft cleared.");
}

function restoreSnapshot(snapshot) {
  const legacySourceCoordinateSpace =
    Number(snapshot.editWidth || 0) > Math.max(GAME_SCREEN_WIDTH * 1.5, Number(snapshot.width || 0) * 1.5);
  const pointCoordinateSpace = snapshot.pointCoordinateSpace || (legacySourceCoordinateSpace ? "source" : "target");
  const pointMap = new Map();
  for (const p of snapshot.points || []) {
    pointMap.set(p.id, { ...p });
  }
  const restorePoint = (p) => pointMap.get(p?.id);

  state.width = snapshot.width;
  state.height = snapshot.height;
  state.editWidth = snapshot.editWidth;
  state.editHeight = snapshot.editHeight;
  state.editZoom = normalizedEditZoom(snapshot.editZoom == null ? 100 : Number(snapshot.editZoom) <= 3 ? Number(snapshot.editZoom) * 100 : snapshot.editZoom);
  state.mode = snapshot.mode;
  state.lightProfile = currentLightProfile();
  state.backgroundItems = Array.isArray(snapshot.backgroundItems)
    ? snapshot.backgroundItems.map((background, index) => ({
        ...background,
        id: background.id || `b${index + 1}`,
        fileName: background.fileName || background.name || "",
        visible: background.visible !== false,
        visibleInDay: background.visibleInDay !== false,
        visibleInNight: background.visibleInNight !== false,
        z: Number(background.z ?? index)
      }))
    : [];
  state.nextBackgroundId = Math.max(
    Number(snapshot.nextBackgroundId) || 1,
    ...state.backgroundItems.map((background) => {
      const match = String(background.id || "").match(/(\d+)$/);
      return match ? Number(match[1]) + 1 : 1;
    })
  );
  state.backgrounds = {
    day: snapshot.backgrounds?.day || snapshot.base || null,
    night: snapshot.backgrounds?.night || null
  };
  setLegacyBaseFromBackgrounds();
  state.baseFit = BASE_FIT_WIDTH;
  state.baseOpacity = snapshot.baseOpacity;
  state.sourceScale = snapshot.sourceScale;
  state.baseTrim = snapshot.baseTrim ? { ...snapshot.baseTrim } : null;
  state.furnitureImportMode = snapshot.furnitureImportMode;
  state.aspectLocked = snapshot.aspectLocked;
  state.aspectRatio = snapshot.aspectRatio;
  state.items = (snapshot.items || []).map((item) => ({ ...item }));
  state.selectedId = snapshot.selectedId;
  state.nextId = snapshot.nextId;
  state.points = Array.from(pointMap.values());
  state.faces = (snapshot.faces || []).map((face) => {
    const type = normalizeFaceType(face.type);
    const semantic = type === FACE_TYPE_SPRITE_AREA || type === FACE_TYPE_DOORWAY;
    return {
      ...face,
      type,
      visible: face.visible !== false,
      shadowSurface: normalizeShadowSurface(face.shadowSurface, type),
      receivesShadow: semantic ? false : face.receivesShadow !== false,
      points: face.points.map(restorePoint).filter(Boolean)
    };
  });
  state.selectedFaceId = snapshot.selectedFaceId;
  state.selectedPointIndex = snapshot.selectedPointIndex;
  state.selectedPointId = snapshot.selectedPointId;
  state.expandedFaceIds = new Set(snapshot.expandedFaceIds || []);
  state.draftExpanded = snapshot.draftExpanded ?? true;
  state.nextFaceId = snapshot.nextFaceId;
  state.nextPointId = snapshot.nextPointId;
  state.draftPoints = (snapshot.draftPoints || []).map(restorePoint).filter(Boolean);
  state.drawingFace = snapshot.drawingFace;
  state.guides = { ...snapshot.guides };
  state.night = normalizeNightSettings(snapshot.night);
  ensureSeparateModeLights();
  state.lightProfile = currentLightProfile();
  state.showProjectionGuides = snapshot.showProjectionGuides === true;
  state.showGrid = snapshot.showGrid;
  state.gridOpacity = clampNumber(snapshot.gridOpacity ?? 0.18, 0, 1);
  state.snapToGrid = snapshot.snapToGrid;
  syncCanvasSizeToPreparedRoom();
  if (pointCoordinateSpace !== "edit") {
    for (const point of state.points) {
      const target = pointCoordinateSpace === "source" ? sourcePointToTargetPoint(point) : point;
      const edit = targetToEditPoint(target);
      point.x = Math.round(clampNumber(edit.x, 0, state.editWidth));
      point.y = Math.round(clampNumber(edit.y, 0, state.editHeight));
    }
  }
}

function snapshotCompareKey(snapshot) {
  return JSON.stringify(snapshot, (key, value) => {
    if (key === "img" || key === "dataUrl" || key === "originalDataUrl") return undefined;
    if (key === "editZoom") return undefined;
    return value;
  });
}

function snapshotsEqual(a, b) {
  if (!a || !b) return false;
  return snapshotCompareKey(a) === snapshotCompareKey(b);
}

function commitHistory() {
  const snapshot = takeSnapshot();
  const current = state.history[state.historyIndex];
  if (snapshotsEqual(current, snapshot)) {
    updateUndoButtons();
    return;
  }
  state.history = state.history.slice(0, state.historyIndex + 1);
  state.history.push(snapshot);
  if (state.history.length > state.maxHistory) {
    state.history.shift();
  }
  state.historyIndex = state.history.length - 1;
  updateUndoButtons();
}

function undo() {
  if (state.historyIndex <= 0) return;
  state.historyIndex -= 1;
  restoreSnapshot(state.history[state.historyIndex]);
  syncInputsFromState();
  refreshList();
  refreshSelectedPanel();
  refreshFaceList();
  refreshSelectedFacePanel();
  updateEditModeButtons();
  render();
  updateUndoButtons();
  setStatus("Undone.");
}

function redo() {
  if (state.historyIndex >= state.history.length - 1) return;
  state.historyIndex += 1;
  restoreSnapshot(state.history[state.historyIndex]);
  syncInputsFromState();
  refreshList();
  refreshSelectedPanel();
  refreshFaceList();
  refreshSelectedFacePanel();
  updateEditModeButtons();
  render();
  updateUndoButtons();
  setStatus("Redone.");
}

function syncInputsFromState() {
  document.getElementById("canvasW").value = state.width;
  document.getElementById("canvasH").value = state.height;
  document.getElementById("aspectLock").checked = state.aspectLocked;
  document.getElementById("baseOpacity").value = Math.round(state.baseOpacity * 100);
  document.getElementById("sourceScale").value = state.sourceScale;
  document.getElementById("furnitureImportMode").value = state.furnitureImportMode;
  document.getElementById("baseFit").value = BASE_FIT_WIDTH;
  document.getElementById("showGuides").checked = state.guides.show;
  document.getElementById("showGrid").checked = state.showGrid;
  document.getElementById("gridOpacity").value = Math.round(state.gridOpacity * 100);
  document.getElementById("snapToGrid").checked = state.snapToGrid;
  document.getElementById("spriteX").value = state.guides.spriteX;
  document.getElementById("spriteY").value = state.guides.spriteY;
  document.getElementById("spriteW").value = state.guides.spriteW;
  document.getElementById("spriteH").value = state.guides.spriteH;
  syncLightInputsFromState();
  document.getElementById("castShadows").checked = state.night.castShadows;
  document.getElementById("shadowMode").value = state.night.shadowMode;
  updateBaseImageMeta();
  updateModeButtons();
  updateEditModeButtons();
  updateToolButtons();
  updateZoomControls();
  updateGridOpacityLabel();
}

function updateWorkflowStatus() {
  if (!hasBackground()) {
    setWorkflowStatus("Step 1/4: Drop a reference image to start.");
    return;
  }
  if (state.faces.length === 0) {
    setWorkflowStatus("Step 2/4: Switch to Shape mode and trace wall, floor, or sprite-area faces.");
    return;
  }
  if (state.items.length === 0) {
    setWorkflowStatus("Step 3/4: Switch to Furniture mode and import PNG furniture.");
    return;
  }
  setWorkflowStatus("Step 4/4: Tune preview, add sprites, and export.");
}

function sharedPointFaceCount(point) {
  if (!point) return 0;
  let count = 0;
  if (state.draftPoints.some((entry) => entry.id === point.id)) count += 1;
  for (const face of state.faces) {
    if (face.points.some((entry) => entry.id === point.id)) count += 1;
  }
  return count;
}

function updateSharedPointHint() {
  const point = selectedPoint();
  if (!point || state.editMode !== "shape") return;
  const count = sharedPointFaceCount(point);
  if (count > 1) {
    setWorkflowStatus(`Shared point used by ${count} faces/drafts. Dragging affects all of them.`);
  }
}

function exportBackgroundInfo(mode, prepared = preparedRoomMetrics()) {
  const normalized = normalizeBackgroundMode(mode);
  const bg = firstBackgroundLayerForMode(normalized) || (state.backgrounds || {})[normalized];
  return {
    fileName: bg ? (bg.fileName || bg.name) : "",
    role: normalized,
    loaded: Boolean(bg),
    sourceWidth: bg ? bg.width : 0,
    sourceHeight: bg ? bg.height : 0,
    layerCount: backgroundLayersForMode(normalized).length || (bg ? 1 : 0),
    fit: BASE_FIT_WIDTH,
    trim: { ...prepared.trim }
  };
}

function exportBackgroundLayers(prepared = preparedRoomMetrics()) {
  return backgroundItems().map((background, index) => ({
    id: background.id || `b${index + 1}`,
    name: background.name,
    fileName: background.fileName || background.name,
    z: Number(background.z ?? index),
    visible: background.visible !== false,
    visibleInDay: background.visibleInDay !== false,
    visibleInNight: background.visibleInNight !== false,
    sourceWidth: background.width || 0,
    sourceHeight: background.height || 0,
    fit: BASE_FIT_WIDTH,
    trim: { ...prepared.trim }
  }));
}

function exportProjectionVector(point) {
  return [Number(point.x.toFixed(4)), Number(point.y.toFixed(4))];
}

function exportRoomProjectionObject() {
  const model = roomProjectionModel(faceTargetPoints);
  if (!model) return null;
  return {
    type: model.type,
    origin: exportProjectionVector(model.origin),
    axisU: exportProjectionVector(model.axisU),
    axisV: exportProjectionVector(model.axisV),
    axisZ: exportProjectionVector(model.axisZ),
    planes: {
      floor: { faceId: model.faces.floor.id, coordinate: "z", value: 0 },
      leftWall: { faceId: model.faces.left_wall.id, coordinate: "v", value: 0 },
      rightWall: { faceId: model.faces.right_wall.id, coordinate: "u", value: 0 }
    }
  };
}

function exportItemProjectionObject(item) {
  const model = roomProjectionModel(faceTargetPoints);
  if (!model) return null;
  const bounds = itemBounds(item);
  const anchorPoint = { x: bounds.x + bounds.w * 0.5, y: bounds.y + bounds.h * 0.88 };
  const usesWallAnchor = itemUsesWallShadowAnchor(item);
  const explicitFaceId = itemShadowFaceIds(item).find((id) => model.points.has(String(id)));
  const anchorFace = usesWallAnchor ?
    (explicitFaceId ? Object.values(model.faces).find((face) => String(face.id) === explicitFaceId) :
      Object.values(model.faces).find((face) => String(face.id) === automaticWallShadowFaceId(item))) :
    model.faces.floor;
  if (!anchorFace) return null;

  let anchorUV;
  if (anchorFace === model.faces.floor) {
    const world = screenPointToProjectionWorld(model, anchorPoint, 0);
    anchorUV = [world.u, world.v];
  } else {
    const basis = faceProjectionBasis(model.points.get(String(anchorFace.id)) || []);
    if (!basis) return null;
    const local = screenToFaceLocal(basis, anchorPoint);
    anchorUV = [local.u / basis.uLength, local.v / basis.vLength];
  }
  return {
    model: model.type,
    anchorFaceId: anchorFace.id,
    anchorUV: anchorUV.map((value) => Number(value.toFixed(5))),
    heightPx: Math.round(itemHeightPx(item)),
    depthPx: Math.round(itemWallShadowDepthPx(item)),
    casterContour: compactLocalPolygon(item.shadowPolygon)
  };
}

function exportLayoutObject() {
  const prepared = preparedRoomMetrics();
  const primary = primaryBackground();
  return {
    version: 4,
    canvas: {
      width: state.width,
      height: state.height,
      screenWidth: GAME_SCREEN_WIDTH,
      screenHeight: GAME_SCREEN_HEIGHT
    },
    base: {
      fileName: primary ? (primary.fileName || primary.name) : "",
      fit: BASE_FIT_WIDTH,
      sourceWidth: primary ? primary.width : 0,
      sourceHeight: primary ? primary.height : 0,
      trim: { ...prepared.trim }
    },
    backgrounds: {
      activeMode: normalizeBackgroundMode(state.mode),
      sharedTransform: true,
      fit: BASE_FIT_WIDTH,
      trim: { ...prepared.trim },
      day: exportBackgroundInfo("day", prepared),
      night: exportBackgroundInfo("night", prepared)
    },
    backgroundLayers: exportBackgroundLayers(prepared),
    sourceScale: Number(state.sourceScale.toFixed(4)),
    baseOpacity: Number(state.baseOpacity.toFixed(4)),
    roomGeometry: exportRoomGeometryObject(),
    guides: state.guides,
    night: state.night,
    furniture: sortedItems()
      .filter((item) => (item.source || "furniture") === "furniture")
      .map((item) => ({
      id: item.id,
      name: item.name,
      fileName: item.fileName,
      x: Math.round(item.x),
      y: Math.round(item.y),
      z: item.z,
      scale: Number(itemScaleX(item).toFixed(4)),
      scaleX: Number(itemScaleX(item).toFixed(4)),
      scaleY: Number(itemScaleY(item).toFixed(4)),
      aspectLocked: itemAspectLocked(item),
      targetWidth: Math.round(itemTargetWidth(item)),
      targetHeight: Math.round(itemTargetHeight(item)),
      sourceScale: Number((item.sourceScale || state.sourceScale).toFixed(4)),
      opacity: Number(item.opacity.toFixed(4)),
      visible: item.visible,
      visibleInDay: item.visibleInDay !== false,
      visibleInNight: item.visibleInNight !== false,
      anchor: item.anchor,
      slot: itemKindPreset(item.kind).slot,
      layer: itemKindPreset(item.kind).layer,
      source: "furniture",
      libraryId: item.libraryId || "",
      libraryRevision: item.libraryRevision || 0,
      furnitureType: normalizeFurnitureType(item.furnitureType),
      kind: normalizeItemKind(item.kind),
      heightPx: Math.round(itemHeightPx(item)),
      footprint: normalizeFootprint(item.footprint),
      footprintPolygon: compactLocalPolygon(item.footprintPolygon),
      shadowPolygon: compactLocalPolygon(item.shadowPolygon),
      shadowAnchor: normalizeShadowAnchor(item.shadowAnchor),
      shadowFaceIds: itemShadowFaceIds(item),
      wallShadowDepthPx: Math.round(itemWallShadowDepthPx(item)),
      wallShadowOffsetY: Math.round(itemWallShadowOffsetY(item)),
      castsShadow: itemCastsShadow(item),
      shadowOpacity: Math.round(itemShadowOpacity(item)),
      shadowLength: Math.round(itemShadowLength(item)),
      shadowBlur: Number(itemShadowBlur(item).toFixed(1)),
      projection: exportItemProjectionObject(item),
      receivesShadow: item.receivesShadow !== false,
      occludesSprite: !!item.occludesSprite,
      sortY: Math.round(itemSortY(item)),
      cutoutApplied: !!item.cutoutApplied,
      speciesId: item.speciesId || 0,
      action: item.action || "",
      frame: item.frame || "",
      sourceWidth: item.sourceWidth,
      sourceHeight: item.sourceHeight
    }))
  };
}

function exportRoomGeometryObject() {
  const prepared = preparedRoomMetrics();
  const primary = primaryBackground();
  return {
    version: 3,
    coordinateSpace: "target",
    projection: exportRoomProjectionObject(),
    room: {
      width: prepared.width,
      height: prepared.height
    },
    viewport: {
      width: GAME_SCREEN_WIDTH,
      height: GAME_SCREEN_HEIGHT
    },
    canvas: {
      width: state.width,
      height: state.height
    },
    edit: {
      width: state.editWidth,
      height: state.editHeight
    },
    prepared: {
      width: prepared.width,
      height: prepared.height,
      y: prepared.y,
      scale: Number(prepared.scale.toFixed(8)),
      trim: { ...prepared.trim }
    },
    reference: {
      fileName: primary ? (primary.fileName || primary.name) : "",
      sourceWidth: primary ? primary.width : 0,
      sourceHeight: primary ? primary.height : 0,
      trim: { ...prepared.trim },
      fit: BASE_FIT_WIDTH,
      opacity: Number(state.baseOpacity.toFixed(4))
    },
    backgrounds: {
      day: exportBackgroundInfo("day", prepared),
      night: exportBackgroundInfo("night", prepared),
      layers: exportBackgroundLayers(prepared)
    },
    faces: state.faces.map((face, index) => ({
      id: face.id,
      type: normalizeFaceType(face.type),
      visible: !isSemanticAreaFace(face),
      editorVisible: faceVisible(face),
      shadowSurface: shadowSurfaceForFace(face),
      receivesShadow: !isSemanticAreaFace(face) && face.receivesShadow !== false,
      drawOrder: index,
      points: faceTargetPoints(face).map((point) => [Math.round(point.x), Math.round(point.y)]),
      sourcePoints: faceSourcePoints(face)
    }))
  };
}

function downloadText(fileName, text, mime = "text/plain") {
  const blob = new Blob([text], { type: mime });
  downloadBlob(fileName, blob);
}

function downloadBlob(fileName, blob) {
  if (!blob) {
    setStatus(`Export failed: ${fileName}.`);
    return;
  }
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = fileName;
  document.body.appendChild(a);
  a.click();
  a.remove();
  window.setTimeout(() => URL.revokeObjectURL(url), 1000);
}

function dataUrlToBlob(dataUrl) {
  const [header, body] = String(dataUrl || "").split(",");
  const mime = (header.match(/^data:([^;]+)/) || [])[1] || "application/octet-stream";
  const binary = atob(body || "");
  const bytes = new Uint8Array(binary.length);
  for (let index = 0; index < binary.length; ++index) {
    bytes[index] = binary.charCodeAt(index);
  }
  return new Blob([bytes], { type: mime });
}

function canvasToPngBlob(canvasEl) {
  return new Promise((resolve, reject) => {
    if (canvasEl.toBlob) {
      try {
        canvasEl.toBlob((blob) => {
          if (blob) {
            resolve(blob);
            return;
          }
          try {
            resolve(dataUrlToBlob(canvasEl.toDataURL("image/png")));
          } catch (error) {
            reject(error);
          }
        }, "image/png");
        return;
      } catch (error) {
        try {
          resolve(dataUrlToBlob(canvasEl.toDataURL("image/png")));
        } catch (_) {
          reject(error);
        }
        return;
      }
    }
    try {
      resolve(dataUrlToBlob(canvasEl.toDataURL("image/png")));
    } catch (error) {
      reject(error);
    }
  });
}

async function ensureExportablePreviewImages() {
  let converted = 0;
  for (const item of state.items) {
    if (!itemVisibleInMode(item) || !item.dataUrl || /^data:/i.test(item.dataUrl)) continue;
    if (item.source === "project_sprite") {
      try {
        if (await hydrateProjectSpriteImage(item)) {
          converted += 1;
          continue;
        }
      } catch (_) {}
    }
    try {
      const dataUrl = await imageSourceToDataUrl(item.dataUrl);
      item.img = await loadImageSource(dataUrl);
      item.dataUrl = dataUrl;
      item.exportableImage = true;
      converted += 1;
    } catch (_) {
      item.exportableImage = false;
    }
  }
  return converted;
}

function resetPreviewCanvasBitmap() {
  previewCanvas.width = Math.max(1, state.width);
  previewCanvas.height = Math.max(1, state.height);
}

async function downloadCanvas(fileName) {
  await ensureExportablePreviewImages();
  resetPreviewCanvasBitmap();
  renderPreview();
  try {
    const blob = await canvasToPngBlob(previewCanvas);
    downloadBlob(fileName, blob);
    setStatus(`Preview PNG exported: ${fileName}.`);
  } catch (error) {
    console.error(error);
    const blockedItems = state.items
      .filter((item) => itemVisibleInMode(item) && item.exportableImage === false)
      .map((item) => item.fileName || item.name || item.id)
      .slice(0, 3);
    const blockedText = blockedItems.length ? ` Non-exportable image: ${blockedItems.join(", ")}.` : "";
    setStatus(`Preview PNG export failed. Reload the editor and retry; if it still fails, re-import path-based images.${blockedText}`);
  }
}

async function importLayout(file) {
  const text = await file.text();
  const layout = JSON.parse(text);
  if (layout.type === "stickmon-room-editor-session" || layout.snapshot) {
    const snapshot = await hydrateSessionSnapshot(layout.snapshot || layout);
    restoreSnapshot(snapshot);
    state.drawingFace = false;
    state.history = [];
    state.historyIndex = -1;
    refreshAfterStateRestore();
    commitHistory();
    setStatus(`Session file loaded: ${file.name}.`);
    return;
  }
  if (layout.canvas) {
    document.getElementById("canvasW").value = GAME_SCREEN_WIDTH;
    document.getElementById("canvasH").value = Math.max(GAME_SCREEN_HEIGHT, layout.canvas.height || 135);
  }
  document.getElementById("baseFit").value = BASE_FIT_WIDTH;
  if (layout.sourceScale) {
    document.getElementById("sourceScale").value = layout.sourceScale;
  }
  if (layout.baseOpacity != null) {
    document.getElementById("baseOpacity").value = Math.round(layout.baseOpacity * 100);
  }
  if (layout.roomGeometry) {
    const geometry = layout.roomGeometry;
    const canvasWidth = GAME_SCREEN_WIDTH;
    const canvasHeight = Math.max(
      GAME_SCREEN_HEIGHT,
      Number(geometry.canvas?.height || layout.canvas?.height || geometry.room?.height || state.height || GAME_SCREEN_HEIGHT)
    );
    document.getElementById("canvasW").value = canvasWidth;
    document.getElementById("canvasH").value = canvasHeight;
    const importedTrim = coerceTrimBox(geometry.prepared?.trim || geometry.reference?.trim || layout.base?.trim);
    if (importedTrim) {
      state.baseTrim = importedTrim;
    } else if (!hasBackground()) {
      state.baseTrim = null;
    }
    const roomWidth = Number(geometry.room?.width || canvasWidth);
    const roomHeight = Number(geometry.room?.height || canvasHeight);
    const preparedScale = Number(geometry.prepared?.scale) ||
      (importedTrim ? roomWidth / Math.max(1, importedTrim.width) : 0);
    const preparedY = Number(geometry.prepared?.y) || 0;
    state.width = canvasWidth;
    state.height = canvasHeight;
    syncCanvasSizeToPreparedRoom();
    const pointByCoord = new Map();
    state.points = [];
    state.nextPointId = 1;
    state.faces = (geometry.faces || []).map((face, index) => {
      const points = (face.sourcePoints || face.points || []).map((rawPoint) => {
        let targetX;
        let targetY;
        if (face.sourcePoints && importedTrim && preparedScale > 0) {
          targetX = ((Number(rawPoint[0]) || 0) - importedTrim.x) * preparedScale;
          targetY = ((Number(rawPoint[1]) || 0) - importedTrim.y) * preparedScale + preparedY;
        } else {
          targetX = Number(rawPoint[0]) || 0;
          targetY = Number(rawPoint[1]) || 0;
        }
        targetX = clampNumber(targetX, 0, state.width);
        targetY = clampNumber(targetY, 0, state.height);
        const edit = targetToEditPoint({ x: targetX, y: targetY });
        const x = Math.round(clampNumber(edit.x, 0, state.editWidth));
        const y = Math.round(clampNumber(edit.y, 0, state.editHeight));
        const key = `${Math.round(x)},${Math.round(y)}`;
        if (!pointByCoord.has(key)) {
          pointByCoord.set(key, createShapePoint(x, y));
        }
        return pointByCoord.get(key);
      });
      const type = normalizeFaceType(face.type);
      return {
        id: face.id || `face${index + 1}`,
        type,
        visible: face.editorVisible ?? face.visible !== false,
        shadowSurface: normalizeShadowSurface(face.shadowSurface, type),
        receivesShadow: type === FACE_TYPE_SPRITE_AREA || type === FACE_TYPE_DOORWAY
          ? false
          : face.receivesShadow !== false,
        points
      };
    });
    state.nextFaceId = state.faces.reduce((next, face) => {
      const match = String(face.id).match(/(\d+)$/);
      return Math.max(next, match ? Number(match[1]) + 1 : next);
    }, 1);
    state.selectedFaceId = state.faces[0]?.id || null;
    state.selectedPointIndex = state.faces[0]?.points.length ? 0 : null;
    state.selectedPointId = state.faces[0]?.points[0]?.id || null;
    state.draftPoints = [];
    state.drawingFace = false;
    pruneUnusedPoints();
  }
  if (layout.guides) {
    document.getElementById("spriteX").value = layout.guides.spriteX ?? 88;
    document.getElementById("spriteY").value = layout.guides.spriteY ?? 50;
    document.getElementById("spriteW").value = layout.guides.spriteW ?? 64;
    document.getElementById("spriteH").value = layout.guides.spriteH ?? 64;
    document.getElementById("showGuides").checked = layout.guides.show ?? true;
  }
  if (layout.night) {
    const nextNight = normalizeNightSettings(layout.night);
    state.night = nextNight;
    syncLightInputsFromState();
    document.getElementById("castShadows").checked = nextNight.castShadows;
    document.getElementById("shadowMode").value = nextNight.shadowMode;
  }

	  const missing = [];
	  const byFile = new Map(state.items.map((item) => [item.fileName, item]));
	  for (const entry of layout.furniture || []) {
	    let item = byFile.get(entry.fileName);
	    if (!item && entry.libraryId) {
	      const libraryEntry = furnitureLibraryEntryById(entry.libraryId);
	      if (libraryEntry) {
	        item = await createFurnitureItemFromLibraryEntry(libraryEntry, entry);
	        state.items.push(item);
	        byFile.set(item.fileName, item);
	      }
	    }
	    if (!item) {
	      missing.push(entry.libraryId || entry.fileName);
	      continue;
	    }
	    Object.assign(item, {
      id: entry.id || item.id,
      name: entry.name || item.name,
      x: entry.x ?? item.x,
      y: entry.y ?? item.y,
      z: entry.z ?? item.z,
      scale: entry.scale ?? item.scale,
      scaleX: entry.scaleX ?? (entry.targetWidth ? entry.targetWidth / Math.max(1, item.sourceWidth) : entry.scale ?? item.scaleX ?? item.scale),
      scaleY: entry.scaleY ?? (entry.targetHeight ? entry.targetHeight / Math.max(1, item.sourceHeight) : entry.scale ?? item.scaleY ?? item.scale),
      aspectLocked: entry.aspectLocked,
	      sourceScale: entry.sourceScale ?? item.sourceScale,
	      opacity: entry.opacity ?? item.opacity,
	      visible: entry.visible ?? item.visible,
	      visibleInDay: entry.visibleInDay ?? item.visibleInDay,
	      visibleInNight: entry.visibleInNight ?? item.visibleInNight,
	      anchor: entry.anchor || item.anchor,
      slot: entry.slot || item.slot,
	      layer: entry.layer || item.layer,
	      source: entry.source || item.source,
	      libraryId: entry.libraryId || item.libraryId,
	      libraryRevision: entry.libraryRevision ?? item.libraryRevision,
	      furnitureType: entry.furnitureType || item.furnitureType || inferFurnitureType(entry.fileName || item.fileName || entry.name || item.name),
      kind: entry.kind || item.kind || inferItemKind(entry.fileName || item.fileName || entry.name || item.name, entry.furnitureType || item.furnitureType),
      heightPx: entry.projection?.heightPx ?? entry.heightPx ?? item.heightPx,
      footprint: entry.footprint || item.footprint,
      footprintPolygon: normalizeLocalPolygon(entry.footprintPolygon ?? item.footprintPolygon),
      shadowPolygon: normalizeLocalPolygon(entry.projection?.casterContour ?? entry.shadowPolygon ?? item.shadowPolygon),
      shadowAnchor: entry.shadowAnchor ?? item.shadowAnchor,
      shadowFaceIds: normalizeShadowFaceIds(entry.shadowFaceIds ?? item.shadowFaceIds),
      wallShadowDepthPx: entry.projection?.depthPx ?? entry.wallShadowDepthPx ?? item.wallShadowDepthPx,
      wallShadowOffsetY: entry.wallShadowOffsetY ?? item.wallShadowOffsetY,
      projection: entry.projection ? { ...entry.projection } : item.projection,
      castsShadow: entry.castsShadow ?? item.castsShadow,
      shadowOpacity: entry.shadowOpacity ?? entry.shadowAlpha ?? item.shadowOpacity,
      shadowLength: entry.shadowLength ?? item.shadowLength,
      shadowBlur: entry.shadowBlur ?? item.shadowBlur,
      receivesShadow: entry.receivesShadow ?? item.receivesShadow,
      occludesSprite: entry.occludesSprite ?? item.occludesSprite,
      sortY: entry.sortY ?? item.sortY,
      cutoutApplied: entry.cutoutApplied ?? item.cutoutApplied,
      speciesId: entry.speciesId ?? item.speciesId,
      action: entry.action ?? item.action,
      frame: entry.frame ?? item.frame
    });
    if (item.aspectLocked === undefined) {
      item.aspectLocked = Math.abs(itemScaleX(item) - itemScaleY(item)) < 0.0001;
    }
    if (item.aspectLocked) {
      item.scaleY = itemScaleX(item);
    }
    item.scale = itemScaleX(item);
    if (item.source === "furniture") {
      item.furnitureType = normalizeFurnitureType(item.furnitureType);
    }
    normalizeItemSemantics(item);
  }
  setStatus(`Layout imported: ${file.name}.${missing.length ? ` Missing assets: ${missing.join(", ")}.` : ""}`);
  refreshList();
  refreshSelectedPanel();
  refreshFaceList();
  refreshSelectedFacePanel();
  render();
  commitHistory();
}

function escapeHtml(value) {
  return String(value).replace(/[&<>"']/g, (ch) => ({
    "&": "&amp;",
    "<": "&lt;",
    ">": "&gt;",
    '"': "&quot;",
    "'": "&#039;"
  }[ch]));
}

function escapeAttr(value) {
  return escapeHtml(value).replace(/`/g, "&#096;");
}

document.getElementById("baseInput").addEventListener("change", async (event) => {
  const files = [...event.target.files];
  event.target.value = "";
  if (files.length) await loadBackgroundFiles(files);
});

document.getElementById("furnitureInput").addEventListener("change", (event) => {
  addFurnitureFiles([...event.target.files]);
});

document.getElementById("layoutInput").addEventListener("change", (event) => {
  if (event.target.files[0]) importLayout(event.target.files[0]);
});

document.getElementById("canvasW").addEventListener("input", () => applyRoomSizeInput("width"));
document.getElementById("canvasH").addEventListener("input", () => applyRoomSizeInput("height"));
document.getElementById("aspectLock").addEventListener("change", () => {
  state.aspectLocked = document.getElementById("aspectLock").checked;
  state.aspectRatio = state.width / Math.max(1, state.height);
  render();
  commitHistory();
});

document.getElementById("copyLightToDay").addEventListener("click", () => {
  copyActiveLightToProfile("day");
  render();
  commitHistory();
});

document.getElementById("copyLightToNight").addEventListener("click", () => {
  copyActiveLightToProfile("night");
  render();
  commitHistory();
});

for (const id of ["baseOpacity", "sourceScale", "furnitureImportMode", "baseFit", "showGuides", "showGrid", "gridOpacity", "snapToGrid", "spriteX", "spriteY", "spriteW", "spriteH", "lightShape", "lightStrength", "lightX", "lightY", "lightDepth", "lightRadius", "lightAngle", "lightSpread", "castShadows", "shadowMode"]) {
  document.getElementById(id).addEventListener("input", () => {
    render();
    if (id === "baseOpacity" || id === "baseFit") refreshList();
    commitHistory();
  });
}

document.getElementById("dayMode").addEventListener("click", () => {
  commitLightInputsToState();
  state.mode = "day";
  updateModeButtons();
  syncLightInputsFromState();
  refreshList();
  render();
  commitHistory();
});

document.getElementById("nightMode").addEventListener("click", () => {
  commitLightInputsToState();
  state.mode = "night";
  updateModeButtons();
  syncLightInputsFromState();
  refreshList();
  render();
  commitHistory();
});

document.getElementById("furnitureMode").addEventListener("click", () => {
  state.editMode = "furniture";
  state.drawingFace = false;
  resetItemPolygonDraft();
  state.selectedFaceId = null;
  state.selectedPointIndex = null;
  state.selectedPointId = null;
  updateEditModeButtons();
  refreshFaceList();
  refreshSelectedFacePanel();
  render();
  commitHistory();
});

document.getElementById("shapeMode").addEventListener("click", () => {
  state.editMode = "shape";
  state.drawingFace = false;
  resetItemPolygonDraft();
  state.selectedId = null;
  updateEditModeButtons();
  refreshList();
  refreshSelectedPanel();
  render();
  commitHistory();
});

addRoomFaceButton?.addEventListener("click", (event) => {
  event.preventDefault();
  event.stopPropagation();
  toggleRoomFaceDrawing();
});

document.getElementById("selectTool").addEventListener("click", () => setToolMode("select"));
document.getElementById("measureHeightTool").addEventListener("click", () => setToolMode("measureHeight"));
document.getElementById("clearMeasure").addEventListener("click", clearMeasurement);

document.getElementById("projectionGuides").addEventListener("click", () => {
  state.showProjectionGuides = !state.showProjectionGuides;
  render();
  commitHistory();
});

document.getElementById("editZoom").addEventListener("input", (event) => {
  setEditZoom(event.target.value);
});

document.getElementById("zoomOut").addEventListener("click", () => {
  setEditZoom(Math.round(state.editZoom * 100) - 25);
});

document.getElementById("zoomIn").addEventListener("click", () => {
  setEditZoom(Math.round(state.editZoom * 100) + 25);
});

document.getElementById("zoomReset").addEventListener("click", () => {
  setEditZoom(100);
});

document.getElementById("resetView").addEventListener("click", () => {
  state.selectedId = null;
  state.selectedFaceId = null;
  state.selectedPointIndex = null;
  state.selectedPointId = null;
  state.lightSelected = false;
  state.editZoom = 1;
  updateZoomControls();
  updateEditModeButtons();
  refreshList();
  refreshSelectedPanel();
  refreshFaceList();
  refreshSelectedFacePanel();
  render();
  commitHistory();
});

document.getElementById("undoAction").addEventListener("click", undo);
document.getElementById("redoAction").addEventListener("click", redo);

document.getElementById("exportJson").addEventListener("click", () => {
  downloadText("room_layout.json", JSON.stringify(exportLayoutObject(), null, 2), "application/json");
  setStatus("Layout JSON exported: room_layout.json. It includes roomGeometry.");
});

document.getElementById("exportGeometry")?.addEventListener("click", () => {
  downloadText("room_geometry.json", JSON.stringify(exportRoomGeometryObject(), null, 2), "application/json");
});

document.getElementById("exportPreview").addEventListener("click", async () => {
  await downloadCanvas(`room_${state.mode}_preview.png`);
});

document.getElementById("exportFurnitureLibrary")?.addEventListener("click", () => {
  exportFurnitureLibraryBundle();
});

document.getElementById("exportSelectedFurnitureLibrary")?.addEventListener("click", async () => {
  const item = selectedItem();
  if (!item || item.source !== "furniture") {
    setStatus("Select annotated furniture before saving it to the library.");
    return;
  }
  try {
    await addFurnitureItemsToLibrary([item]);
  } catch (error) {
    console.error(error);
    setStatus(isFilePickerAbort(error) ? "Room editor folder was not selected." : (error.message || "Adding selected furniture failed."));
  }
});

document.getElementById("changeFurnitureLibraryRoot")?.addEventListener("click", async () => {
  if (!window.showDirectoryPicker) {
    setStatus("Direct library writing is not supported in this browser. Use Export furniture bundle instead.");
    return;
  }
  try {
    await ensureRoomEditorRootHandle(true);
    setStatus("Room editor folder selected. Add selected and Save session will write under this tool folder.");
  } catch (error) {
    console.error(error);
    setStatus(isFilePickerAbort(error) ? "Room editor folder was not selected." : (error.message || "Room editor folder selection failed."));
  }
});

document.getElementById("saveSession").addEventListener("click", async () => {
  try {
    await saveEditorSession();
  } catch (error) {
    console.error(error);
    setStatus(error.message || "Session save failed.");
  }
});

document.getElementById("changeSessionFile").addEventListener("click", async () => {
  try {
    await saveEditorSession({ forcePick: true });
  } catch (error) {
    console.error(error);
    setStatus(error.message || "Session save location change failed.");
  }
});

document.getElementById("loadSession").addEventListener("click", async () => {
  try {
    await loadEditorSession();
  } catch (error) {
    console.error(error);
    setStatus(error.message || "Session load failed.");
  }
});

document.getElementById("clearSession").addEventListener("click", async () => {
  try {
    await clearEditorSession();
  } catch (error) {
    console.error(error);
    setStatus(error.message || "Clearing saved session failed.");
  }
});

previewCanvas.addEventListener("click", () => {
  state.previewZoomed = !state.previewZoomed;
  updatePreviewZoom();
});

document.getElementById("deleteItem").addEventListener("click", () => {
  const item = selectedItem();
  if (!item) return;
  deleteItemById(item.id);
});

document.getElementById("duplicateItem").addEventListener("click", () => {
  const item = selectedItem();
  if (!item) return;
  const copy = { ...item, id: `f${state.nextId++}`, name: `${item.name}_copy`, x: item.x + 4, y: item.y + 4 };
  copy.sortY = autoSortY(copy);
  state.items.push(copy);
  state.selectedId = copy.id;
  refreshList();
  refreshSelectedPanel();
  render();
  commitHistory();
});

canvas.addEventListener("pointerdown", (event) => {
  if (state.toolMode === "measureHeight") {
    const canvasPoint = pointerToCanvas(event);
    state.measure.dragging = true;
    setMeasureStart(canvasPoint);
    canvas.classList.add("dragging");
    canvas.setPointerCapture(event.pointerId);
    updateMeasurePanel();
    render();
    return;
  }

  const canvasPoint = pointerToCanvas(event);
  if (state.itemPolygonDraft.active) {
    handleItemPolygonDraftPointerDown(event);
    return;
  }
  if (hitTestConeControl(canvasPoint.x, canvasPoint.y)) {
    state.draggingConeControl = true;
    state.lightSelected = true;
    updateEditModeButtons();
    canvas.classList.add("dragging");
    canvas.setPointerCapture(event.pointerId);
    updateConeControlFromCanvasPoint(canvasPoint);
    render();
    return;
  }
  if (hitTestLight(canvasPoint.x, canvasPoint.y)) {
    state.draggingLight = true;
    state.lightSelected = true;
    updateEditModeButtons();
    canvas.classList.add("dragging");
    canvas.setPointerCapture(event.pointerId);
    setLightPositionFromCanvasPoint(canvasPoint);
    render();
    return;
  }
  if (state.editMode === "shape") {
    state.lightSelected = false;
    updateEditModeButtons();
    handleShapePointerDown(event);
    commitHistory();
    return;
  }

  const polygonHit = hitTestItemPolygonPoint(canvasPoint.x, canvasPoint.y);
  if (polygonHit) {
    state.lightSelected = false;
    state.selectedId = polygonHit.item.id;
    state.selectedFaceId = null;
    state.selectedPointIndex = null;
    state.selectedPointId = null;
    state.draggingItemPolygon = true;
    state.dragPolygonKey = polygonHit.key;
    state.dragPolygonIndex = polygonHit.index;
    setActiveItemPolygon(polygonHit.key);
    materializeItemPolygon(polygonHit.item, polygonHit.key);
    syncPolygonEditorField(polygonHit.item, polygonHit.key);
    updateDraggedItemPolygon(canvasPoint);
    canvas.classList.add("dragging");
    canvas.setPointerCapture(event.pointerId);
    updateEditModeButtons();
    render();
    return;
  }

  const p = pointerToTarget(event);
  const item = hitTest(p.x, p.y);
  if (item) {
    state.lightSelected = false;
    state.selectedId = item.id;
    state.selectedFaceId = null;
    state.selectedPointIndex = null;
    state.selectedPointId = null;
    state.dragging = true;
    state.dragOffsetX = p.x - item.x;
    state.dragOffsetY = p.y - item.y;
    canvas.classList.add("dragging");
    canvas.setPointerCapture(event.pointerId);
  } else {
    state.selectedId = null;
    state.lightSelected = false;
  }
  updateEditModeButtons();
  refreshList();
  refreshSelectedPanel();
  render();
});

canvas.addEventListener("pointermove", (event) => {
  if (state.measure.dragging) {
    setMeasureEnd(pointerToCanvas(event));
    updateMeasurePanel();
    render();
    return;
  }

  if (state.draggingConeControl) {
    updateConeControlFromCanvasPoint(pointerToCanvas(event));
    render();
    return;
  }

  if (state.draggingLight) {
    setLightPositionFromCanvasPoint(pointerToCanvas(event));
    render();
    return;
  }
  if (state.itemPolygonDraft.dragging) {
    moveItemPolygonDraftPoint(pointerToCanvas(event));
    return;
  }
  if (state.draggingItemPolygon) {
    updateDraggedItemPolygon(pointerToCanvas(event));
    render();
    return;
  }
  if (state.editMode === "shape" && state.draggingPoint) {
    const shapePoint = state.points.find((entry) => entry.id === state.dragPointId);
    if (!shapePoint) return;
    const point = pointerToCanvas(event);
    shapePoint.x = Math.round(point.x);
    shapePoint.y = Math.round(point.y);
    const membership = findPointMembership(shapePoint);
    setSelectedPoint(shapePoint, membership);
    render();
    return;
  }
  if (!state.dragging) return;
  const item = selectedItem();
  if (!item) return;
  const p = pointerToTarget(event);
  item.x = snapValue(Math.round(p.x - state.dragOffsetX));
  item.y = snapValue(Math.round(p.y - state.dragOffsetY));
  item.sortY = autoSortY(item);
  refreshList();
  refreshSelectedPanel();
  render();
});

function finishCanvasDrag(event) {
  if (state.measure.dragging) {
    setMeasureEnd(pointerToCanvas(event));
    state.measure.dragging = false;
    canvas.classList.remove("dragging");
    try {
      canvas.releasePointerCapture(event.pointerId);
    } catch (_) {}
    updateMeasurePanel();
    render();
    setStatus(measurementStatusText());
    return;
  }

  if (state.itemPolygonDraft.dragging) {
    finishItemPolygonDraftPointDrag(event);
    return;
  }

  if (state.dragging || state.draggingPoint || state.draggingItemPolygon || state.draggingLight || state.draggingConeControl) {
    commitHistory();
  }
  state.dragging = false;
  state.draggingItemPolygon = false;
  state.draggingLight = false;
  state.draggingConeControl = false;
  state.draggingPoint = false;
  state.dragPointId = null;
  state.dragPolygonKey = null;
  state.dragPolygonIndex = null;
  canvas.classList.remove("dragging");
  refreshSelectedFacePanel();
  try {
    canvas.releasePointerCapture(event.pointerId);
  } catch (_) {}
}

canvas.addEventListener("pointerup", finishCanvasDrag);
canvas.addEventListener("pointercancel", finishCanvasDrag);

window.addEventListener("keydown", (event) => {
  if (event.key === "Escape" && state.previewZoomed) {
    state.previewZoomed = false;
    updatePreviewZoom();
    event.preventDefault();
    return;
  }
  if (event.key === "Escape" && state.itemPolygonDraft.active) {
    cancelItemPolygonDraft();
    event.preventDefault();
    return;
  }
  if (event.key === "Escape" && state.editMode === "shape" && state.drawingFace) {
    setRoomFaceDrawing(false);
    commitHistory();
    event.preventDefault();
    return;
  }

  if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === "z") {
    event.preventDefault();
    if (event.shiftKey) {
      redo();
    } else {
      undo();
    }
    return;
  }

  if (state.toolMode === "measureHeight") {
    if (event.key === "Escape") {
      setToolMode("select");
      event.preventDefault();
    }
    return;
  }

  if (state.editMode === "shape") {
    const shapePoint = selectedPoint();
    if (shapePoint) {
      let dx = 0;
      let dy = 0;
      if (event.key === "ArrowLeft") dx = -1;
      if (event.key === "ArrowRight") dx = 1;
      if (event.key === "ArrowUp") dy = -1;
      if (event.key === "ArrowDown") dy = 1;
      if (!dx && !dy) return;
      if (event.shiftKey) {
        dx *= 8;
        dy *= 8;
      }
      shapePoint.x += dx;
      shapePoint.y += dy;
      event.preventDefault();
      refreshSelectedFacePanel();
      render();
      commitHistory();
      return;
    }
  }

  const item = selectedItem();
  if (!item) return;

  if (state.editMode === "furniture" && item.source === "project_sprite") {
    return;
  }

  let dx = 0;
  let dy = 0;
  if (event.key === "ArrowLeft") dx = -1;
  if (event.key === "ArrowRight") dx = 1;
  if (event.key === "ArrowUp") dy = -1;
  if (event.key === "ArrowDown") dy = 1;
  if (!dx && !dy) return;
  if (event.shiftKey) {
    dx *= 8;
    dy *= 8;
  }
  item.x = snapValue(item.x + dx);
  item.y = snapValue(item.y + dy);
  event.preventDefault();
  refreshList();
  refreshSelectedPanel();
  render();
  commitHistory();
});

function setupImageDropZone(id, onDrop) {
  const dropZone = document.getElementById(id);
  if (!dropZone) return;
  dropZone.addEventListener("dragover", (event) => {
    event.preventDefault();
    dropZone.classList.add("dragover");
  });
  dropZone.addEventListener("dragleave", () => dropZone.classList.remove("dragover"));
  dropZone.addEventListener("drop", async (event) => {
    event.preventDefault();
    dropZone.classList.remove("dragover");
    const images = [...event.dataTransfer.files].filter((file) => file.type.startsWith("image/"));
    if (!images.length) {
      setStatus("Drop image files here. Use Layout JSON to restore a layout.");
      return;
    }
    await onDrop(images);
  });
}

setupImageDropZone("backgroundDropZone", (images) => loadBackgroundFiles(images));
setupImageDropZone("furnitureDropZone", (images) => addFurnitureFiles(images));

function setupLeftPanelState() {
  const panelIds = [
    "assetsPanel",
    "backgroundPanel",
    "furnitureImportPanel",
    "projectSpritesPanel",
    "furnitureLibraryPanel",
    "viewHelpersPanel"
  ];
  const panels = panelIds
    .map((id) => document.getElementById(id))
    .filter(Boolean);
  let saved = null;
  try {
    saved = JSON.parse(localStorage.getItem(LEFT_PANEL_STATE_KEY) || "null");
  } catch {
    saved = null;
  }
  const defaults = hasBackground()
    ? { furnitureLibraryPanel: true }
    : { assetsPanel: true, backgroundPanel: true };
  const openState = saved && typeof saved === "object" ? saved : defaults;
  let ready = false;
  const save = () => {
    if (!ready) return;
    const next = Object.fromEntries(panels.map((panel) => [panel.id, panel.open]));
    try {
      localStorage.setItem(LEFT_PANEL_STATE_KEY, JSON.stringify(next));
    } catch {
      // Panel preferences are optional; editing still works without storage.
    }
  };
  for (const panel of panels) {
    panel.open = openState[panel.id] === true;
    panel.addEventListener("toggle", save);
  }
  requestAnimationFrame(() => { ready = true; });
}

function refreshInitialEmptyEditor() {
  commitHistory();
  refreshList();
  refreshSelectedPanel();
  refreshFaceList();
  refreshSelectedFacePanel();
  updateBaseImageMeta();
  updateModeButtons();
  updateEditModeButtons();
  updateToolButtons();
  updatePreviewZoom();
  updateZoomControls();
  updateGridOpacityLabel();
  updateUndoButtons();
  render();
}

async function initializeEditor() {
  setupHoverTooltips();
  initProjectSpriteControls();
  initFurnitureLibraryControls();
  const loaded = await tryAutoLoadEditorSession();
  if (!loaded) refreshInitialEmptyEditor();
  setupLeftPanelState();
}

initializeEditor();
