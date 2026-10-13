import { describe, expect, it } from 'vitest';
import * as game from '../src/core/game';
import { findMatches } from '../src/core/match';
import { createBoard } from '../src/core/board';
import type { GameState, ItemKind, MatchLength } from '../src/core/types';

export function stableFixture(k: MatchLength = 3): GameState {
  return { nickname: '测试', matchLength: k, score: 0, level: 1, status: 'playing', nextTileId: 601,
    board: Array.from({ length: 20 }, (_, row) => Array.from({ length: 30 }, (_, col) =>
      ({ id: row * 30 + col + 1, kind: ((row + col) % 9 + 1) as ItemKind }))) };
}

describe('只读有效操作查找', () => {
  it.each([3, 4] as const)('k=%i 只读查找与完整棋盘模拟的首个操作一致', k => {
    let seed = 21;
    const random = () => { seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0; return seed / 4294967296; };
    for (let sample = 0; sample < 15; sample++) {
      const state = { ...stableFixture(k), ...createBoard(k, random) };
      const work = state.board.map(row => [...row]);
      let expected = null;
      search: for (let row = 0; row < 20; row++) for (let col = 0; col < 30; col++) {
        for (const to of [{ row, col: col + 1 }, { row: row + 1, col }]) {
          if (to.row >= 20 || to.col >= 30) continue;
          [work[row][col], work[to.row][to.col]] = [work[to.row][to.col], work[row][col]];
          const matched = findMatches(work, k).cells.length > 0;
          [work[row][col], work[to.row][to.col]] = [work[to.row][to.col], work[row][col]];
          if (matched) { expected = { from: { row, col }, to }; break search; }
        }
      }
      expect(game.findLegalMove(state)).toEqual(expected);
    }
  });
  it.each([3, 4] as const)('k=%i 死局返回null', k => {
    expect(game.findLegalMove(stableFixture(k))).toBeNull();
  });
  it.each([3, 4] as const)('k=%i 可找到右下边界交换，查询不修改输入', k => {
    const state = stableFixture(k);
    const board = state.board.map(row => row.map(tile => ({ ...tile! })));
    for (let col = 30 - k; col < 30; col++) board[19][col].kind = col === 29 ? 9 : 8;
    board[18][29].kind = 8;
    const input = { ...state, board };
    expect(findMatches(board, k).cells).toHaveLength(0);
    const before = JSON.stringify(input);
    const identity = board[0][0];
    const move = game.findLegalMove(input)!;
    expect(move).not.toBeNull();
    expect(game.findLegalMove(input)).toEqual(move);
    expect(JSON.stringify(input)).toBe(before);
    expect(board[0][0]).toBe(identity);
    expect(Object.isFrozen(move)).toBe(true);
    expect(Object.isFrozen(move.from)).toBe(true);
    expect(Object.isFrozen(move.to)).toBe(true);
    expect(Math.abs(move.from.row - move.to.row) + Math.abs(move.from.col - move.to.col)).toBe(1);
    [board[move.from.row][move.from.col], board[move.to.row][move.to.col]] =
      [board[move.to.row][move.to.col], board[move.from.row][move.from.col]];
    expect(findMatches(board, k).cells.length).toBeGreaterThan(0);
  });
  it('胜利没有提示，损坏或尚未稳定的输入被拒绝', () => {
    expect(game.findLegalMove({ ...stableFixture(), status: 'won' })).toBeNull();
    expect(() => game.findLegalMove({ ...stableFixture(), nextTileId: 2 })).toThrow();
    expect(() => game.findLegalMove({ ...stableFixture(), board: [] })).toThrow();
  });
});
