# 星空装饰素材

新增装饰仅保留两件：

| 名称 | 游戏用透明 PNG | 物理像素 | 逻辑位置 |
| --- | --- | --- | --- |
| 右墙挂画 | `assets/picture-right-wall.png` | 56×48 | (193,43) |
| 星星抱枕 | `assets/star-floor-cushion.png` | 64×56 | (183,82) |

左墙置物架沿用 `../starry-set-v1/assets/shelf-starry.png`（112×68），
架子和月亮、星球仪为一体资源，逻辑位置 (67,21)。抱枕替换原地面星球仪。

`assets/` 是游戏尺寸资源；`masters/` 为生成母图，不用于固件打包。
`manifest.json` 记录新增资源及复用置物架；`prompts.json` 仅保留这两件的生成记录。

`room-preview.png` 是使用原一体置物架的完整房间效果图；
`assets-preview.png` 是两件装饰的 3 倍最近邻放大对照。
`preview-layout.json` 使用相对路径，依赖同级 `starry-set-v1` 和仓库原房间资源；
使用项目合成器复现时，将 furniture_dir 和 layout_dir 均设为本目录，
先按 2 倍放大布局，再调用 compose_room_image。

已清理旧版小玩偶、地面星球仪、左墙挂画、拆分置物架及独立摆件和切换预览。
素材包是本目录的归档；复现完整预览需保留上述仓库依赖。
此次仅整理源素材和预览，未接入固件。
