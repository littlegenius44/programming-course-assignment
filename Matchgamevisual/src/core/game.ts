import { applyGravity, copyBoard, createPlayableBoard, freezeBoard, refillBoard } from './board';
import { findMatches } from './match';
import { findAvailableSwap } from './moves';
import { BOARD_COLS, BOARD_ROWS, ITEM_TYPES, MAX_CHAIN_ROUNDS } from './types';
import type { GameState, GameStep, LegalMove, MoveResolution, Position, RandomSource, MatchLength, StepDetails, TileMove } from './types';

/** 冻结内部生成的步骤详情；不冻结调用方传入的位置或工作棋盘。 */
function freezeData<T>(value: T): T {
  if (value !== null && typeof value === 'object' && !Object.isFrozen(value)) {
    for (const child of Object.values(value)) freezeData(child);
    Object.freeze(value);
  }
  return value;
}

/** 每一步都复制棋盘，历史快照不会引用之后修改的数组或物品。 */
function snapshot(state: GameState): GameState {
  return Object.freeze({ ...state, board: freezeBoard(state.board) });
}

/** 创建20×30的稳定棋盘；随机源可注入以复现初始化和后续补充。 */
export function createGame(nickname: string, matchLength: MatchLength, random: RandomSource = Math.random): GameState {
  nickname = nickname.trim();
  if (!nickname) throw new RangeError('昵称不能为空。');
  const initialized = createPlayableBoard(matchLength, random);
  return snapshot({ ...initialized, nickname, matchLength, score: 0, level: 1, status: 'playing' });
}

/** 检查稳定棋盘是否仍有有效相邻交换，不生成随机数或改变棋盘。 */
export function hasLegalMove(state: GameState): boolean {
  return findLegalMove(state) !== null;
}

export function findLegalMove(state: GameState): LegalMove | null {
  if (state.status === 'won') return null;
  validateStableState(state);
  return findAvailableSwap(state.board, state.matchLength);
}

/** 自动洗牌仅重新生成稳定棋盘，保留已提交成绩和游戏设置。 */
export function reshuffleGame(state: GameState, random: RandomSource = Math.random): GameState {
  if (state.status === 'won') throw new Error('已结束的游戏不能洗牌。');
  validateStableState(state);
  return snapshot({ ...state, ...createPlayableBoard(state.matchLength, random, state.nextTileId) });
}

/** 输入只能来自上一回合的稳定终态，拒绝损坏的棋盘和编号计数器。 */
function validateStableState(state: GameState): void {
  if (state.board.length !== BOARD_ROWS || state.board.some(row => row.length !== BOARD_COLS)) {
    throw new RangeError('游戏棋盘必须为20行、30列。');
  }
  const tiles = state.board.flat();
  if (tiles.some(tile => !tile || !Number.isInteger(tile.kind) || tile.kind < 1 || tile.kind > ITEM_TYPES)) {
    throw new RangeError('操作必须从无空格且物品有效的稳定棋盘开始。');
  }
  const ids = tiles.map(tile => tile!.id);
  if (ids.some(id => !Number.isSafeInteger(id) || id < 1) || new Set(ids).size !== ids.length ||
      !Number.isSafeInteger(state.nextTileId) || state.nextTileId <= Math.max(...ids)) {
    throw new RangeError('物品编号必须唯一，下一编号必须大于现存编号。');
  }
  if (!Number.isSafeInteger(state.score) || state.score < 0) throw new RangeError('累计分数必须是非负整数。');
  if (findMatches(state.board, state.matchLength).cells.length) throw new Error('操作必须从无匹配的稳定棋盘开始。');
}

/** 执行真实规则并记录步骤；不修改输入状态，不依赖DOM、计时器或Phaser。 */
export function resolveMove(state: GameState, from: Position, to: Position, random: RandomSource = Math.random): MoveResolution {
  const reject = (reason: MoveResolution['reason']): MoveResolution => Object.freeze({
    outcome: 'rejected', reason, steps: Object.freeze([]), finalState: snapshot(state),
  });
  if (state.status === 'won') return reject('game-over');
  const inBounds = (position: Position): boolean => Number.isInteger(position.row) && Number.isInteger(position.col) &&
    position.row >= 0 && position.row < BOARD_ROWS && position.col >= 0 && position.col < BOARD_COLS;
  if (!inBounds(from) || !inBounds(to)) return reject('invalid-position');
  if (Math.abs(from.row - to.row) + Math.abs(from.col - to.col) !== 1) return reject('not-adjacent');
  if (!state.board[from.row]?.[from.col] || !state.board[to.row]?.[to.col]) return reject('empty-cell');
  validateStableState(state);

  // 工作状态与历史步骤分开，步骤只在阶段完成时捕获。
  let current = state;
  const steps: GameStep[] = [];
  const record = (details: StepDetails): void => {
    steps.push(freezeData({ ...details, index: steps.length, after: snapshot(current) }));
  };
  const finish = (outcome: 'invalid' | 'valid'): MoveResolution => Object.freeze({
    outcome, reason: null, steps: Object.freeze(steps), finalState: steps[steps.length - 1].after,
  });

  const first = state.board[from.row][from.col]!;
  const second = state.board[to.row][to.col]!;
  const moves: TileMove[] = [
    { id: first.id, from: { ...from }, to: { ...to } },
    { id: second.id, from: { ...to }, to: { ...from } },
  ];
  const swapped = copyBoard(state.board);
  swapped[from.row][from.col] = second;
  swapped[to.row][to.col] = first;
  current = { ...current, board: swapped };
  record({ kind: 'SWAP', moves });

  let round = 1;
  let matches = findMatches(current.board, state.matchLength);
  record({ kind: 'MATCH', round, matches });
  if (!matches.cells.length) {
    current = state;
    record({ kind: 'SWAP_BACK', moves: moves.map(move => ({ id: move.id, from: move.to, to: move.from })) });
    record({ kind: 'STABLE' });
    return finish('invalid');
  }

  while (matches.cells.length) {
    if (round > MAX_CHAIN_ROUNDS) throw new Error(`连锁超过${MAX_CHAIN_ROUNDS}轮，请检查随机源或算法；本回合未提交。`);
    const board = copyBoard(current.board);
    const removed = matches.cells.map(position => ({ tile: { ...board[position.row][position.col]! }, position }));
    for (const { position } of removed) board[position.row][position.col] = null;
    const scorePerTile = (round === 1 ? 10 : round === 2 ? 20 : 30) as 10 | 20 | 30;
    const scoreDelta = removed.length * scorePerTile;
    current = { ...current, board, score: current.score + scoreDelta };
    record({ kind: 'CLEAR', round, removed, count: removed.length, scorePerTile, scoreDelta });

    const gravity = applyGravity(current.board);
    current = { ...current, board: gravity.board };
    record({ kind: 'GRAVITY', moves: gravity.moves });

    const refill = refillBoard(current.board, current.nextTileId, random);
    current = { ...current, board: refill.board, nextTileId: refill.nextTileId };
    record({ kind: 'REFILL', spawned: refill.spawned });

    round++;
    matches = findMatches(current.board, state.matchLength);
    record({ kind: 'MATCH', round, matches });
  }

  // 一次交换的全部连锁结束后，才检查累计门槛和生成下一关棋盘。
  record({ kind: 'STABLE' });
  if (current.score >= 1200) {
    current = { ...current, level: 3, status: 'won' };
    record({ kind: 'WIN' });
  } else {
    const nextLevel = current.score >= 700 ? 3 : current.score >= 300 ? 2 : 1;
    if (nextLevel > current.level) {
      const fromLevel = current.level;
      const initialized = createPlayableBoard(current.matchLength, random, current.nextTileId);
      current = { ...current, ...initialized, level: nextLevel };
      record({ kind: 'LEVEL_CHANGE', fromLevel, toLevel: nextLevel });
    }
  }
  return finish('valid');
}
