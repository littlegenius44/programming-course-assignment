import type { GameStep } from '../core/types';
import { timingFor } from './playback';
import type { PlaybackOptions, PlaybackTiming } from './playback';

export interface StepRenderer {
  playStep(step: GameStep, signal: AbortSignal, timing: PlaybackTiming): Promise<void>;
  setPaused(paused: boolean): void;
}

export interface PlaybackCallbacks {
  onStart(step: GameStep): void;
  onFinish(step: GameStep): void;
}

/** 单队列播放器负责时序和取消，不执行规则或直接写入游戏终态。 */
export class StepPlayer {
  private generation = 0;
  private paused = false;
  private active: AbortController | null = null;
  private waiting: Array<() => void> = [];
  private options: PlaybackOptions = { demo: false, speed: 1 };
  constructor(private readonly renderer: StepRenderer) {}

  setOptions(options: PlaybackOptions): void {
    timingFor(options);
    this.options = { ...options };
  }

  async play(steps: ReadonlyArray<GameStep>, callbacks: PlaybackCallbacks): Promise<boolean> {
    const paused = this.paused;
    this.cancel();
    if (paused) this.pause();
    const generation = this.generation;
    const controller = new AbortController();
    this.active = controller;
    try {
      for (const step of steps) {
        if (!await this.waitUntilRunning(generation)) return false;
        // 整个步骤及其多段动画使用同一份时长，设置修改留给下一步。
        const timing = timingFor(this.options);
        callbacks.onStart(step);
        await this.renderer.playStep(step, controller.signal, timing);
        if (!await this.waitUntilRunning(generation)) return false;
        callbacks.onFinish(step);
      }
      return generation === this.generation;
    } catch (error) {
      if (controller.signal.aborted) return false;
      throw error;
    } finally {
      if (this.active === controller) this.active = null;
    }
  }

  pause(): void {
    this.paused = true;
    this.renderer.setPaused(true);
  }
  resume(): void {
    this.paused = false;
    this.renderer.setPaused(false);
    this.releaseWaiting();
  }
  cancel(): void {
    this.generation++;
    this.active?.abort();
    this.active = null;
    this.resume();
  }
  private releaseWaiting(): void {
    for (const release of this.waiting.splice(0)) release();
  }
  private async waitUntilRunning(generation: number): Promise<boolean> {
    while (this.paused && generation === this.generation) await new Promise<void>(resolve => this.waiting.push(resolve));
    return generation === this.generation;
  }
}
