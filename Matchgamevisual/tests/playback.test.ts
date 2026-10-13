import { describe, expect, it } from 'vitest';
import { createGame } from '../src/core/game';
import type { GameStep } from '../src/core/types';
import { StepPlayer } from '../src/presentation/StepPlayer';
import type { StepRenderer } from '../src/presentation/StepPlayer';
import type { PlaybackTiming } from '../src/presentation/playback';

class ControlledRenderer implements StepRenderer {
  readonly started: number[] = [];
  readonly waiting: Array<() => void> = [];
  readonly timings: PlaybackTiming[] = [];
  paused = false;
  playStep(step: GameStep, signal: AbortSignal, timing: PlaybackTiming): Promise<void> {
    this.started.push(step.index);
    this.timings.push(timing);
    return new Promise((resolve, reject) => {
      const abort = (): void => reject(new DOMException('取消', 'AbortError'));
      signal.addEventListener('abort', abort, { once: true });
      this.waiting.push(() => { signal.removeEventListener('abort', abort); resolve(); });
    });
  }
  setPaused(paused: boolean): void { this.paused = paused; }
  complete(): void { this.waiting.shift()?.(); }
}

const state = createGame('测试', 3, () => 0);
const steps: GameStep[] = [0, 1].map(index => ({ kind: 'STABLE', index, after: state }));
const settle = async (): Promise<void> => { for (let i = 0; i < 8; i++) await Promise.resolve(); };

describe('动画步骤串行、暂停与取消', () => {
  it('开始独立新队列保留暂停，显式取消才解除暂停', async () => {
    const renderer = new ControlledRenderer();
    const player = new StepPlayer(renderer);
    player.pause();
    const playing = player.play(steps, { onStart: () => {}, onFinish: () => {} });
    await settle();
    expect(renderer.paused).toBe(true);
    expect(renderer.started).toHaveLength(0);
    player.resume();
    await settle();
    expect(renderer.started).toEqual([0]);
    player.cancel();
    expect(await playing).toBe(false);
    expect(renderer.paused).toBe(false);
  });
  it('修改演示设置从下一步骤生效，不改变已启动步骤的时长', async () => {
    const renderer = new ControlledRenderer();
    const player = new StepPlayer(renderer);
    const playing = player.play(steps, { onStart: () => {}, onFinish: () => {} });
    await settle();
    player.setOptions({ demo: true, speed: 0.25 });
    expect(renderer.timings[0].scale).toBe(1);
    renderer.complete();
    await settle();
    expect(renderer.timings[1].scale).toBe(4);
    renderer.complete();
    expect(await playing).toBe(true);
  });
  it('前一步未完成不启动下一步，每一步只发布一次', async () => {
    const renderer = new ControlledRenderer();
    const finished: number[] = [];
    const player = new StepPlayer(renderer);
    const playing = player.play(steps, { onStart: () => {}, onFinish: step => finished.push(step.index) });
    await settle();
    expect(renderer.started).toEqual([0]);
    expect(finished).toEqual([]);
    renderer.complete();
    await settle();
    expect(renderer.started).toEqual([0, 1]);
    expect(finished).toEqual([0]);
    renderer.complete();
    expect(await playing).toBe(true);
    expect(finished).toEqual([0, 1]);
  });

  it('暂停期间不发布完成或推进，恢复后继续同一队列', async () => {
    const renderer = new ControlledRenderer();
    const finished: number[] = [];
    const player = new StepPlayer(renderer);
    const playing = player.play(steps, { onStart: () => {}, onFinish: step => finished.push(step.index) });
    await settle();
    player.pause();
    expect(renderer.paused).toBe(true);
    renderer.complete();
    await settle();
    expect(finished).toEqual([]);
    expect(renderer.started).toEqual([0]);
    player.resume();
    await settle();
    expect(finished).toEqual([0]);
    renderer.complete();
    expect(await playing).toBe(true);
  });

  it('取消后旧动画完成也不能发布状态', async () => {
    const renderer = new ControlledRenderer();
    const finished: number[] = [];
    const player = new StepPlayer(renderer);
    const playing = player.play(steps, { onStart: () => {}, onFinish: step => finished.push(step.index) });
    await settle();
    player.cancel();
    expect(await playing).toBe(false);
    renderer.complete();
    await settle();
    expect(finished).toEqual([]);
    expect(renderer.started).toEqual([0]);
  });

  it('暂停等待时取消不会挂起旧播放', async () => {
    const renderer = new ControlledRenderer();
    const player = new StepPlayer(renderer);
    const playing = player.play(steps, { onStart: () => {}, onFinish: () => {} });
    await settle();
    player.pause();
    renderer.complete();
    await settle();
    player.cancel();
    expect(await playing).toBe(false);
  });
});
