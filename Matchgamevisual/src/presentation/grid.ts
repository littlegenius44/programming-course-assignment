import { BOARD_COLS, BOARD_ROWS } from '../core/types';
import type { Position } from '../core/types';

export const CELL_SIZE = 32;

export interface SelectionResult {
  readonly selected: Position | null;
  readonly pair: { readonly from: Position; readonly to: Position } | null;
}

/** 将Phaser逻辑坐标转换为格子，边界外不产生选择。 */
export function positionAt(x: number, y: number): Position | null {
  if (!Number.isFinite(x) || !Number.isFinite(y) || x < 0 || y < 0 || x >= BOARD_COLS * CELL_SIZE || y >= BOARD_ROWS * CELL_SIZE) return null;
  return { row: Math.floor(y / CELL_SIZE), col: Math.floor(x / CELL_SIZE) };
}

/** 处理选中、取消和相邻位置标记，不执行游戏规则。 */
export function selectPosition(selected: Position | null, position: Position): SelectionResult {
  if (selected?.row === position.row && selected.col === position.col) return { selected: null, pair: null };
  if (selected && Math.abs(selected.row - position.row) + Math.abs(selected.col - position.col) === 1) {
    return { selected: null, pair: { from: selected, to: position } };
  }
  return { selected: position, pair: null };
}
