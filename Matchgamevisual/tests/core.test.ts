import { describe, expect, it } from 'vitest';
import { findMatches } from '../src/core/match';
import { applyGravity, createBoard, refillBoard } from '../src/core/board';
import { createGame, hasLegalMove, resolveMove } from '../src/core/game';
import type { Board, GameState, ItemKind, Position, RandomSource } from '../src/core/types';

/** 把明确的数字夹具转换为带唯一编号的棋盘，0 表示空格。 */
function boardOf(values: number[][]): Board {
  let nextId = 1;
  return values.map(row => row.map(kind => kind === 0 ? null : { id: nextId++, kind: kind as ItemKind }));
}

/** 测试内固定种子，不给核心引入额外随机数依赖。 */
function seededRandom(seed: number): RandomSource {
  return () => {
    seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0;
    return seed / 4294967296;
  };
}

/** 明确指定补充的物品值，之后使用固定种子。 */
function sequenceRandom(values: number[]): RandomSource {
  let index = 0;
  const fallback = seededRandom(123);
  return () => index < values.length ? (values[index++] - 0.5) / 9 : fallback();
}

/** 完整600格夹具；首行第三格与下一行交换后恰好消除三格。 */
function playableState(score = 0, level: 1 | 2 | 3 = 1): GameState {
  const values = Array.from({ length: 20 }, (_, row) => Array.from({ length: 30 }, (_, col) => (row + col) % 9 + 1));
  values[0][1] = 1;
  values[0][2] = 2;
  values[1][2] = 1;
  return { board: boardOf(values), nickname: '测试玩家', matchLength: 3, score, level, status: 'playing', nextTileId: 601 };
}

/** 中部十字消除会让三列下落，中间一列同时跨过三个空格。 */
function gravityState(): GameState {
  const state = playableState();
  const values = state.board.map(row => row.map(tile => tile!.kind));
  values[9][10] = 1;
  values[10][8] = 4;
  values[10][9] = 1;
  values[10][10] = 2;
  values[10][11] = 1;
  values[11][10] = 1;
  values[12][10] = 1;
  return { ...state, board: boardOf(values) };
}

const swapFrom: Position = { row: 0, col: 2 };
const swapTo: Position = { row: 1, col: 2 };

describe('稳定棋盘的有效操作检测', () => {
  it('有合法操作时返回真，并保持原棋盘与计分', () => {
    const state = playableState();
    const before = JSON.stringify(state);
    expect(hasLegalMove(state)).toBe(true);
    expect(JSON.stringify(state)).toBe(before);
  });

  it.each([3, 4] as const)('k=%i无有效交换的棋盘返回假，不自动洗牌', k => {
    const state = playableState();
    const values = Array.from({ length: 20 }, (_, row) => Array.from({ length: 30 }, (_, col) => (row + col) % 9 + 1));
    const dead = { ...state, board: boardOf(values), matchLength: k };
    const before = JSON.stringify(dead);
    expect(hasLegalMove(dead)).toBe(false);
    expect(JSON.stringify(dead)).toBe(before);
  });

  it('胜利状态不再提供操作', () => {
    expect(hasLegalMove({ ...playableState(), status: 'won' })).toBe(false);
  });
});

describe('横纵连续段与统一标记', () => {
  it('消除超过 k 的整段，而非只消除前 k 格', () => {
    const matches = findMatches(boardOf([[1, 1, 1, 1, 2]]), 3);
    expect(matches.cells).toHaveLength(4);
    expect(matches.segments[0].cells).toHaveLength(4);
  });

  it('k=4 不消除三格，消除四格', () => {
    expect(findMatches(boardOf([[2, 2, 2, 3]]), 4).cells).toHaveLength(0);
    expect(findMatches(boardOf([[2, 2, 2, 2]]), 4).cells).toHaveLength(4);
  });

  it('横纵十字统一标记五格，交叉点只出现一次', () => {
    const matches = findMatches(boardOf([[2, 1, 3], [1, 1, 1], [4, 1, 5]]), 3);
    expect(matches.segments).toHaveLength(2);
    expect(matches.segments.map(segment => segment.direction)).toEqual(['horizontal', 'vertical']);
    expect(matches.cells).toHaveLength(5);
    expect(matches.cells.filter(cell => cell.row === 1 && cell.col === 1)).toHaveLength(1);
  });

  it('其他数字与空格打断连续段，空格本身不匹配', () => {
    expect(findMatches(boardOf([[1, 1, 2, 1, 1], [0, 0, 0, 0, 0]]), 3).cells).toHaveLength(0);
  });

  it('检测最后一列的纵向匹配', () => {
    const matches = findMatches(boardOf([[1, 9], [2, 9], [3, 9], [4, 9]]), 4);
    expect(matches.cells).toEqual([{ row: 0, col: 1 }, { row: 1, col: 1 }, { row: 2, col: 1 }, { row: 3, col: 1 }]);
  });
});

describe('棋盘初始化、重力与补充', () => {
  it.each([3, 4] as const)('k=%i 的固定随机源也能生成无初始匹配的完整棋盘', k => {
    for (const value of [0, 0.999999]) {
      const result = createBoard(k, () => value, 43);
      expect(result.board).toHaveLength(20);
      expect(result.board.every(row => row.length === 30)).toBe(true);
      expect(result.board.flat().every(tile => tile && tile.kind >= 1 && tile.kind <= 9)).toBe(true);
      expect(new Set(result.board.flat().map(tile => tile?.id)).size).toBe(600);
      expect(result.nextTileId).toBe(643);
      expect(findMatches(result.board, k).cells).toHaveLength(0);
    }
  });

  it.each([-0.1, 1, NaN, Infinity])('拒绝随机源返回非法值 %s', value => {
    expect(() => createBoard(3, () => value)).toThrow(/随机/);
  });

  it('相同数字也按各自编号保持顺序，并记录真实下落位置', () => {
    const board = boardOf([[1], [0], [1], [0], [3]]);
    const result = applyGravity(board);
    expect(result.board.map(row => row[0]?.id ?? null)).toEqual([null, null, 1, 2, 3]);
    expect(result.moves).toEqual([
      { id: 2, from: { row: 2, col: 0 }, to: { row: 3, col: 0 } },
      { id: 1, from: { row: 0, col: 0 }, to: { row: 2, col: 0 } },
    ]);
    expect(board.map(row => row[0]?.id ?? null)).toEqual([1, null, 2, null, 3]);
  });

  it('满列不移动，空列保持全空', () => {
    const board = boardOf([[1, 0], [2, 0], [3, 0]]);
    expect(applyGravity(board)).toEqual({ board, moves: [] });
  });

  it('补充按行优先消耗随机数，只填空格且允许产生连锁', () => {
    const board = boardOf([[0, 0, 0], [1, 2, 3]]);
    let calls = 0;
    const result = refillBoard(board, 4, () => { calls++; return 0; });
    expect(calls).toBe(3);
    expect(result.nextTileId).toBe(7);
    expect(result.board[0].map(tile => tile?.id)).toEqual([4, 5, 6]);
    expect(result.board[1]).toEqual(board[1]);
    expect(result.spawned.map(entry => entry.position)).toEqual([{ row: 0, col: 0 }, { row: 0, col: 1 }, { row: 0, col: 2 }]);
    expect(findMatches(result.board, 3).cells).toHaveLength(3);
    expect(board[0]).toEqual([null, null, null]);
  });
});

describe('完整操作、连锁计分与关卡', () => {
  it('建立独立可运行的核心游戏，保留昵称和设置', () => {
    const state = createGame('  玩家甲  ', 4, seededRandom(7));
    expect(state.nickname).toBe('玩家甲');
    expect(state.board).toHaveLength(20);
    expect(state.nextTileId).toBe(601);
    expect(state.matchLength).toBe(4);
    expect(state.score).toBe(0);
    expect(findMatches(state.board, 4).cells).toHaveLength(0);
  });

  it('空昵称不创建游戏', () => {
    expect(() => createGame('  ', 3)).toThrow(/昵称/);
  });

  it('相同种子与操作产生完全相同的步骤和终态', () => {
    expect(createGame('玩家', 3, seededRandom(42))).toEqual(createGame('玩家', 3, seededRandom(42)));
    const state = playableState();
    expect(resolveMove(state, swapFrom, swapTo, seededRandom(42))).toEqual(resolveMove(state, swapFrom, swapTo, seededRandom(42)));
  });

  it.each([
    [{ row: -1, col: 0 }, swapTo, 'invalid-position'],
    [{ row: 20, col: 0 }, swapTo, 'invalid-position'],
    [{ row: 0, col: 30 }, swapTo, 'invalid-position'],
    [{ row: 0.5, col: 2 }, swapTo, 'invalid-position'],
    [{ row: NaN, col: 2 }, swapTo, 'invalid-position'],
    [swapFrom, swapFrom, 'not-adjacent'],
    [swapFrom, { row: 2, col: 2 }, 'not-adjacent'],
    [swapFrom, { row: 1, col: 3 }, 'not-adjacent'],
  ] as const)('非法操作 %j → %j 无动画、无随机消耗', (from, to, reason) => {
    const state = playableState();
    const result = resolveMove(state, from, to, () => { throw new Error('不应消费随机数'); });
    expect(result.outcome).toBe('rejected');
    expect(result.reason).toBe(reason);
    expect(result.steps).toEqual([]);
    expect(result.finalState).toEqual(state);
  });

  it('无效交换产生真实交换和回退快照，完整恢复编号和状态', () => {
    const state = playableState(10);
    const from = { row: 18, col: 10 };
    const to = { row: 18, col: 11 };
    const result = resolveMove(state, from, to, () => { throw new Error('无效操作不应补充'); });
    expect(result.outcome).toBe('invalid');
    expect(result.steps.map(step => step.kind)).toEqual(['SWAP', 'MATCH', 'SWAP_BACK', 'STABLE']);
    expect(result.steps[0].after.board[from.row][from.col]).toEqual(state.board[to.row][to.col]);
    expect(result.finalState).toEqual(state);
  });

  it('有效交换输出完整阶段，CLEAR 留空格，REFILL 后继续检测', () => {
    const state = playableState();
    const before = JSON.stringify(state);
    const result = resolveMove(state, swapFrom, swapTo, sequenceRandom([7, 8, 9]));
    expect(result.outcome).toBe('valid');
    expect(result.steps.map(step => step.kind)).toEqual(['SWAP', 'MATCH', 'CLEAR', 'GRAVITY', 'REFILL', 'MATCH', 'STABLE']);
    expect(result.steps[2].after.board[0].slice(0, 3)).toEqual([null, null, null]);
    expect(result.steps[4].after.board[0].slice(0, 3).map(tile => tile?.kind)).toEqual([7, 8, 9]);
    expect(result.finalState.score).toBe(30);
    expect(findMatches(result.finalState.board, 3).cells).toHaveLength(0);
    expect(result.finalState.board.flat().every(Boolean)).toBe(true);
    expect(JSON.stringify(state)).toBe(before);
    expect(result.steps.at(-1)?.after).toEqual(result.finalState);
  });

  it('四轮三格计分30/60/90/90，第三轮后封顶', () => {
    const result = resolveMove(playableState(), swapFrom, swapTo, sequenceRandom([1, 1, 1, 1, 1, 1, 1, 1, 1, 7, 8, 9]));
    const clearSteps = result.steps.filter(step => step.kind === 'CLEAR');
    expect(clearSteps.map(step => step.round)).toEqual([1, 2, 3, 4]);
    expect(clearSteps.map(step => step.scorePerTile)).toEqual([10, 20, 30, 30]);
    expect(clearSteps.map(step => step.scoreDelta)).toEqual([30, 60, 90, 90]);
    expect(result.finalState.score).toBe(270);
  });

  it.each([[270, 1, 300, 2], [670, 2, 700, 3]] as const)('累计%s分后达到门槛，仅在稳定后进入下一关', (score, level, targetScore, nextLevel) => {
    const state = playableState(score, level);
    const result = resolveMove(state, swapFrom, swapTo, sequenceRandom([7, 8, 9]));
    expect(result.finalState.score).toBe(targetScore);
    expect(result.finalState.level).toBe(nextLevel);
    expect(result.steps.at(-2)?.kind).toBe('STABLE');
    expect(result.steps.at(-1)?.kind).toBe('LEVEL_CHANGE');
    expect(result.finalState.nickname).toBe(state.nickname);
    expect(result.finalState.matchLength).toBe(state.matchLength);
    expect(findMatches(result.finalState.board, 3).cells).toHaveLength(0);
    expect(Math.min(...result.finalState.board.flat().map(tile => tile!.id))).toBeGreaterThan(603);
  });

  it('一回合跨300和700门槛，只换一次棋盘并直接进入第三关', () => {
    const result = resolveMove(playableState(290), swapFrom, swapTo, sequenceRandom([...Array<number>(15).fill(1), 7, 8, 9]));
    expect(result.finalState.score).toBe(740);
    expect(result.finalState.level).toBe(3);
    expect(result.steps.filter(step => step.kind === 'LEVEL_CHANGE')).toHaveLength(1);
    const change = result.steps.at(-1);
    expect(change?.kind === 'LEVEL_CHANGE' && change.fromLevel).toBe(1);
    expect(change?.kind === 'LEVEL_CHANGE' && change.toLevel).toBe(3);
  });

  it('达到1200后仍结算剩余连锁，再获胜且不生成新棋盘', () => {
    const result = resolveMove(playableState(1170, 3), swapFrom, swapTo, sequenceRandom([1, 1, 1, 7, 8, 9]));
    expect(result.finalState.score).toBe(1260);
    expect(result.finalState.status).toBe('won');
    expect(result.steps.filter(step => step.kind === 'CLEAR')).toHaveLength(2);
    expect(result.steps.at(-2)?.kind).toBe('STABLE');
    expect(result.steps.at(-1)?.kind).toBe('WIN');
    expect(result.steps.some(step => step.kind === 'LEVEL_CHANGE')).toBe(false);
    expect(result.finalState.nextTileId).toBe(607);
    expect(resolveMove(result.finalState, swapFrom, swapTo).reason).toBe('game-over');
  });

  it('异常随机源造成持续连锁时明确报错，不静默截断或修改输入', () => {
    const state = playableState();
    const before = JSON.stringify(state);
    expect(() => resolveMove(state, swapFrom, swapTo, () => 0)).toThrow(/连锁.*100/);
    expect(JSON.stringify(state)).toBe(before);
  });

  it('下一次真实交换重新从每格10分计分', () => {
    const state = playableState();
    const values = state.board.map(row => row.map(tile => tile!.kind));
    values[10][0] = 1;
    values[10][1] = 1;
    values[10][2] = 2;
    values[11][2] = 1;
    const first = resolveMove({ ...state, board: boardOf(values) }, swapFrom, swapTo,
      sequenceRandom([1, 1, 1, 1, 1, 1, 1, 1, 1, 7, 8, 9]));
    expect(first.finalState.score).toBe(270);
    const second = resolveMove(first.finalState, { row: 10, col: 2 }, { row: 11, col: 2 }, seededRandom(42));
    const clear = second.steps.find(step => step.kind === 'CLEAR');
    expect(clear?.kind === 'CLEAR' && clear.round).toBe(1);
    expect(clear?.kind === 'CLEAR' && clear.scorePerTile).toBe(10);
    expect(clear?.kind === 'CLEAR' && clear.scoreDelta).toBe(30);
  });

  it('k=4 的有效交换消除四格', () => {
    const state = playableState();
    const values = state.board.map(row => row.map(tile => tile!.kind));
    values[0][2] = 1;
    values[0][3] = 2;
    values[1][2] = 4;
    values[1][3] = 1;
    const result = resolveMove({ ...state, board: boardOf(values), matchLength: 4 },
      { row: 0, col: 3 }, { row: 1, col: 3 }, sequenceRandom([6, 7, 8, 9]));
    expect(result.outcome).toBe('valid');
    expect(result.finalState.score).toBe(40);
    expect(findMatches(result.finalState.board, 4).cells).toHaveLength(0);
  });

  it('一次交换产生横纵交叉，实际只消除五格并计50分', () => {
    const state = playableState();
    const values = state.board.map(row => row.map(tile => tile!.kind));
    values[0][2] = 3;
    values[0][1] = 1;
    values[1][0] = 1;
    values[1][1] = 2;
    values[1][2] = 1;
    values[2][1] = 1;
    values[3][1] = 1;
    const input = { ...state, board: boardOf(values) };
    expect(findMatches(input.board, 3).cells).toHaveLength(0);
    const result = resolveMove(input, { row: 0, col: 1 }, { row: 1, col: 1 }, sequenceRandom([7, 8, 9, 6, 5]));
    const clear = result.steps.find(step => step.kind === 'CLEAR');
    expect(clear?.kind === 'CLEAR' && clear.count).toBe(5);
    expect(clear?.kind === 'CLEAR' && clear.scoreDelta).toBe(50);
  });

  it('拒绝已经有匹配或编号重复的损坏输入', () => {
    const state = playableState();
    const values = state.board.map(row => row.map(tile => tile!.kind));
    values[0][2] = 1;
    expect(() => resolveMove({ ...state, board: boardOf(values) }, swapFrom, swapTo)).toThrow(/稳定/);
    expect(() => resolveMove({ ...state, nextTileId: 1 }, swapFrom, swapTo)).toThrow(/编号/);
    const duplicated = state.board.map(row => [...row]);
    duplicated[19][29] = { ...duplicated[0][0]! };
    expect(() => resolveMove({ ...state, board: duplicated }, swapFrom, swapTo)).toThrow(/编号/);
  });
});

describe('步骤快照与动画重放契约', () => {
  it('冻结快照不会顺带冻结调用方原有的物品或位置对象', () => {
    const state = playableState();
    const originalTile = state.board[0][0]!;
    const from = { ...swapFrom };
    const result = resolveMove(state, from, swapTo, sequenceRandom([7, 8, 9]));
    expect(Object.isFrozen(originalTile)).toBe(false);
    expect(Object.isFrozen(from)).toBe(false);
    (originalTile as { kind: ItemKind }).kind = 9;
    expect(result.steps[0].after.board[0][0]?.kind).toBe(1);
  });

  it('快照数组和物品不可变，旧步骤保留独立中间状态', () => {
    const state = playableState();
    const result = resolveMove(state, swapFrom, swapTo, sequenceRandom([7, 8, 9]));
    const swap = result.steps[0].after;
    const clear = result.steps[2].after;
    expect(Object.isFrozen(swap)).toBe(true);
    expect(Object.isFrozen(swap.board)).toBe(true);
    expect(Object.isFrozen(swap.board[0])).toBe(true);
    expect(Object.isFrozen(swap.board[0][0])).toBe(true);
    expect(swap.board[0][0]?.kind).toBe(1);
    expect(clear.board[0][0]).toBeNull();
    expect(result.finalState.board[0][0]?.kind).toBe(7);
    expect(() => { (swap.board[0] as Array<unknown>)[0] = null; }).toThrow();
    expect(result.steps.map(step => step.index)).toEqual(result.steps.map((_, index) => index));
  });

  it.each([0, 270, 1170, 'gravity'] as const)('只按步骤的变化数据重放，逐步得到真实快照（夹具%s）', fixture => {
    const state = fixture === 'gravity' ? gravityState() : playableState(fixture, fixture >= 700 ? 3 : 1);
    const result = fixture === 'gravity'
      ? resolveMove(state, { row: 9, col: 10 }, { row: 10, col: 10 }, seededRandom(42))
      : resolveMove(state, swapFrom, swapTo, sequenceRandom([1, 1, 1, 7, 8, 9]));
    if (fixture === 'gravity') {
      const gravity = result.steps.find(step => step.kind === 'GRAVITY');
      expect(gravity?.kind === 'GRAVITY' && gravity.moves.length).toBeGreaterThan(20);
      expect(gravity?.kind === 'GRAVITY' && new Set(gravity.moves.map(move => move.from.col)).size).toBe(3);
      expect(gravity?.kind === 'GRAVITY' && gravity.moves.some(move => move.to.row - move.from.row === 3)).toBe(true);
    }
    let board = state.board.map(row => [...row]);
    let visibleScore = state.score;
    for (const step of result.steps) {
      if (step.kind === 'SWAP' || step.kind === 'SWAP_BACK' || step.kind === 'GRAVITY') {
        // 先收集来源，再统一移走，避免交换覆盖和同列移动覆盖。
        const moved = step.moves.map(move => ({ move, tile: board[move.from.row][move.from.col] }));
        for (const { move, tile } of moved) {
          expect(tile?.id).toBe(move.id);
          board[move.from.row][move.from.col] = null;
        }
        for (const { move, tile } of moved) board[move.to.row][move.to.col] = tile;
      } else if (step.kind === 'CLEAR') {
        for (const { position, tile } of step.removed) {
          expect(board[position.row][position.col]?.id).toBe(tile.id);
          board[position.row][position.col] = null;
        }
        visibleScore += step.scoreDelta;
      } else if (step.kind === 'REFILL') {
        for (const { position, tile } of step.spawned) {
          expect(board[position.row][position.col]).toBeNull();
          board[position.row][position.col] = tile;
        }
      } else if (step.kind === 'LEVEL_CHANGE') {
        board = step.after.board.map(row => [...row]);
      }
      expect(board).toEqual(step.after.board);
      expect(visibleScore).toBe(step.after.score);
    }
    expect(board).toEqual(result.finalState.board);
    expect(result.steps.at(-1)?.after).toEqual(result.finalState);
    expect(new Set(result.finalState.board.flat().map(tile => tile?.id)).size).toBe(600);
  });
});
