import type { Board, MatchLength, MatchResult, MatchSegment, Position } from './types';

/** 扫描横纵连续段，并按位置去重。 */
export function findMatches(board: Board, matchLength: MatchLength): MatchResult {
  if (matchLength !== 3 && matchLength !== 4) throw new RangeError('消除长度只能是 3 或 4。');
  const rows = board.length;
  const cols = board[0]?.length ?? 0;
  if (board.some(row => row.length !== cols)) throw new RangeError('棋盘必须是矩形。');

  // 两个方向共用标记矩阵，收集完成后才输出去重位置。
  const marked = board.map(row => row.map(() => false));
  const segments: MatchSegment[] = [];

  /** 扫描一条完整的行或列；遇到空格也要结束当前连续段。 */
  const scanLine = (line: Position[], direction: MatchSegment['direction']): void => {
    let start = 0;
    while (start < line.length) {
      const origin = line[start];
      const tile = board[origin.row][origin.col];
      let end = start + 1;
      while (tile && end < line.length && board[line[end].row][line[end].col]?.kind === tile.kind) end++;
      if (tile && end - start >= matchLength) {
        const cells = Object.freeze(line.slice(start, end));
        segments.push(Object.freeze({ direction, kind: tile.kind, cells }));
        for (const cell of cells) marked[cell.row][cell.col] = true;
      }
      start = end;
    }
  };

  for (let row = 0; row < rows; row++) {
    scanLine(Array.from({ length: cols }, (_, col) => Object.freeze({ row, col })), 'horizontal');
  }
  for (let col = 0; col < cols; col++) {
    scanLine(Array.from({ length: rows }, (_, row) => Object.freeze({ row, col })), 'vertical');
  }

  const cells: Position[] = [];
  for (let row = 0; row < rows; row++) {
    for (let col = 0; col < cols; col++) {
      if (marked[row][col]) cells.push(Object.freeze({ row, col }));
    }
  }
  return Object.freeze({ segments: Object.freeze(segments), cells: Object.freeze(cells) });
}
