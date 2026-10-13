import { findMatches } from './match';
import { BOARD_COLS, BOARD_ROWS } from './types';
import type { Board, LegalMove, MatchLength } from './types';

/** 调用方提供稳定棋盘；只在副本上模拟相邻交换，返回前恢复。 */
export function findAvailableSwap(board: Board, k: MatchLength): LegalMove | null {
  const work = board.map(row => [...row]);
  for (let row = 0; row < BOARD_ROWS; row++) {
    for (let col = 0; col < BOARD_COLS; col++) {
      for (const to of [{ row, col: col + 1 }, { row: row + 1, col }]) {
        if (to.row >= BOARD_ROWS || to.col >= BOARD_COLS) continue;
        const first = work[row][col]!;
        const second = work[to.row][to.col]!;
        if (first.kind === second.kind) continue;
        work[row][col] = second;
        work[to.row][to.col] = first;
        // 稳定输入的新匹配只可能出现在交换格所在行列。列转为行后复用同一扫描器；
        // 每次最多抽取两条线，少于k，因此抽取线之间不会产生伪纵向匹配。
        const rows = to.row === row ? [work[row]] : [work[row], work[to.row]];
        const columns = to.col === col ? [work.map(line => line[col])]
          : [work.map(line => line[col]), work.map(line => line[to.col])];
        const matched = findMatches(rows, k).cells.length > 0 || findMatches(columns, k).cells.length > 0;
        work[row][col] = first;
        work[to.row][to.col] = second;
        if (matched) return Object.freeze({ from: Object.freeze({ row, col }), to: Object.freeze({ ...to }) });
      }
    }
  }
  return null;
}
