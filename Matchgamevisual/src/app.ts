import { createGame, findLegalMove, hasLegalMove, reshuffleGame, resolveMove } from './core/game';
import type { GameState, GameStep, LegalMove, MatchLength, Position, RandomSource } from './core/types';
import { StepPlayer } from './presentation/StepPlayer';
import type { StepRenderer } from './presentation/StepPlayer';
import { SessionClock } from './session';
import { explainStep } from './presentation/explanation';
import type { StepExplanation } from './presentation/explanation';
import type { PlaybackOptions } from './presentation/playback';

export type ClearSummary = Pick<Extract<GameStep, { kind: 'CLEAR' }>, 'round' | 'count' | 'scorePerTile' | 'scoreDelta'>;

export interface GameRenderer extends StepRenderer {
  showState(state: GameState): void;
  setInputEnabled(enabled: boolean): void;
  showHint(move: LegalMove | null): void;
}

export interface AppModel {
  readonly screen: 'start' | 'game' | 'result';
  readonly game: GameState | null;
  readonly elapsedMs: number;
  readonly busy: boolean;
  readonly paused: boolean;
  readonly hidden: boolean;
  readonly deadlocked: boolean;
  readonly reshuffling: boolean;
  readonly stage: string;
  readonly message: string;
  readonly result: 'won' | 'quit' | null;
  readonly interrupted: boolean;
  readonly faulted: boolean;
  readonly playback: PlaybackOptions;
  readonly algorithm: StepExplanation | null;
  readonly lastClear: ClearSummary | null;
}

export interface AppOptions {
  readonly renderer: GameRenderer;
  readonly render: (model: AppModel) => void;
  readonly now?: () => number;
  readonly random?: RandomSource;
  readonly onError?: (error: unknown) => void;
}

/** 应用会话协调器：最终提交与当前播放快照分开。 */
export class GameController {
  private committed: GameState | null = null;
  private visible: GameState | null = null;
  private screen: AppModel['screen'] = 'start';
  private result: AppModel['result'] = null;
  private busy = false;
  private manualPaused = false;
  private backgroundPaused = false;
  private hidden = false;
  private deadlocked = false;
  private reshuffling = false;
  private faulted = false;
  private interrupted = false;
  private stage = '等待开始';
  private message = '';
  private generation = 0;
  private settings: { nickname: string; matchLength: MatchLength } | null = null;
  private playback: PlaybackOptions = Object.freeze({ demo: false, speed: 1 });
  private algorithm: StepExplanation | null = null;
  private lastClear: ClearSummary | null = null;
  private readonly player: StepPlayer;
  private readonly clock: SessionClock;
  private readonly random: RandomSource;

  constructor(private readonly options: AppOptions) {
    this.player = new StepPlayer(options.renderer);
    this.clock = new SessionClock(options.now);
    this.random = options.random ?? Math.random;
    this.publish();
  }
  get model(): AppModel {
    return { screen: this.screen, game: this.visible, elapsedMs: this.screen === 'start' ? 0 : this.clock.elapsedMs,
      busy: this.busy, paused: this.manualPaused || this.backgroundPaused, hidden: this.hidden,
      deadlocked: this.deadlocked, reshuffling: this.reshuffling, stage: this.stage, message: this.message, result: this.result,
      interrupted: this.interrupted, faulted: this.faulted, playback: this.playback,
      algorithm: this.algorithm, lastClear: this.lastClear };
  }
  setPlayback(options: PlaybackOptions): void {
    this.player.setOptions(options);
    this.playback = Object.freeze({ ...options });
    this.publish();
  }
  start(nickname: string, matchLength: MatchLength): void {
    const game = createGame(nickname, matchLength, this.random);
    this.generation++;
    this.player.cancel();
    this.committed = this.visible = game;
    this.settings = { nickname: game.nickname, matchLength };
    this.clearAlgorithm();
    this.screen = 'game';
    this.result = null;
    this.busy = this.manualPaused = this.faulted = this.interrupted = this.reshuffling = false;
    this.backgroundPaused = this.hidden;
    this.deadlocked = false;
    this.stage = '等待操作';
    this.message = '点击两个相邻物品进行交换，按 H 获取免费提示。';
    this.clock.start();
    if (this.hidden) { this.clock.pause(); this.player.pause(); }
    // 先显示游戏页面，再让Phaser读取已展开容器的尺寸。
    this.publish();
    this.options.renderer.showState(game);
    this.options.renderer.showHint(null);
  }

  async move(from: Position, to: Position): Promise<void> {
    if (!this.committed || this.screen !== 'game' || this.busy || this.model.paused || this.deadlocked || this.faulted) return;
    const generation = this.generation;
    this.clearAlgorithm();
    this.busy = true;
    this.publish();
    try {
      const resolution = resolveMove(this.committed, from, to, this.random);
      if (resolution.outcome === 'rejected') {
        this.busy = false;
        this.message = '请选择棋盘内两个相邻物品。';
        this.publish();
        return;
      }
      const completed = await this.playSteps(resolution.steps, generation);
      if (!completed || generation !== this.generation) return;
      // 消除回合先提交；后续独立洗牌失败或取消也不能丢失这份成绩。
      this.committed = this.visible = resolution.finalState;
      if (this.committed.status === 'won') {
        this.busy = false;
        this.clock.pause();
        this.screen = 'result';
        this.result = 'won';
        this.stage = '胜利';
      } else if (!hasLegalMove(this.committed)) {
        await this.performReshuffle(generation);
        return;
      } else {
        this.busy = false;
        this.stage = '等待操作';
        this.message = resolution.outcome === 'invalid' ? '交换未产生消除，已恢复。' : '本回合已完成，继续交换。';
      }
      this.publish();
    } catch (error) {
      this.handleFailure(error, generation);
    }
  }

  async requestHint(): Promise<void> {
    if (!this.committed || this.screen !== 'game' || this.busy || this.model.paused || this.faulted || this.committed.status !== 'playing') return;
    const generation = this.generation;
    try {
      const move = findLegalMove(this.committed);
      if (!move) {
        await this.performReshuffle(generation);
        return;
      }
      this.options.renderer.showHint(move);
      const direction = move.to.col > move.from.col ? '向右' : '向下';
      this.message = `提示：第${move.from.row + 1}行第${move.from.col + 1}列${direction}交换，可以产生消除。`;
      this.publish();
    } catch (error) {
      this.handleFailure(error, generation);
    }
  }

  private playSteps(steps: ReadonlyArray<GameStep>, generation: number): Promise<boolean> {
    return this.player.play(steps, {
      onStart: step => {
        if (generation !== this.generation) return;
        this.algorithm = explainStep(step);
        this.stage = this.algorithm.title;
        if (step.kind === 'CLEAR') {
          this.lastClear = Object.freeze({ round: step.round, count: step.count,
            scorePerTile: step.scorePerTile, scoreDelta: step.scoreDelta });
        }
        this.message = step.kind === 'CLEAR'
          ? `消除${step.count}个物品，+${step.scoreDelta}分（每格${step.scorePerTile}分）。`
          : step.kind === 'SHUFFLE' ? '当前无可用操作，正在重新生成棋盘。'
          : step.kind === 'MATCH' && step.matches.cells.length ? `发现${step.matches.cells.length}格匹配。` : '';
        this.publish();
      },
      onFinish: step => {
        if (generation !== this.generation) return;
        this.visible = step.after;
        this.publish();
      },
    });
  }

  private async performReshuffle(generation: number): Promise<void> {
    this.busy = this.reshuffling = this.deadlocked = true;
    this.stage = '自动洗牌';
    this.message = '当前无可用操作，正在重新生成棋盘。';
    this.options.renderer.showHint(null);
    this.publish();
    const after = reshuffleGame(this.committed!, this.random);
    const step: GameStep = Object.freeze({ kind: 'SHUFFLE', reason: 'no-legal-move', index: 0, after });
    if (!await this.playSteps(Object.freeze([step]), generation) || generation !== this.generation) return;
    this.committed = this.visible = after;
    this.busy = this.reshuffling = this.deadlocked = false;
    this.stage = '等待操作';
    this.message = '棋盘已重新生成，可以继续操作。按 H 获取免费提示。';
    this.publish();
  }

  private handleFailure(error: unknown, generation: number): void {
    if (generation !== this.generation) return;
    const duringShuffle = this.reshuffling;
    this.player.cancel();
    this.visible = this.committed;
    this.clearAlgorithm();
    this.busy = this.reshuffling = false;
    this.faulted = this.manualPaused = true;
    this.clock.pause();
    this.player.pause();
    this.stage = '已暂停';
    this.message = duringShuffle ? '棋盘未能重新生成，已保留成绩，请重新开始或退出。' : '本回合未能完成，请重新开始或退出。';
    this.publish();
    this.options.renderer.showState(this.committed!);
    this.options.onError?.(error);
  }

  pause(): void {
    if (this.screen !== 'game') return;
    this.manualPaused = true;
    this.applyPause();
  }
  resume(): void {
    if (this.screen !== 'game' || this.hidden || this.faulted) return;
    this.manualPaused = this.backgroundPaused = false;
    this.clock.resume();
    this.player.resume();
    this.publish();
  }
  setHidden(hidden: boolean): void {
    this.hidden = hidden;
    if (this.screen === 'game' && hidden) {
      this.backgroundPaused = true;
      this.applyPause();
    } else this.publish();
  }
  quit(): void {
    if (this.screen !== 'game' || !this.committed) return;
    this.interrupted = this.busy && !this.reshuffling;
    this.generation++;
    this.player.cancel();
    this.clock.pause();
    this.visible = this.committed;
    this.clearAlgorithm();
    this.busy = this.manualPaused = this.backgroundPaused = this.faulted = this.reshuffling = false;
    this.options.renderer.showHint(null);
    this.screen = 'result';
    this.result = 'quit';
    this.stage = '整局结束';
    this.publish();
  }
  restart(): void {
    if (this.settings) this.start(this.settings.nickname, this.settings.matchLength);
  }
  backToStart(): void {
    this.generation++;
    this.player.cancel();
    this.clock.pause();
    this.committed = this.visible = null;
    this.clearAlgorithm();
    this.screen = 'start';
    this.result = null;
    this.busy = this.manualPaused = this.backgroundPaused = this.deadlocked = this.faulted = this.interrupted = this.reshuffling = false;
    this.options.renderer.showHint(null);
    this.publish();
  }
  noteSelection(position: Position | null): void {
    if (this.screen !== 'game' || this.busy || this.model.paused) return;
    this.message = position ? `已选中第${position.row + 1}行、第${position.col + 1}列，请点击相邻物品。` : '已取消选择。';
    this.publish();
  }
  tick(): void { if (this.screen === 'game') this.publish(); }
  dispose(): void {
    this.generation++;
    this.player.cancel();
    this.clock.pause();
    this.options.renderer.setInputEnabled(false);
  }
  private applyPause(): void {
    this.clock.pause();
    this.player.pause();
    this.publish();
  }
  private clearAlgorithm(): void { this.algorithm = null; this.lastClear = null; }
  private publish(): void {
    const model = this.model;
    this.options.renderer.setInputEnabled(model.screen === 'game' && !model.busy && !model.paused && !model.deadlocked && !model.faulted);
    this.options.render(model);
  }
}
