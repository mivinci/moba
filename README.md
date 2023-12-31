# MOBA 游戏组队/匹配模拟

大致思路就是将匹配的过程抽象为某个数据结构和一系列函数，部分函数里需要热更新的部分交给 Lua 运行时调用可在运行期替换的 Lua 代码。代码规范上也偏爱和遵循 Lua 的风格。另外，抽象出的数据结构里的*装载因子*的设计比较巧妙，算是原创（应该是吧？


## 设计综述

先总结并完善下需求：

- 按照 段位、排位分、擅长的路线 三个因素来尽可能公平地匹配队友或对手
- 匹配逻辑和相关配置交给嵌入的 Lua 脚本，实现运行期热更新
- 预组队存在队员人数不够的话，要跟池子里其他队伍合并，满足 5 人开黑条件
- 匹配度和匹配等待时间取舍的动态调整
- 匹配对手后允许队内换位，换位可能使得队伍的加权排位分更新，由此来模拟选/禁英雄的博弈阶段


### 结构体

**玩家**

```c
struct player {
  int id;  // starts from 1
  int rank;
  int score;
  int role;  // role bitmap
};
```


`player::id` 表示玩家 id 且唯一。

`player::rank` 表示玩家段位，整型，取值范围为 [0, 5]，分别对应段位名称：

- BRONZE     
- SILVER     
- GOLD       
- PLATNUM    
- DIAMOND    
- CHALLENGER

`player::score` 表示玩家的排位分，整型。

`player::role` 表示玩家擅长的路线，位图。

使用位图的原因有两点：

1. 用一个整型数字就能表示出玩家擅长的所有路线
2. 玩家进行组队时，可以通过对该字段进行简单的或运算，得出该队伍的路线分配情况，可作为匹配因素

所以，从低位到高位，分别对应路线：

- TOP 上路
- BOT 下路
- MID 中路
- JG  打野
- SUP 辅助

例如，若玩家 p1 的 role 为 0，则表示玩家 p1 要打上路，跟 role 为 6 的玩家 p2 组队，则该队伍的 role 就为 (p1.role | p2.role) = 7，即该队伍覆盖了上下中路，还差打野和辅助，在匹配队友的时候就可以再通过或运算找到 role 为 24 的队伍进行合并。

**队伍**

```c
struct group {
  int len;
  int cap;
  struct player *players;
  struct list_head node;
};
```

`group::len` 表示预组队人数，整型。

`group::cap` 表示该队伍要打的是几人开黑局，整型。

`group::players` 记录队员和站位，该字段预分配的长度为 `group::cap`。

`group::node` 链表节点，参考 Linux 内核循坏双链表 `list.h` 使用

**匹配池**

```c
struct moba {
  lua_State *L;
  struct list_head *queues;  // matchmaking queues
  int len;
  int n;
  int k;
};
```

`moba::L` Lua 运行沙盒。

`moba::queues` 不同人数队伍的匹配池，链表数组。


`moba::len` 已合并的预组队队伍的个数，整型。

`moba::n` 每个队伍需要的人数，整型。若为 5 则表示为 5 排匹配池。

`moba::k` 匹配池的*装载因子*，整型。

装载因子用来控制匹配队友等待时间，装载因子越大，等待的时间就越久，但找到更合适的队友的概率就越大。



### 函数

这里介绍几个核心函数和实现原理。

```c
struct moba *moba_open(int n, int k);
```

创建并返回一个匹配池。例如，`moba_open(5, 32)` 表示创建一个 5 黑匹配池，装载因子为 32。

```c
void moba_load(struct moba *M, const char *name);
```

加载一个 Lua 脚本，参数 `name` 为要加载的 Lua 脚本的路径。所以系统检测到 Lua 脚本更新时，可以重新调用函数实现运行期热更新匹配逻辑的实现。

```c
void moba_push(struct moba *M, struct group *g);
```

将一个预组队队伍加入对应的匹配池。`moba::queues` 用链表数组维护了不同人数的匹配池，如下图所示

```
+---+   
| 1 |--> g13 --> g12 --> g11 --> g10
+---+   
| 2 |--> g20
+---+
| 3 |--> g31 --> g30
+---+
| 4 |--> g42 --> g41 --> g40
+---+
| 5 |--> g50 --> g51
+---+
```

假设我们现在要将一个人数为 4 的预组队队伍 g43 加入匹配池，那么有两种情况：

1. 若 5 人匹配池已满（达到装载因子大小），则直接将 g43 插入 4 人匹配池。
2. 若 5 人匹配池未满（未达到装载因子大小），则遍历 1 人匹配池，找到与 g43 最匹配的 1 人队伍，将他们合并后插入 5 号匹配池。

这样便巧妙地实现了通过装载因子 k 来控制匹配队友等待时间。

```c
int moba_match(struct moba *M, struct match *mat);
```

在匹配池里找到人数已达到开黑要求的两只队伍，将他们放到一个 `struct match` 结构体中，若匹配到，则返回 0，否则返回 -1。`struct match` 的定义如下：

```c
struct match {
  struct group *red;
  struct group *blue;
};
```
每调用一次 `moba_match` 就会进行一次对手匹配，所以系统可以通过调正该函数的调用频率来控制匹配的速度。该函数调用的频率越小，匹配池里玩家的等待时间就越长，但找到更合适的队友和对手的概率就越大。

综上，通过对**装载因子**和调用 `moba_match` 的**频率**的控制来实现对**匹配度**和**匹配等待时间**的动态调整。

## 示例

### 依赖

需要先下载 Lua 库

- Archlinux

```bash
pacman -S lua
```

- Debian

```bash
apt install lua
```

- RHEL

```bash
dnf install lua
```

- macOS

```bash
brew install lua
```

### 编译

直接 make 就行，Makefile 里写好了完整的编译流程。

```bash
make
```

### 运行

准备一些预组队队伍信息，写入 [input.txt](./input.txt)。

**输入描述**

1. 第一行两个整数 n, m，分别表示队数和每队需要的开黑人数
2. 接下来的 n 部分，每部分第一行一个整数 k，表示预组队人数
3. 每部分后 k 行里每行 3 个整数，分别表示玩家的段位、排位分、擅长路线

然后运行示例：

```bash
make test
```

**输出描述**

如下图所示，前 5 行表示双方每位队员的站位和每个队员的排位分，第 6 行为双方的队伍加权平均排位分，第 7 行表示双方的 ELO 值。ELO 值的计算方式也可以在 [test.lua](./test.lua) 中更改。 

```
        BLUE    RED
TOP     1(10)   7(12)
BOT     2(20)   8(20)
MID     3(10)   9(30)
JG      4(30)   6(20)
SUP     5(20)   10(13)
SCORE   17.0    18.9
ELO     0.50    0.50
1 pair(s) of groups matched
```




### 匹配逻辑

匹配逻辑实现在 [test.lua](./test.lua) 里的 `match` 函数中，匹配队友和对手都是通过该函数实现，例如按段位、排位分、擅长路线综合匹配：

```lua
function match(g1, g2)
  -- 若段位差超过 delta_rank 则匹配失败
  local rank_min1, rank_max1 = moba.rank(g1)
  local rank_min2, rank_max2 = moba.rank(g2)
  if ((rank_max1 - rank_min2) > delta_rank) or ((rank_max2 - rank_min1) > delta_rank) then
    return false
  end

  -- 若路线覆盖没有达到 delta_role 则匹配失败
  local role1 = moba.role(g1)
  local role2 = moba.role(g2)
  if (bitcount5(role1 | role2) < delta_role) then
    return false
  end

  -- 若 score 之差超过 delta_score 则匹配失败
  local s1 = score(g1)
  local s2 = score(g2)
  return math.abs(s1 - s2) < delta_score
end
```

## 参考

- https://en.wikipedia.org/wiki/Elo_rating_system
- https://ai.plainenglish.io/exploring-skill-based-matchmaking-systems-in-online-games-96491816d9e7
- https://segmentfault.com/q/1010000009504187
- https://www.youtube.com/watch?v=-pglxege-gU
- https://github.com/cloudwu/skynet
- https://www.lua.org/manual/5.1
- *Programming in Lua - 4th edition*
