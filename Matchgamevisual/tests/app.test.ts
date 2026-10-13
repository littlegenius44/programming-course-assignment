import { describe, expect, it } from 'vitest';
import { GameController } from '../src/app';
import type { GameRenderer } from '../src/app';
import type { GameState, GameStep, LegalMove, MatchLength, Position } from '../src/core/types';
import { findMatches } from '../src/core/match';
import { findLegalMove, hasLegalMove } from '../src/core/game';

const settle = async (): Promise<void> => { for (let i = 0; i < 16; i++) await Promise.resolve(); };

/** 随机源先生成确定的四轮夹具，再补充到死局，后续初始化使用周期源。 */
function shuffleSource(k: MatchLength = 3) {
  const kinds = Array.from({ length: 20 }, (_, r) => Array.from({ length: 30 }, (_, c) => (r + c) % 9 + 1));
  for (let col = 0; col < k; col++) kinds[0][col] = col === k - 1 ? 2 : 1;
  kinds[1][k - 1] = 1;
  const draws: number[] = [];
  for (let row = 0; row < 20; row++) for (let col = 0; col < 30; col++) {
    const candidates = Array.from({ length: 9 }, (_, i) => i + 1).filter(kind =>
      !(col >= k - 1 && Array.from({ length: k - 1 }, (_, i) => kinds[row][col - i - 1]).every(value => value === kind)) &&
      !(row >= k - 1 && Array.from({ length: k - 1 }, (_, i) => kinds[row - i - 1][col]).every(value => value === kind)));
    draws.push((candidates.indexOf(kinds[row][col]) + 0.5) / candidates.length);
  }
  draws.push(...(k === 3 ? [1, 1, 1, 1, 1, 1, 1, 1, 1, 7, 8, 9] : [7, 8, 9, 6]).map(kind => (kind - 0.5) / 9));
  let calls = 0;
  let fail = false;
  return { calls: () => calls, fail: () => { fail = true; }, random: () => {
    if (fail) return NaN;
    const i = calls++;
    const offset = i - draws.length;
    return i < draws.length ? draws[i] : ((Math.floor(offset / 30) % 20 + offset % 30) % 9 + 0.5) / 9;
  } };
}
function seededRandom(seed = 7): () => number {
  return () => { seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0; return seed / 4294967296; };
}

/** 使用真实匹配扫描找一个合法操作，控制器仍执行真实结算。 */
function legalPair(state: GameState): [Position, Position] {
  const board = state.board.map(row => [...row]);
  for (let row = 0; row < 20; row++) for (let col = 0; col < 30; col++) {
    for (const to of [{ row, col: col + 1 }, { row: row + 1, col }]) {
      if (to.row >= 20 || to.col >= 30) continue;
      [board[row][col], board[to.row][to.col]] = [board[to.row][to.col], board[row][col]];
      const matched = findMatches(board, state.matchLength).cells.length > 0;
      [board[row][col], board[to.row][to.col]] = [board[to.row][to.col], board[row][col]];
      if (matched) return [{ row, col }, to];
    }
  }
  throw new Error('测试种子的棋盘没有有效交换。');
}

class TestRenderer implements GameRenderer {
  hint: LegalMove | null = null;
  state: GameState | null = null;
  enabled = false;
  paused = false;
  readonly steps: GameStep[] = [];
  readonly waiting: Array<() => void> = [];
  constructor(private readonly instant = false) {}
  showState(state: GameState): void { this.state = state; }
  showHint(move: LegalMove | null): void { this.hint = move; }
  setInputEnabled(enabled: boolean): void { this.enabled = enabled; }
  setPaused(paused: boolean): void { this.paused = paused; }
  playStep(step: GameStep, signal: AbortSignal): Promise<void> {
    this.steps.push(step);
    if (this.instant) return Promise.resolve();
    return new Promise((resolve, reject) => {
      const abort = (): void => reject(new DOMException('取消', 'AbortError'));
      signal.addEventListener('abort', abort, { once: true });
      this.waiting.push(() => { signal.removeEventListener('abort', abort); resolve(); });
    });
  }
  complete(): void { this.waiting.shift()?.(); }
}

describe('完整游戏会话', () => {
  it.each([3, 4] as const)('k=%i 普通及三档演示洗牌终态一致，随后提示可以执行', async k => {
    const results = [];
    for (const playback of [{ demo: false, speed: 1 }, { demo: true, speed: 1 },
      { demo: true, speed: 0.5 }, { demo: true, speed: 0.25 }] as const) {
      const renderer = new TestRenderer(true);
      const app = new GameController({ renderer, render: () => {}, random: shuffleSource(k).random });
      app.setPlayback(playback);
      app.start('洗牌玩家', k);
      await app.move({ row: 0, col: k - 1 }, { row: 1, col: k - 1 });
      expect(renderer.steps.at(-1)!.kind).toBe('SHUFFLE');
      expect(app.model.game!.score).toBe(k === 3 ? 270 : 40);
      await app.requestHint();
      expect(renderer.hint).not.toBeNull();
      results.push({ state: app.model.game, steps: [...renderer.steps] });
      const hint = renderer.hint!;
      await app.move(hint.from, hint.to);
      expect(app.model.game!.score).toBeGreaterThan(k === 3 ? 270 : 40);
    }
    for (const result of results) expect(result).toEqual(results[0]);
  });
  it.each(['manual', 'background'] as const)('%s 暂停跨消除与洗牌队列保持，计时不重置', async mode => {
    let now = 0;
    let stop = true;
    const renderer = new TestRenderer(true);
    const source = shuffleSource();
    const app = new GameController({ renderer, random: source.random, now: () => now, render: model => {
      if (model.reshuffling && stop) { stop = false; if (mode === 'manual') app.pause(); else app.setHidden(true); }
    } });
    app.start('暂停洗牌', 3);
    now = 1000;
    const playing = app.move({ row: 0, col: 2 }, { row: 1, col: 2 });
    for (let i = 0; i < 20 && stop; i++) await settle();
    expect(stop).toBe(false);
    now = 5000;
    expect(app.model.reshuffling).toBe(true);
    expect(app.model.paused).toBe(true);
    expect(renderer.steps.some(step => step.kind === 'SHUFFLE')).toBe(false);
    expect(app.model.elapsedMs).toBe(1000);
    const calls = source.calls();
    await app.requestHint();
    expect(source.calls()).toBe(calls);
    expect(renderer.hint).toBeNull();
    if (mode === 'background') { app.setHidden(false); await settle(); expect(app.model.paused).toBe(true); }
    app.resume();
    now = 5500;
    await playing;
    expect(app.model.game!.score).toBe(270);
    expect(app.model.elapsedMs).toBe(1500);
    expect(app.model.busy).toBe(false);
  });
  it('洗牌生成故障保留成绩并暂停', async () => {
    const source = shuffleSource();
    const renderer = new TestRenderer(true);
    const app = new GameController({ renderer, random: source.random, render: model => {
      if (model.reshuffling) source.fail();
    } });
    app.start('生成故障', 3);
    await app.move({ row: 0, col: 2 }, { row: 1, col: 2 });
    expect(app.model.game!.score).toBe(270);
    expect(app.model.faulted).toBe(true);
    expect(app.model.message).toContain('已保留成绩');
    expect(renderer.steps.some(step => step.kind === 'SHUFFLE')).toBe(false);
  });
  it('洗牌播放中重开隔离旧完成回调，清除洗牌锁', async () => {
    const renderer = new TestRenderer(true);
    const original = renderer.playStep.bind(renderer);
    let finish: (() => void) | undefined;
    renderer.playStep = (step, signal) => step.kind === 'SHUFFLE'
      ? new Promise(resolve => { renderer.steps.push(step); finish = resolve; }) : original(step, signal);
    const app = new GameController({ renderer, render: () => {}, random: shuffleSource().random });
    app.start('重开洗牌', 3);
    const playing = app.move({ row: 0, col: 2 }, { row: 1, col: 2 });
    for (let i = 0; i < 20 && !finish; i++) await settle();
    expect(app.model.reshuffling).toBe(true);
    app.restart();
    const restarted = app.model.game;
    finish!();
    await playing;
    expect(app.model.game).toBe(restarted);
    expect(app.model.game!.score).toBe(0);
    expect(app.model.reshuffling).toBe(false);
    expect(app.model.busy).toBe(false);
    expect(renderer.enabled).toBe(true);
  });
  it('死局自动洗牌保留完整回合270分和计时，输入直到洗牌完成才解锁', async () => {
    let now = 0;
    const renderer = new TestRenderer(true);
    const source = shuffleSource();
    const observed: boolean[] = [];
    const app = new GameController({ renderer, random: source.random, now: () => now,
      render: model => { if (model.game && model.screen === 'game' && model.game.score > 0) observed.push(model.busy); } });
    app.start('洗牌玩家', 3);
    now = 1600;
    await app.move({ row: 0, col: 2 }, { row: 1, col: 2 });
    const shuffle = renderer.steps.find(step => step.kind === 'SHUFFLE')!;
    expect(shuffle).toBeDefined();
    expect(shuffle.index).toBe(0);
    expect(shuffle.after.score).toBe(270);
    expect(app.model.game!.score).toBe(270);
    expect(app.model.elapsedMs).toBe(1600);
    expect(app.model.lastClear!.round).toBe(4);
    expect(app.model.algorithm!.title).toBe('自动洗牌');
    expect(app.model.reshuffling).toBe(false);
    expect(app.model.deadlocked).toBe(false);
    expect(hasLegalMove(app.model.game!)).toBe(true);
    expect(renderer.enabled).toBe(true);
    expect(observed.slice(0, -1).every(Boolean)).toBe(true);
    expect(renderer.steps.filter(step => step.kind === 'CLEAR')).toHaveLength(4);
  });
  it('洗牌期间退出保留新成绩，旧完成回调不覆盖结果', async () => {
    const renderer = new TestRenderer(true);
    const original = renderer.playStep.bind(renderer);
    let finish: (() => void) | undefined;
    renderer.playStep = (step, signal) => step.kind === 'SHUFFLE'
      ? new Promise(resolve => { renderer.steps.push(step); finish = resolve; }) : original(step, signal);
    const app = new GameController({ renderer, render: () => {}, random: shuffleSource().random });
    app.start('洗牌玩家', 3);
    const playing = app.move({ row: 0, col: 2 }, { row: 1, col: 2 });
    for (let i = 0; i < 20 && !finish; i++) await settle();
    expect(app.model.reshuffling).toBe(true);
    expect(app.model.game!.score).toBe(270);
    const deadBoard = app.model.game;
    app.quit();
    finish!();
    await playing;
    expect(app.model.game).toBe(deadBoard);
    expect(app.model.interrupted).toBe(false);
    expect(app.model.game!.score).toBe(270);
    expect(app.model.screen).toBe('result');
  });
  it('洗牌播放失败保留已提交分数，暂停等待重开或退出', async () => {
    const renderer = new TestRenderer(true);
    const original = renderer.playStep.bind(renderer);
    const errors: unknown[] = [];
    renderer.playStep = (step, signal) => step.kind === 'SHUFFLE' ? Promise.reject(new Error('洗牌动画失败')) : original(step, signal);
    const app = new GameController({ renderer, render: () => {}, random: shuffleSource().random, onError: e => errors.push(e) });
    app.start('洗牌玩家', 3);
    await app.move({ row: 0, col: 2 }, { row: 1, col: 2 });
    expect(app.model.game!.score).toBe(270);
    expect(app.model.paused).toBe(true);
    expect(app.model.faulted).toBe(true);
    expect(app.model.reshuffling).toBe(false);
    expect(app.model.message).toContain('已保留成绩');
    expect(errors).toHaveLength(1);
    expect(renderer.state).toBe(app.model.game);
  });
  it('免费提示返回真实位置，不变更状态、不消费随机数', async () => {
    const renderer = new TestRenderer(true);
    let calls = 0;
    const source = seededRandom();
    const app = new GameController({ renderer, render: () => {}, random: () => { calls++; return source(); } });
    app.start('提示玩家', 4);
    const state = app.model.game;
    const count = calls;
    await app.requestHint();
    expect(app.model.game).toBe(state);
    expect(calls).toBe(count);
    expect(renderer.hint).toEqual(findLegalMove(state!));
    const move = renderer.hint!;
    expect(app.model.message).toContain(`第${move.from.row + 1}行第${move.from.col + 1}列`);
    expect(app.model.message).toContain(move.to.col > move.from.col ? '向右' : '向下');
    const message = app.model.message;
    app.pause();
    await app.requestHint();
    expect(app.model.message).toBe(message);
    app.resume();
    app.quit();
    await app.requestHint();
    expect(app.model.game).toBe(state);
  });
  it('演示偏好保留到新局，非法设置不覆盖原偏好', () => {
    const app = new GameController({ renderer: new TestRenderer(true), render: () => {}, random: seededRandom() });
    app.setPlayback({ demo: true, speed: 0.5 });
    app.start('演示', 3);
    app.restart();
    expect(app.model.playback).toEqual({ demo: true, speed: 0.5 });
    expect(app.model.algorithm).toBeNull();
    expect(app.model.lastClear).toBeNull();
    expect(() => app.setPlayback({ demo: true, speed: 2 as 1 })).toThrow();
    expect(app.model.playback.speed).toBe(0.5);
  });

  it('最近CLEAR摘要来自真实消除，不把末尾空MATCH当成新连锁', async () => {
    const renderer = new TestRenderer(true);
    const app = new GameController({ renderer, render: () => {}, random: seededRandom() });
    app.start('演示', 3);
    await app.move(...legalPair(app.model.game!));
    const lastClear = renderer.steps.filter(s => s.kind === 'CLEAR').at(-1)!;
    expect(app.model.lastClear).toEqual({ round: lastClear.round, count: lastClear.count,
      scorePerTile: lastClear.scorePerTile, scoreDelta: lastClear.scoreDelta });
    const lastMatch = renderer.steps.filter(s => s.kind === 'MATCH').at(-1)!;
    expect(lastMatch.matches.cells).toHaveLength(0);
    expect(lastMatch.round).toBe(lastClear.round + 1);
    expect(app.model.algorithm!.title).toBe('棋盘稳定');
    app.quit();
    expect(app.model.algorithm).toBeNull();
    expect(app.model.lastClear).toBeNull();
  });

  it('普通与三档演示运行相同操作，步骤和终态完全相同', async () => {
    const results = [];
    for (const playback of [{ demo: false, speed: 1 }, { demo: true, speed: 1 },
      { demo: true, speed: 0.5 }, { demo: true, speed: 0.25 }] as const) {
      const renderer = new TestRenderer(true);
      const app = new GameController({ renderer, render: () => {}, random: seededRandom() });
      app.setPlayback(playback);
      app.start('演示', 3);
      await app.move(...legalPair(app.model.game!));
      results.push({ steps: renderer.steps, state: app.model.game });
    }
    for (const result of results) expect(result).toEqual(results[0]);
  });
  it('开始后允许操作，退出后冻结成绩和用时', () => {
    let now = 0;
    const renderer = new TestRenderer();
    const app = new GameController({ renderer, render: () => {}, now: () => now, random: seededRandom() });
    app.start('玩家甲', 3);
    expect(app.model.screen).toBe('game');
    expect(renderer.enabled).toBe(true);
    now = 2500;
    app.quit();
    expect(app.model.screen).toBe('result');
    expect(app.model.result).toBe('quit');
    expect(app.model.elapsedMs).toBe(2500);
    now = 9000;
    expect(app.model.elapsedMs).toBe(2500);
    expect(renderer.enabled).toBe(false);
  });

  it('操作锁拒绝连点，暂停同时冻结动画推进和计时', async () => {
    let now = 0;
    const renderer = new TestRenderer();
    const app = new GameController({ renderer, render: () => {}, now: () => now, random: seededRandom() });
    app.start('玩家甲', 3);
    const pair = legalPair(app.model.game!);
    const playing = app.move(...pair);
    await settle();
    expect(app.model.busy).toBe(true);
    expect(renderer.enabled).toBe(false);
    await app.move(...pair);
    expect(renderer.steps).toHaveLength(1);
    now = 1000;
    app.pause();
    now = 5000;
    renderer.complete();
    await settle();
    expect(app.model.paused).toBe(true);
    expect(app.model.elapsedMs).toBe(1000);
    expect(renderer.steps).toHaveLength(1);
    app.resume();
    await settle();
    expect(renderer.steps).toHaveLength(2);
    now = 5500;
    expect(app.model.elapsedMs).toBe(1500);
    app.quit();
    await playing;
  });

  it('后台返回仍等待主动继续，不覆盖主动暂停', () => {
    const renderer = new TestRenderer();
    const app = new GameController({ renderer, render: () => {}, random: seededRandom() });
    app.start('玩家', 3);
    app.setHidden(true);
    app.resume();
    expect(app.model.paused).toBe(true);
    app.setHidden(false);
    expect(app.model.paused).toBe(true);
    app.resume();
    expect(app.model.paused).toBe(false);
    app.pause();
    app.setHidden(true);
    app.setHidden(false);
    expect(app.model.paused).toBe(true);
  });

  it('退出动画中的回合只保留已提交分数，旧回调不改变结果', async () => {
    const renderer = new TestRenderer();
    const app = new GameController({ renderer, render: () => {}, random: seededRandom() });
    app.start('玩家', 3);
    const playing = app.move(...legalPair(app.model.game!));
    await settle();
    for (let i = 0; i < 3; i++) { renderer.complete(); await settle(); }
    expect(app.model.game!.score).toBeGreaterThan(0);
    app.quit();
    await playing;
    renderer.complete();
    await settle();
    expect(app.model.screen).toBe('result');
    expect(app.model.game!.score).toBe(0);
    expect(app.model.interrupted).toBe(true);
  });

  it('中途重开隔离旧播放，复用设置并重置分数与计时', async () => {
    let now = 0;
    const renderer = new TestRenderer();
    const app = new GameController({ renderer, render: () => {}, now: () => now, random: seededRandom() });
    app.start('玩家', 3);
    const playing = app.move(...legalPair(app.model.game!));
    await settle();
    app.pause();
    now = 8000;
    app.restart();
    await playing;
    renderer.complete();
    await settle();
    expect(app.model.game!.nickname).toBe('玩家');
    expect(app.model.game!.score).toBe(0);
    expect(app.model.elapsedMs).toBe(0);
    expect(app.model.busy).toBe(false);
    expect(app.model.paused).toBe(false);
    expect(renderer.enabled).toBe(true);
  });

  it('真实操作完成一局，累计得分后进入胜利结果', async () => {
    const renderer = new TestRenderer(true);
    const app = new GameController({ renderer, render: () => {}, random: seededRandom() });
    app.start('玩家', 3);
    for (let i = 0; i < 100 && app.model.screen !== 'result'; i++) await app.move(...legalPair(app.model.game!));
    expect(app.model.result).toBe('won');
    expect(app.model.screen).toBe('result');
    expect(app.model.game!.score).toBeGreaterThanOrEqual(1200);
    expect(app.model.game!.level).toBe(3);
    expect(renderer.enabled).toBe(false);
    expect(renderer.steps.at(-1)!.kind).toBe('WIN');
  });

  it('周期死局随机源也能生成可玩开局', async () => {
    let index = 0;
    const random = (): number => {
      const row = Math.floor(index / 30) % 20;
      const col = index++ % 30;
      return ((row + col) % 9 + 0.5) / 9;
    };
    const renderer = new TestRenderer(true);
    const app = new GameController({ renderer, render: () => {}, random });
    app.start('死局验证', 4);
    expect(app.model.deadlocked).toBe(false);
    expect(renderer.enabled).toBe(true);
    expect(legalPair(app.model.game!)).toHaveLength(2);
    app.restart();
    expect(app.model.game!.score).toBe(0);
    expect(app.model.deadlocked).toBe(false);
    app.quit();
    expect(app.model.screen).toBe('result');
  });

  it('动画失败恢复已提交局面并停止计时，重开解除故障', async () => {
    let now = 0;
    const renderer = new TestRenderer(true);
    const error = new Error('模拟动画失败');
    const errors: unknown[] = [];
    renderer.playStep = () => Promise.reject(error);
    const app = new GameController({ renderer, render: () => {}, now: () => now,
      random: seededRandom(), onError: value => errors.push(value) });
    app.start('故障验证', 3);
    const stable = app.model.game;
    now = 1000;
    await app.move(...legalPair(stable!));
    expect(app.model.game).toBe(stable);
    expect(renderer.state).toBe(stable);
    expect(app.model.faulted).toBe(true);
    expect(app.model.busy).toBe(false);
    expect(errors).toEqual([error]);
    now = 8000;
    app.resume();
    expect(app.model.paused).toBe(true);
    expect(app.model.elapsedMs).toBe(1000);
    app.restart();
    expect(app.model.faulted).toBe(false);
    expect(app.model.paused).toBe(false);
    expect(renderer.enabled).toBe(true);
  });
});
