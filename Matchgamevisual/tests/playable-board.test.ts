import { describe, expect, it } from 'vitest';
import * as board from '../src/core/board';
import * as game from '../src/core/game';
import { findMatches } from '../src/core/match';
import type { GameState } from '../src/core/types';

function periodic(): { random: () => number; calls: () => number } {
  let i = 0;
  return { random: () => ((Math.floor(i / 30) % 20 + i++ % 30) % 9 + 0.5) / 9, calls: () => i };
}

describe('有限生成可玩棋盘', () => {
  it.each([3, 4] as const)('k=%i 周期死局源也有限结束且保留连续编号', k => {
    const source = periodic();
    const generated = board.createPlayableBoard(k, source.random, 1000);
    expect(source.calls()).toBe(3 * 600 + 2 + 600 - 2 * k);
    expect(findMatches(generated.board, k).cells).toHaveLength(0);
    const state: GameState = { ...generated, nickname: '测试', score: 80, level: 1, status: 'playing', matchLength: k };
    expect(game.findLegalMove(state)).not.toBeNull();
    expect(generated.nextTileId).toBe(1600);
    expect(new Set(generated.board.flat().map(tile => tile!.id)).size).toBe(600);
    const kinds = generated.board.slice(0, 2).map(row => row.slice(0, k).map(tile => tile!.kind));
    expect(kinds[0].slice(0, k - 1).every(value => value === kinds[0][0])).toBe(true);
    expect(kinds[0][k - 1]).toBe(kinds[1][0]);
    expect(kinds[1][k - 1]).toBe(kinds[0][0]);
    expect(kinds[0][0]).not.toBe(kinds[1][0]);
  });
  it.each([3, 4] as const)('k=%i 固定随机源也不会重试无止境', k => {
    for (const value of [0, 0.5, 0.999999]) {
      const initialized = board.createPlayableBoard(k, () => value);
      expect(findMatches(initialized.board, k).cells).toHaveLength(0);
      expect(game.hasLegalMove({ ...initialized, nickname: '测试', score: 0, level: 1, status: 'playing', matchLength: k })).toBe(true);
    }
  });
  it('普通createBoard仍只消费600次，异常源明确报错', () => {
    const source = periodic();
    board.createBoard(3, source.random);
    expect(source.calls()).toBe(600);
    for (const value of [-1, 1, NaN, Infinity]) expect(() => board.createPlayableBoard(3, () => value)).toThrow();
  });
  it.each([3, 4] as const)('k=%i 洗牌保留元数据，更新编号并冻结终态', k => {
    const initialized = board.createBoard(k, periodic().random, 800);
    const before: GameState = { ...initialized, nickname: '保留玩家', score: 760, level: 3, status: 'playing', matchLength: k };
    const serialized = JSON.stringify(before);
    const after = game.reshuffleGame(before, periodic().random);
    expect([after.nickname, after.score, after.level, after.matchLength, after.status])
      .toEqual([before.nickname, before.score, before.level, k, 'playing']);
    expect(after.nextTileId).toBe(before.nextTileId + 600);
    expect(after.board.flat().every(tile => tile!.id >= before.nextTileId)).toBe(true);
    expect(Object.isFrozen(after)).toBe(true);
    expect(Object.isFrozen(after.board[0][0])).toBe(true);
    expect(JSON.stringify(before)).toBe(serialized);
    expect(game.hasLegalMove(after)).toBe(true);
    let calls = 0;
    expect(() => game.reshuffleGame({ ...before, status: 'won' }, () => { calls++; return 0; })).toThrow();
    expect(calls).toBe(0);
  });
});
