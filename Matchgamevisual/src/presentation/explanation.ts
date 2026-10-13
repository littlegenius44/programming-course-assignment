import type { GameStep } from '../core/types';

export interface StepExplanation {
  readonly phase: 0 | 1 | 2 | 3 | 4 | 5 | null;
  readonly title: string;
  readonly description: string;
}

/** 说明只读当前步骤的真实记录。 */
export function explainStep(step: GameStep): StepExplanation {
  switch (step.kind) {
    case 'SHUFFLE':
      return { phase: null, title: '自动洗牌', description: '当前无可用操作。保留分数、关卡和用时，重新生成无初始匹配且至少存在一次有效交换的棋盘。' };
    case 'SWAP': return { phase: 0, title: '交换物品', description: '交换两个相邻位置，随后检测是否形成横向或纵向匹配。' };
    case 'SWAP_BACK': return { phase: 0, title: '交换回退', description: '这次交换没有形成匹配，将两个物品恢复到原来的位置。' };
    case 'MATCH': return { phase: step.round === 1 ? 1 : 5,
      title: step.round === 1 ? '匹配检测' : '再次检测', description: step.matches.cells.length
        ? `发现${step.matches.segments.length}条匹配段，交叉去重后共${step.matches.cells.length}格。高亮位置将统一消除。`
        : '没有发现新的匹配；若本次交换无效则回退，否则本回合已经稳定。' };
    case 'CLEAR': return { phase: 2, title: step.round === 1 ? '统一消除' : `连锁第${step.round}轮`,
      description: `本轮消除${step.count}格，每格${step.scorePerTile}分，增加${step.scoreDelta}分。空格是消除后的真实状态。` };
    case 'GRAVITY': return { phase: 3, title: '重力下落',
      description: `${step.moves.length}个物品沿原列下落，保持同列物品的相对顺序，填补下方空格。` };
    case 'REFILL': return { phase: 4, title: '随机补充',
      description: `补充${step.spawned.length}个新物品，随后再次检测；新物品可能触发下一轮连锁。` };
    case 'STABLE': return { phase: null, title: '棋盘稳定', description: '本回合所有连锁已结束，棋盘没有空格或待消除匹配。' };
    case 'LEVEL_CHANGE': return { phase: null, title: `进入第${step.toLevel}关`, description: '全部连锁结束后达到累计目标，保持成绩和用时，生成新关卡棋盘。' };
    case 'WIN': return { phase: null, title: '达成胜利', description: '全部连锁结束，累计得分达到1200分，完成三关。' };
  }
}
