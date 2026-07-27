1.背包物品分类 ✅ 2026-07-27 已完成：商店「日常」新增美味粮+甜/辣/酸/苦/涩五种口味粮，「探险」新增解麻/解眠/灼伤/解冻四药（解毒药收窄为仅解毒）；背包（MenuScene）与战斗背包同步支持新物品使用；SAVE_VERSION 1→2（旧档重置）；设计文档 §6.4 已同步
2.性格→口味偏好 + 战斗投掷口味接入 ✅ 2026-07-27 已完成：25 性格按修正项映射喜/厌口味（物攻辣/物防涩/特攻苦/特防酸/速度甜）；房间喂食心情喜欢×150%讨厌×75%（爱心/停顿可感知反馈+状态页标注）；战斗背包可投全部食物，按野生性格分四档结算（接受率50/55/60/40%，Boss 30/35/40/20%，羁绊+30/35/40/20）；全部数值集中在 src/game/FoodTuning.h；设计文档 §6.4.6 已同步
3.

参考：
赤之救助队神兽移动：https://www.bilibili.com/video/BV1Ux4y1e7yM/?spm_id_from=333.337.search-card.all.click&vd_source=1cd465352ac4284ef51b32cc833ad6f5
时间线：固拉多：00:44:01

./origin_asset/source_sheets/low/Game Boy Advance - Pokemon Mystery Dungeon_ Red Rescue Team - Pokemon (1st Generation) - Pokemon (#001-025).png，处理错误，我告诉你每只精灵占的行数
从上到下依次为：
3，2，2，
3，2，3，
3，2，2，
2，2，2，
2，2，2，
2，2，2，
2，2，
2，2，
3，3，
3

Game Boy Advance - Pokemon Mystery Dungeon_ Red Rescue Team - Pokemon (1st Generation) - Pokemon (#026-050)重新处理，每只精灵占的行数从上到下依次为：
3，
2，2，
2，2，2，
2，2，2，
2，2，
2，3，
2，2，
2，2，2，
2，3，
1，2，
2，


Game Boy Advance - Pokemon Mystery Dungeon_ Red Rescue Team - Pokemon (1st Generation) - Pokemon (#076-100)重新处理，每只精灵占的行数从上到下依次为：
3，
2，2，
2，2，
2，2，
2，
2，3，
2，2，
2，3，
2，2，
2，2，2，
2，
2，3，
2，3，
2，


Game Boy Advance - Pokemon Mystery Dungeon_ Red Rescue Team - Pokemon (1st Generation) - Pokemon (#101-125)重新处理，每只精灵占的行数从上到下依次为：
2，
3，3，
4，3，
3，2，
2，
2，4，
2，2，
2，
2，
3，
2，2，
2，3，
2，2，
3，2，
3，3，


009_blastoise最后一行被切掉了
040_wigglytuff

./origin_asset/source_sheets/low/中，123_scyther，094_gengar，093_haunter，092_gastly做进一步处理：
1. 四只精灵的walk有3帧，前15个图是walk0~walk4, 3帧一组，共5个朝向，从左到右依次为：正面，左下，左，左上，背面
2. 123_scyther第二行倒数第7，6是两帧sleeping
3. 094_gengar第二行倒数第10，9是两帧sleeping
4. 093_haunter第二行正数第10，9是两帧sleeping
5. 092_gastly第二行正数第7，8是两帧sleeping

./origin_asset/source_sheets/low/中，026_raichu拆分写入项目，图片描述如下：
1. walk有3帧，3帧一组，共8个朝向，24个图由第一行21个+第二行3个。从左到右依次为：正面，右下，右，右上，背面，左上，左，左下（在第二行）。8个朝向不可用镜像
2. 第二行倒数第3，2为两帧sleep

./origin_asset/source_sheets/low/中，007_squirtle，009_blastoise拆分写入项目，图片描述如下：
1. walk有3帧，前15个图是walk0~walk4, 3帧一组，共5个朝向，从左到右依次为：正面，左下，左，左上，背面，其他方向用镜像
2. 007_squirtle第二行正数第15，16为两帧sleep
3. 009_blastoise第二行正数第4，5为两帧sleep

也处理008_wartortle，图片描述如下：
1. walk有3帧，3帧一组，共8个朝向，24个图由第一行21个+第二行3个。从左到右依次为：正面，右下，右，右上，背面，左上，左，左下。8个朝向不可用镜像
2. 第二行倒数第6，7是两帧sleeping


处理下：./origin_asset/source_sheets/low/split_by_species/016_pidgey.png,017_pidgeotto,018_pidgeot.png,
1. walk有3帧，3帧一组，共5个朝向，从左到右，正面，左下，左，左上，背面
2. 016的第一行最后俩帧是sleep
3. 017的第二行的6，7是sleep
4. 018的第二行的6，7是sleep


鬼斯通，迷你龙，哈克龙的运动逻辑不对


1.鬼斯通的移动不够自然，walk中第一帧是移动的开始帧，第二帧是运动的持续帧，第三帧是运动结束帧。不应该移动过程中循环播放
2.迷你龙改成walking0,1同时也是idle帧
3.哈克龙将idle_0，idle_1加到walking动作里，改成walking0~2。改完后迷你龙对齐了，walking0~2一共三帧，同时walking0,1也是idle帧
4.迷你龙,哈克龙的运动改成1->2->3->2->1，视觉上是依次加速到减速的过程
5.抽象一些状态机，运行注入不同的运动模式


梳理下状态页的信息：

状态第一页：
1.左侧要放精灵的icon，右侧是信息按行排列如下：
名字，
等级 性别，
属性，
特性（如猛火），
性格
2.在最下方写来源，参考GBA（如在草丛小路遇到，「草丛小路」要高亮）

第二页：
能力值：
hp: 19/19   特攻：xx
物攻：xx    特防：xx
物防：xx.   速度：xx
经验：
经验值： xxxxx  升级需要：xxx
[--------               ](经验值进度条)

第三页
技能：
[一般（带颜色）]抓击/冲撞/等 
[技能属性（带颜色）]技能名  熟练度：，低，中，搞，满
威力：xx  (特殊技能的)
水波震动对手 (特殊技能的)

其他页保持不变

处理./origin_asset/source_sheets/medium/261_poochyena_262_mightyena.png描述如下：
1.poochyena和mightyena第二行为move，3帧，5个方向，从左到右是正面，左下，左，左上，背面。其他方向用镜像
2.poochyena和mightyena第四行，倒数2，3为sleep

处理./origin_asset/source_sheets/medium/278_wingull_279_pelipper.png描述如下：
1.wingull和pelipper第一行为move，3帧，5个方向，从左到右是正面，左下，左，左上，背面。其他方向用镜像
2.wingull第二行倒数2，3为sleep
3.pelipper第四行倒数2，3为sleep

处理./origin_asset/source_sheets/medium/276_taillow_277_swellow.png
1.taillow和swellow第一行为move，3帧，5个方向，从左到右是正面，左下，左，左上，背面。其他方向用镜像
2.taillow和swellow第三行倒数2，3，为sleep

./origin_asset/source_sheets/low/012_butterfree.png裁剪错误，第二行被裁掉了


处理./origin_asset/source_sheets/low/010_caterpie.png,011_metapod,012_butterfree。先生成预览图，等我确认后再写人
1. 第一行前15帧为move，3帧，5个方向，从左到右是正面，左下，左，左上，背面。其他方向用镜像
2. 010_caterpie第一行倒数3，4为sleep
3. 011_metapod第一行倒数2，3为sleep
4. 012_butterfree第二行2，3为sleep

处理./origin_asset/source_sheets/low/074_geodude.png，075_graveler，076_golem先生成预览图，等我确认后再写人
1. 第一行前15帧为move，3帧，5个方向，从左到右是正面，左下，左，左上，背面。其他方向用镜像
2. 074_geodude,075_graveler的第二行倒数6，7为sleep
3. 076_golem的第二行倒数8，9为sleep

处理./origin_asset/source_sheets/medium/183_marill_184_azumarill.png,先生成预览图，等我确认后再写入
1. marill和azumarill第一行为move，3帧，5个方向，从左到右是正面，左下，左，左上，背面。其他方向用镜像
2. marill第三行倒数2，3，为sleep
3. azumarill第四行倒数2，3，为sleep
4. marill,azumarill第二行为攻击动画，2帧一组，5个方向同move
5. azumarill 第三行为跳跃动画，1帧，8个方向从左到右是正面，左下，左，左上，背面，右上，右，右下
6. 攻击和和跳跃动画先解析出来，留着后续备用

处理./origin_asset/source_sheets/medium/194_wooper_195_quagsire.png,先生成预览图，等我确认后再写入
1. wooper第一行为move，3帧，5个方向，从左到右是正面，左下，左，左上，背面。其他方向用镜像
2. wooper第二行为攻击动画，2帧一组，5个方向同move
3. quagsire第二行为move,2帧，5个方向,从左到右是正面，左下，左，左上，背面。其他方向用镜像
4. quagsire第一行为攻击动画，2帧一组，5个方向同move
5. 两者的第三行倒数2，3为sleep

./origin_asset/source_sheets/medium/285_shroomish_286_breloom.png，先生成预览图，等我确认后再写入
1. shroomish和breloom第一行为move，3帧，5个方向，从左到右是正面，左下，左，左上，背面。其他方向用镜像
2. shroomish第二行倒数2，3为sleep
3. breloom第三行倒数2，3为sleep

./origin_asset/source_sheets/medium/298_azurill.png处理下，先生成预览图，等我确认后再写入
1. 第一行为move，3帧，5个方向，从左到右是正面，左下，左，左上，背面。其他方向用镜像
2. 第三行倒数2，3为sleep
3. 第二行为jump，3帧，5个方向，从左到右是正面，左下，左，左上，背面。其他方向用镜像

./origin_asset/source_sheets/medium/362_glalie.png处理下，先生成预览图，等我确认后再写入
1. 左边3列为move，每一行的3帧为一组，5个方向从上到下分别为：左，左上 正面，左下，背面，其他用镜像
2. 第一行最后2帧为sleep

./origin_asset/source_sheets/high/361_snorunt.png处理下
1. 左侧4帧为move，中间「sleeping」字样上面2帧是sleep

./origin_asset/source_sheets/high/322_numel_323_camerupt.png 处理下
1. 对应的帧在描述文字的左边


./origin_asset/source_sheets/high/280_ralts.png，281_kirlia处理一下，先生成预览图，等我确认后再写入
1. 280_ralts的idle，walking方向从上往下依次为正面，右下，右，右上，背面，其他用镜像
2. 281_kirlia walk 2帧，第1行代表第1帧的朝向，方向从左到右为正面，左下，左，左上，背面，其他用镜像
2. 281_kirlia attacking也解析出来，3帧，按行排列。方向从上到下为：正面，右下，右，右上，背面，其他用镜像

./origin_asset/source_sheets/medium/282_gardevoir.png处理一下，先生成预览图，等我确认后再写入
1. 第一行为walking, 3帧一组，从左到右为：正面，左下，左，左上，背面。运动方式类似鬼斯通
2. 第二第三行都为3帧一组，方向与第一帧一样。暂时认定为两种idle动作帧
3. 第四行倒数2,3是sleep，
4. 倒数第1个是濒死，先解析出来

./origin_asset/source_sheets/low/041_zubat.png，042_golbat处理一下，先生成预览图，等我确认后再写入
1. walking, 3帧一组，从左到右为：正面，左下，左，左上，背面
2. 倒数第6，7为sleep
./origin_asset/source_sheets/medium/169_crobat.png处理下
1. 第一行为walking, 3帧一组，从左到右为：正面，左下，左，左上，背面
2. 第三行第1，2为sleep

修改商店的ui: ✅ 已实现（两列分类 + 图标列左移进入子菜单 + 未选中侧半透明压暗；2026-07-27 起子菜单商品已扩为日常 8 项 / 探险 6 项）
1.商店ui分成两列，左边是大类名字，如：日常，探险，出售，右侧展示对应的icon。
2.用户选择一项后，左边的一列消失，右边的列做左移动画移动到左边列的位置，意思是进入子菜单了，此时会选中第一个item，原右侧区域展示物品名称，价格，描述
2.光标在左侧时，右侧的icon列表处于一个半透明状态。
3.下面为大概示意：
*日常   icon(普通粮)
探险    icon(美味粮)
出售
返回

552，553，554，560，561，562，568，569，570共同组成一个9格小岛，561是中间区域，其他事8格方向的边缘
571是描述悬崖走向从右侧拐到朝下的拐角，572是描述从左侧拐到朝下的拐角，
515是朝向左侧的山洞入口，571时朝向右侧的山洞入口，524+532是屏幕朝向的山洞入口，526是背向屏幕的山洞入口。以上入口也可以作为出口
539是山洞内的岩石台阶
540，548是爬到上一层的梯子
541是爬到下一层的梯子
579，580，588，596，597，598，590，582，583是半格/四分之一格图，描述边缘走势为，从左侧开始，在580拐点向下在596拐点向右，在598拐点向上，在582拐点向右

601，602，603，609，610，611，617，618，619使用，需要放置在610的位置