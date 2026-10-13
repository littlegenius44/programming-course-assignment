import { describe, expect, it } from 'vitest';
import { resolveMove } from '../src/core/game';
import type { GameState, ItemKind } from '../src/core/types';
import { timingFor } from '../src/presentation/playback';
import { explainStep } from '../src/presentation/explanation';

function fourRounds() {
  const values = Array.from({ length: 20 }, (_, r) => Array.from({ length: 30 }, (_, c) => (r + c) % 9 + 1));
  values[0][1] = 1; values[0][2] = 2; values[1][2] = 1;
  let id = 1;
  const state: GameState = { board: values.map(row => row.map(kind => ({ id: id++, kind: kind as ItemKind }))),
    nickname: '演示测试', matchLength: 3, score: 0, level: 1, status: 'playing', nextTileId: 601 };
  const sequence = [1, 1, 1, 1, 1, 1, 1, 1, 1, 7, 8, 9];
  let index = 0;
  return resolveMove(state, { row: 0, col: 2 }, { row: 1, col: 2 }, () => (sequence[index++] - 0.5) / 9);
}

describe('演示时长与真实说明', () => {
  it('普通模式保持原时长，不采用保存的慢速偏好', () => {
    expect(timingFor({ demo: false, speed: 0.25 })).toEqual({ scale: 1, matchMs: 160, noMatchMs: 70, clearHoldMs: 0, shuffleNoticeMs: 500 });
  });
  it.each([1, 0.5, 0.25] as const)('演示速度%s统一调整动画与观察时间', speed => {
    const timing = timingFor({ demo: true, speed });
    expect(timing.scale).toBe(1 / speed);
    expect(timing.matchMs).toBe(650);
    expect(timing.noMatchMs).toBe(300);
    expect(timing.clearHoldMs).toBe(450);
    expect(timing.shuffleNoticeMs).toBe(1000);
  });
  it('拒绝无效速度', () => {
    expect(() => timingFor({ demo: true, speed: 0 as 1 })).toThrow();
    expect(() => timingFor({ demo: true, speed: Number.NaN as 1 })).toThrow();
  });
  it('洗牌说明独立于消除，不产生新的连锁阶段', () => {
    const view = explainStep({ kind: 'SHUFFLE', reason: 'no-legal-move', index: 0, after: fourRounds().finalState });
    expect(view.title).toBe('自动洗牌');
    expect(view.phase).toBeNull();
    expect(view.description).toContain('保留');
  });
  it('说明读取真实步骤，四轮得分与冻结快照不改变', () => {
    const result = fourRounds();
    const before = JSON.stringify(result);
    for (const speed of [1, 0.5, 0.25] as const) {
      timingFor({ demo: true, speed });
      for (const step of result.steps) {
        const view = explainStep(step);
        expect(view.title.length).toBeGreaterThan(0);
        if (step.kind === 'CLEAR') {
          expect(view.description).toContain(String(step.count));
          expect(view.description).toContain(String(step.scoreDelta));
        }
        if (step.kind === 'MATCH' && !step.matches.cells.length) expect(view.description).toContain('没有');
      }
    }
    expect(result.steps.filter(s => s.kind === 'CLEAR').map(s => s.scoreDelta)).toEqual([30, 60, 90, 90]);
    expect(result.finalState.score).toBe(270);
    expect(JSON.stringify(result)).toBe(before);
  });
  it('交叉匹配说明使用去重的5格，不把两条三格段算作6格', () => {
    const after = fourRounds().finalState;
    const cells = [{ row: 0, col: 1 }, { row: 1, col: 0 }, { row: 1, col: 1 }, { row: 1, col: 2 }, { row: 2, col: 1 }];
    const view = explainStep({ kind: 'MATCH', round: 1, index: 1, after,
      matches: { cells, segments: [
        { direction: 'horizontal', kind: 1, cells: cells.slice(1, 4) },
        { direction: 'vertical', kind: 1, cells: [cells[0], cells[2], cells[4]] },
      ] } });
    expect(view.description).toContain('2条');
    expect(view.description).toContain('5格');
    expect(view.phase).toBe(1);
  });
});
