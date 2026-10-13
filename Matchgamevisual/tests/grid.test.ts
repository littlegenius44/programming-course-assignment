import { describe, expect, it } from 'vitest';
import { positionAt, selectPosition } from '../src/presentation/grid';

describe('棋盘逻辑坐标与选择', () => {
  it('第一格、格子边界与最后一格映射正确', () => {
    expect(positionAt(0, 0)).toEqual({ row: 0, col: 0 });
    expect(positionAt(32, 64)).toEqual({ row: 2, col: 1 });
    expect(positionAt(959.9, 639.9)).toEqual({ row: 19, col: 29 });
  });

  it.each([[-1, 0], [0, -1], [960, 0], [0, 640], [NaN, 0], [0, Infinity]])('坐标(%s,%s)不选择棋盘', (x, y) => {
    expect(positionAt(x, y)).toBeNull();
  });

  it('首次点击选中，同格再次点击取消', () => {
    const cell = { row: 3, col: 4 };
    expect(selectPosition(null, cell)).toEqual({ selected: cell, pair: null });
    expect(selectPosition(cell, { ...cell })).toEqual({ selected: null, pair: null });
  });

  it('非相邻和斜向位置切换选择', () => {
    const selected = { row: 3, col: 4 };
    for (const target of [{ row: 8, col: 9 }, { row: 4, col: 5 }]) {
      expect(selectPosition(selected, target)).toEqual({ selected: target, pair: null });
    }
  });

  it.each([{ row: 2, col: 4 }, { row: 4, col: 4 }, { row: 3, col: 3 }, { row: 3, col: 5 }])('相邻位置%j标记一对，不提前修改棋盘', target => {
    const selected = { row: 3, col: 4 };
    expect(selectPosition(selected, target)).toEqual({ selected: null, pair: { from: selected, to: target } });
  });
});
