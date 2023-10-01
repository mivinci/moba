local moba = require "moba"
local math = require "math"


-- 阈值
local delta_rank = 2
local delta_score = 10
local delta_role = 4


-- 各路的权值
local weight_top = 0.3
local weight_bot = 0.2
local weight_mid = 0.2
local weight_jg  = 0.2
local weight_sup = 0.1


-- 队伍 g 的加权平均 score 值
function score(g)
  return moba.score(g, weight_top, weight_bot, weight_mid, weight_jg, weight_sup)
end


-- 队伍 g1 相较于 g2 的 ELO 值
function elo(g1, g2)
  return 1 / (1 + math.pow(10, (score(g2)-score(g1)) / 400))
end


-- 示例匹配规则
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
  return math.abs(score(g1) - score(g2)) < delta_score
end


-- 统计 x 的低 5 位为 1 的个数
function bitcount5(x)
  local c = 0
  for i = 0, 5, 1 do
    if (x & 1) == 1 then
      c = c + 1
    end
    x = x >> 1
  end
  return c
end