import type { GameStep, MatchSegment, Position } from '../core/types';
import { CELL_SIZE } from './grid';

export const MAX_BURSTS_PER_CLEAR = 12;
export interface ClearEffectPlan {
  readonly centers: ReadonlyArray<Position>;
  readonly style: 'ring' | 'burst';
  readonly diameter: number;
}

/** 装饰只读取消除记录，不能扩大消除范围或修改得分。 */
export function planClearEffects(
  clear: Extract<GameStep, { kind: 'CLEAR' }>,
  segments: ReadonlyArray<MatchSegment>,
  enabled = true,
): ClearEffectPlan {
  const style = clear.round === 1 ? 'ring' : 'burst';
  const diameter = CELL_SIZE * (clear.round === 1 ? 0.9 : clear.round === 2 ? 1.5 : 1.8);
  const centers: Position[] = [];
  if (!enabled) return { centers, style, diameter };
  const key = (position: Position): string => `${position.row},${position.col}`;
  const removed = new Set(clear.removed.map(item => key(item.position)));
  const used = new Set<string>();
  for (const segment of segments) {
    const cells = segment.cells.filter(position => removed.has(key(position)));
    const center = cells[Math.floor(cells.length / 2)];
    if (!center || used.has(key(center))) continue;
    used.add(key(center));
    centers.push({ ...center });
    if (centers.length === MAX_BURSTS_PER_CLEAR) break;
  }
  return { centers, style, diameter };
}
