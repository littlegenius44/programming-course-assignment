import { BOARD_COLS, BOARD_ROWS, ITEM_TYPES } from './types';
import type { Board, ItemKind, LocatedTile, MatchLength, RandomSource, Tile, TileMove } from './types';
import { findAvailableSwap } from './moves';
import { findMatches } from './match';

export const MAX_PLAYABLE_ATTEMPTS = 3;

/** 只复制行；物品由规则层视为只读，移动时不修改物品本身。 */
export function copyBoard(board: Board): Array<Array<Tile | null>> {
  return board.map(row => [...row]);
}

/** 快照复制每行及每个物品，并冻结，避免后续运算或调用方改写历史。 */
export function freezeBoard(board: Board): Board {
  return Object.freeze(board.map(row => Object.freeze(row.map(tile => tile ? Object.freeze({ ...tile }) : null))));
}

/** 随机源约定返回 [0,1)，每次选值恰好消费一个随机数。 */
function randomIndex(random: RandomSource, count: number): number {
  const value = random();
  if (!Number.isFinite(value) || value < 0 || value >= 1) throw new RangeError('随机源必须返回 [0,1) 内的有限数值。');
  return Math.floor(value * count);
}

/** 枚举合法候选，避免固定或异常随机源导致初始化重试不结束。 */
export function createBoard(matchLength: MatchLength, random: RandomSource, nextTileId = 1): { board: Board; nextTileId: number } {
  return buildBoard(matchLength, random, nextTileId);
}

function buildBoard(matchLength: MatchLength, random: RandomSource, nextTileId: number,
  pattern?: { a: ItemKind; b: ItemKind }): { board: Board; nextTileId: number } {
  if (matchLength !== 3 && matchLength !== 4) throw new RangeError('消除长度只能是 3 或 4。');
  if (!Number.isSafeInteger(nextTileId) || nextTileId < 1) throw new RangeError('物品编号必须是正整数。');
  const board: Array<Array<Tile | null>> = Array.from({ length: BOARD_ROWS }, () => Array<Tile | null>(BOARD_COLS).fill(null));
  for (let row = 0; row < BOARD_ROWS; row++) {
    for (let col = 0; col < BOARD_COLS; col++) {
      const candidates: ItemKind[] = [];
      for (let value = 1; value <= ITEM_TYPES; value++) {
        let horizontal = 1;
        let vertical = 1;
        for (let offset = 1; offset < matchLength && board[row][col - offset]?.kind === value; offset++) horizontal++;
        for (let offset = 1; offset < matchLength && board[row - offset]?.[col]?.kind === value; offset++) vertical++;
        if (horizontal < matchLength && vertical < matchLength) candidates.push(value as ItemKind);
      }
      const forced = pattern && row < 2 && col < matchLength
        ? row === 0
          ? col < matchLength - 1 ? pattern.a : pattern.b
          : col < matchLength - 1 ? pattern.b : pattern.a
        : null;
      const kind = forced ?? candidates[randomIndex(random, candidates.length)];
      if (!candidates.includes(kind)) throw new Error('可玩图案与初始化约束冲突。');
      board[row][col] = { id: nextTileId++, kind };
    }
  }
  return { board: freezeBoard(board), nextTileId };
}

/** 有限重试后在初始化时植入一次交换可消除的图案，不改写已生成棋盘。 */
export function createPlayableBoard(k: MatchLength, random: RandomSource, nextTileId = 1): { board: Board; nextTileId: number } {
  for (let attempt = 0; attempt < MAX_PLAYABLE_ATTEMPTS; attempt++) {
    const generated = createBoard(k, random, nextTileId);
    if (findAvailableSwap(generated.board, k)) return generated;
  }
  const a = (randomIndex(random, ITEM_TYPES) + 1) as ItemKind;
  const others = Array.from({ length: ITEM_TYPES }, (_, index) => (index + 1) as ItemKind).filter(kind => kind !== a);
  const b = others[randomIndex(random, others.length)];
  const generated = buildBoard(k, random, nextTileId, { a, b });
  if (findMatches(generated.board, k).cells.length || !findAvailableSwap(generated.board, k)) {
    throw new Error('未能生成稳定且可玩的棋盘。');
  }
  return generated;
}

/** 从底向上读写压缩，每列保持原有次序，只记录实际移动的物品。 */
export function applyGravity(source: Board): { board: Board; moves: ReadonlyArray<TileMove> } {
  const board = copyBoard(source);
  const moves: TileMove[] = [];
  for (let col = 0; col < (board[0]?.length ?? 0); col++) {
    let writeRow = board.length - 1;
    for (let row = board.length - 1; row >= 0; row--) {
      const tile = source[row][col];
      if (!tile) continue;
      board[writeRow][col] = tile;
      if (row !== writeRow) moves.push({ id: tile.id, from: { row, col }, to: { row: writeRow, col } });
      writeRow--;
    }
    while (writeRow >= 0) board[writeRow--][col] = null;
  }
  return { board: freezeBoard(board), moves };
}

/** 补充按行优先遍历，不避开匹配，使新物品可触发下一轮连锁。 */
export function refillBoard(source: Board, nextTileId: number, random: RandomSource): { board: Board; nextTileId: number; spawned: ReadonlyArray<LocatedTile> } {
  const board = copyBoard(source);
  const spawned: LocatedTile[] = [];
  for (let row = 0; row < board.length; row++) {
    for (let col = 0; col < board[row].length; col++) {
      if (board[row][col]) continue;
      const tile: Tile = Object.freeze({ id: nextTileId++, kind: (randomIndex(random, ITEM_TYPES) + 1) as ItemKind });
      board[row][col] = tile;
      spawned.push({ tile, position: { row, col } });
    }
  }
  return { board: freezeBoard(board), nextTileId, spawned };
}
