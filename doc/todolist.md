1.背包物品分类
2.

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
