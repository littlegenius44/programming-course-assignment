import { describe, expect, it } from 'vitest';
import { resolveMove } from '../src/core/game';
import { findMatches } from '../src/core/match';
import type { GameState, GameStep, ItemKind, MatchSegment, Position } from '../src/core/types';
import { MAX_BURSTS_PER_CLEAR, planClearEffects } from '../src/presentation/effectPlan';

type ClearStep = Extract<GameStep, { kind: 'CLEAR' }>;

function fixture(): GameState {
  const values = Array.from({ length: 20 }, (_, row) => Array.from({ length: 30 }, (_, col) => (row + col) % 9 + 1));
  values[0][1] = 1;
  values[0][2] = 2;
  values[1][2] = 1;
  let id = 1;
  return { board: values.map(row => row.map(kind => ({ id: id++, kind: kind as ItemKind }))),
    nickname: '效果契约', matchLength: 3, score: 0, level: 1, status: 'playing', nextTileId: id };
}

function clearAt(cells: Position[], round = 2): ClearStep {
  const after = fixture();
  const scorePerTile = round === 1 ? 10 : round === 2 ? 20 : 30;
  return { kind: 'CLEAR', index: 2, after, round, count: cells.length, scorePerTile,
    scoreDelta: cells.length * scorePerTile, removed: cells.map((position, index) => ({ position, tile: { id: index + 1, kind: 1 } })) };
}

const rowCells = [{ row: 1, col: 0 }, { row: 1, col: 1 }, { row: 1, col: 2 }];
const segment: MatchSegment = { direction: 'horizontal', kind: 1, cells: rowCells };

describe('消除效果只读取真实位置与轮次', () => {
  it('第1轮使用轻圆环，后续轮次使用爆炸图', () => {
    expect(planClearEffects(clearAt(rowCells, 1), [segment]).style).toBe('ring');
    expect(planClearEffects(clearAt(rowCells, 2), [segment]).style).toBe('burst');
  });

  it('第3轮起强度封顶，不随长连锁无限放大', () => {
    const second = planClearEffects(clearAt(rowCells, 2), [segment]);
    const third = planClearEffects(clearAt(rowCells, 3), [segment]);
    const fourth = planClearEffects(clearAt(rowCells, 4), [segment]);
    expect(third.diameter).toBeGreaterThan(second.diameter);
    expect(fourth.diameter).toBe(third.diameter);
  });

  it('横纵交叉段共用一个真实中心，不产生重复爆炸', () => {
    const vertical = [{ row: 0, col: 1 }, { row: 1, col: 1 }, { row: 2, col: 1 }];
    const clear = clearAt([...rowCells, vertical[0], vertical[2]]);
    const plan = planClearEffects(clear, [segment, { direction: 'vertical', kind: 1, cells: vertical }]);
    expect(plan.centers).toEqual([{ row: 1, col: 1 }]);
    expect(clear.count).toBe(5);
    expect(clear.scoreDelta).toBe(100);
  });

  it('600格匹配只限制装饰数量，不限制真实消除记录', () => {
    const board = fixture().board.map(row => row.map(tile => ({ ...tile!, kind: 1 as const })));
    const matched = findMatches(board, 3);
    const clear = clearAt([...matched.cells]);
    const plan = planClearEffects(clear, matched.segments);
    expect(plan.centers).toHaveLength(MAX_BURSTS_PER_CLEAR);
    expect(new Set(plan.centers.map(p => `${p.row},${p.col}`)).size).toBe(MAX_BURSTS_PER_CLEAR);
    expect(plan.centers.every(p => clear.removed.some(x => x.position.row === p.row && x.position.col === p.col))).toBe(true);
    expect(clear.removed).toHaveLength(600);
  });

  it('禁用装饰不改写步骤、得分或物品记录', () => {
    const clear = clearAt(rowCells);
    const before = JSON.stringify(clear);
    expect(planClearEffects(clear, [segment], false).centers).toEqual([]);
    expect(JSON.stringify(clear)).toBe(before);
  });

  it('未被消除的位置不能成为爆炸中心', () => {
    const outside: MatchSegment = { ...segment, cells: [{ row: 10, col: 10 }] };
    expect(planClearEffects(clearAt(rowCells), [outside]).centers).toEqual([]);
    expect(planClearEffects(clearAt(rowCells), []).centers).toEqual([]);
  });

  it('真实四轮结算仍得30/60/90/90分，开启关闭装饰终态相同', () => {
    const values = [1, 1, 1, 1, 1, 1, 1, 1, 1, 7, 8, 9];
    let index = 0;
    const result = resolveMove(fixture(), { row: 0, col: 2 }, { row: 1, col: 2 }, () => (values[index++] - 0.5) / 9);
    const before = JSON.stringify(result);
    let segments: ReadonlyArray<MatchSegment> = [];
    const clearScores: number[] = [];
    for (const step of result.steps) {
      if (step.kind === 'MATCH') segments = step.matches.segments;
      if (step.kind !== 'CLEAR') continue;
      clearScores.push(step.scoreDelta);
      const plan = planClearEffects(step, segments);
      expect(plan.centers.every(p => step.removed.some(x => x.position.row === p.row && x.position.col === p.col))).toBe(true);
      expect(planClearEffects(step, segments, false).centers).toEqual([]);
    }
    expect(clearScores).toEqual([30, 60, 90, 90]);
    expect(result.finalState.score).toBe(270);
    expect(JSON.stringify(result)).toBe(before);
  });
});
