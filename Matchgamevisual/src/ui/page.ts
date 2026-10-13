import type { AppModel } from '../app';
import type { MatchLength } from '../core/types';
import type { PlaybackOptions, PlaybackSpeed } from '../presentation/playback';

interface PageActions {
  start(nickname: string, matchLength: MatchLength): void;
  pause(): void;
  resume(): void;
  restart(): void;
  quit(): void;
  back(): void;
  playback(options: PlaybackOptions): void;
  hint(): void;
}

function element<T extends HTMLElement>(id: string): T {
  const node = document.getElementById(id);
  if (!node) throw new Error(`缺少页面元素：${id}`);
  return node as T;
}

export function formatElapsed(milliseconds: number): string {
  const seconds = Math.floor(milliseconds / 1000);
  const minutes = Math.floor(seconds / 60);
  return `${String(minutes).padStart(2, '0')}:${String(seconds % 60).padStart(2, '0')}`;
}

/** 原生页面只展示会话数据，并把按钮意图交给应用控制器。 */
export class PageUI {
  private readonly dialog = element<HTMLDialogElement>('pause-dialog');
  private readonly removers: (() => void)[] = [];
  private previousScreen: AppModel['screen'] | null = null;

  constructor(actions: PageActions) {
    const updatePlayback = (): void => actions.playback({ demo: element<HTMLInputElement>('demo-mode').checked,
      speed: Number(element<HTMLSelectElement>('playback-speed').value) as PlaybackSpeed });
    this.listen('demo-mode', 'change', updatePlayback);
    this.listen('playback-speed', 'change', updatePlayback);
    this.listen('start-form', 'submit', event => {
      event.preventDefault();
      try {
        actions.start(element<HTMLInputElement>('nickname').value, Number(element<HTMLSelectElement>('match-length').value) as MatchLength);
        element('form-status').textContent = '';
      } catch {
        element('form-status').textContent = '请输入有效昵称后再开始。';
      }
    });
    const buttons: [string, () => void][] = [
      ['hint', () => actions.hint()],
      ['pause', () => actions.pause()], ['resume', () => actions.resume()],
      ['restart', () => actions.restart()], ['quit', () => actions.quit()],
      ['pause-restart', () => actions.restart()], ['pause-quit', () => actions.quit()],
      ['result-restart', () => actions.restart()], ['back', () => actions.back()],
    ];
    for (const [id, action] of buttons) this.listen(id, 'click', action);
    this.listen('pause-dialog', 'cancel', event => { event.preventDefault(); actions.resume(); });
    const hintKey = (event: KeyboardEvent): void => {
      if (event.key.toLowerCase() !== 'h' || event.repeat || event.ctrlKey || event.altKey || event.metaKey) return;
      const target = event.target;
      if (target instanceof HTMLElement && (target.isContentEditable ||
        target instanceof HTMLTextAreaElement || target instanceof HTMLSelectElement ||
        target instanceof HTMLInputElement && !['checkbox', 'radio', 'button', 'submit', 'reset'].includes(target.type))) return;
      event.preventDefault();
      actions.hint();
    };
    document.addEventListener('keydown', hintKey);
    this.removers.push(() => document.removeEventListener('keydown', hintKey));
  }

  setReady(): void {
    element<HTMLButtonElement>('start').disabled = false;
    element('form-status').textContent = '准备好了，开始一局吧。';
  }

  render(model: AppModel): void {
    element('start-page').hidden = model.screen !== 'start';
    element('game-page').hidden = model.screen !== 'game';
    element('result-page').hidden = model.screen !== 'result';
    element('game-controls').hidden = model.screen !== 'game';
    element<HTMLButtonElement>('hint').disabled = model.screen !== 'game' || model.game?.status !== 'playing' ||
      model.busy || model.paused || model.faulted;
    element<HTMLInputElement>('demo-mode').checked = model.playback.demo;
    const speed = element<HTMLSelectElement>('playback-speed');
    speed.value = String(model.playback.speed);
    speed.disabled = !model.playback.demo;
    element('game-page').classList.toggle('demo-mode', model.playback.demo);
    element('algorithm-panel').hidden = !model.playback.demo;
    element('algorithm-title').textContent = model.algorithm?.title ?? '等待操作';
    element('algorithm-description').textContent = model.algorithm?.description ?? '交换相邻物品，观察每个阶段的真实变化。';
    for (const node of element('algorithm-flow').querySelectorAll<HTMLElement>('[data-phase]')) {
      if (model.algorithm?.phase != null && node.dataset.phase === String(model.algorithm.phase)) node.setAttribute('aria-current', 'step');
      else node.removeAttribute('aria-current');
    }
    element('clear-stats').hidden = !model.lastClear;
    if (model.lastClear) {
      element('chain-label').textContent = model.lastClear.round > 1 ? `CHAIN ×${model.lastClear.round}` : '第1轮';
      element('clear-count').textContent = `${model.lastClear.count} 格`;
      element('per-tile').textContent = `${model.lastClear.scorePerTile} 分`;
      element('score-delta').textContent = `+${model.lastClear.scoreDelta}`;
    }
    const state = model.game;
    if (state) {
      const target = [300, 700, 1200][state.level - 1]!;
      element('player-name').textContent = state.nickname;
      element('score').textContent = String(state.score);
      element('level').textContent = `${state.level} / 3`;
      element('target').textContent = String(target);
      element('elapsed').textContent = formatElapsed(model.elapsedMs);
      element('rule').textContent = `连续 ${state.matchLength} 个`;
      const progress = element<HTMLProgressElement>('score-progress');
      progress.max = target;
      progress.value = Math.min(state.score, target);
      element('stage').textContent = model.paused ? '已暂停' : model.stage;
      element('status').textContent = model.message || '正在播放本回合……';
      element('result-title').textContent = model.result === 'won' ? '三关完成！' : '本局已结束';
      element('result-player').textContent = state.nickname;
      element('result-score').textContent = String(state.score);
      element('result-level').textContent = `${state.level} / 3`;
      element('result-time').textContent = formatElapsed(model.elapsedMs);
      element('result-note').textContent = model.interrupted
        ? '退出时未完成的回合未计入最终得分。'
        : model.result === 'won' ? '累计得分达到 1200 分，恭喜完成挑战。' : '可以使用相同设置重新开始，或返回修改设置。';
    }
    if (model.screen === 'game' && model.paused) {
      element('pause-title').textContent = model.faulted ? '回合已中止' : '游戏已暂停';
      element('pause-note').textContent = model.faulted ? model.message
        : model.hidden ? '页面在后台，计时和动画已停止。返回后点击继续。' : '计时和动画已停止，点击继续恢复。';
      element<HTMLButtonElement>('resume').disabled = model.hidden;
      element('resume').hidden = model.faulted;
      if (!this.dialog.open) this.dialog.showModal();
    } else if (this.dialog.open) this.dialog.close();
    if (model.screen !== this.previousScreen) {
      this.previousScreen = model.screen;
      if (model.screen === 'start') element('nickname').focus();
      if (model.screen === 'result') element('result-restart').focus();
    }
  }

  dispose(): void {
    for (const remove of this.removers) remove();
    if (this.dialog.open) this.dialog.close();
  }
  private listen(id: string, type: string, listener: EventListener): void {
    const node = element(id);
    node.addEventListener(type, listener);
    this.removers.push(() => node.removeEventListener(type, listener));
  }
}
