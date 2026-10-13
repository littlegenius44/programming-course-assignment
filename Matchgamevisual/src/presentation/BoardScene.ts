import Phaser from 'phaser';
import { BOARD_COLS, BOARD_ROWS } from '../core/types';
import type { GameState, GameStep, LegalMove, MatchSegment, Position, TileMove } from '../core/types';
import type { GameRenderer } from '../app';
import { BURST_ASSET, GENERATED_ATLAS, ITEM_ASSETS, VISUAL_EFFECTS } from './assets';
import { Effects } from './Effects';
import { CELL_SIZE, positionAt, selectPosition } from './grid';
import type { SelectionResult } from './grid';
import { timingFor } from './playback';
import type { PlaybackTiming } from './playback';

interface BoardCallbacks {
  readonly onReady: () => void;
  readonly onSelection: (selection: SelectionResult) => void;
}

/** 棋盘只按核心步骤改变物品位置，不判断匹配、得分或关卡。 */
export class BoardScene extends Phaser.Scene implements GameRenderer {
  private state: GameState | null = null;
  private enabled = false;
  private selected: Position | null = null;
  private readonly items = new Map<number, Phaser.GameObjects.Image>();
  private selectionOutline!: Phaser.GameObjects.Graphics;
  private matchOutline!: Phaser.GameObjects.Graphics;
  private hintOutline!: Phaser.GameObjects.Graphics;
  private matchedSegments: ReadonlyArray<MatchSegment> = [];
  private effects!: Effects;

  constructor(private readonly callbacks: BoardCallbacks) {
    super({ key: 'BoardScene' });
  }

  preload(): void {
    this.load.image(GENERATED_ATLAS.key, GENERATED_ATLAS.url);
    this.load.image(BURST_ASSET.key, BURST_ASSET.url);
    for (const item of ITEM_ASSETS) {
      if (item.url) this.load.image(item.textureKey, item.url);
    }
  }

  create(): void {
    this.prepareTextures();
    this.effects = new Effects(this);
    this.selectionOutline = this.add.graphics().setDepth(10);
    this.matchOutline = this.add.graphics().setDepth(11);
    this.hintOutline = this.add.graphics().setDepth(9);
    this.game.canvas.setAttribute('aria-label', '20行30列棋盘，鼠标点击选择物品');
    this.input.on('pointerdown', this.handlePointer, this);
    // 场景停止或整个游戏销毁时，都清除自有引用与监听。
    const cleanup = (): void => {
      this.input.off('pointerdown', this.handlePointer, this);
      this.items.clear();
      this.effects.clear();
      this.matchedSegments = [];
      this.selected = null;
      this.hintOutline.clear();
      this.events.off(Phaser.Scenes.Events.SHUTDOWN, cleanup);
      this.events.off(Phaser.Scenes.Events.DESTROY, cleanup);
    };
    this.events.once(Phaser.Scenes.Events.SHUTDOWN, cleanup);
    this.events.once(Phaser.Scenes.Events.DESTROY, cleanup);
    this.callbacks.onReady();
  }

  /** 新局、换关或故障恢复时，根据明确的快照重新建立棋盘。 */
  showState(state: GameState): void {
    this.effects.clear();
    this.matchedSegments = [];
    for (const item of this.items.values()) item.destroy();
    this.items.clear();
    this.clearSelection();
    this.hintOutline.clear();
    this.matchOutline.clear();
    this.state = state;
    for (let row = 0; row < BOARD_ROWS; row++) {
      for (let col = 0; col < BOARD_COLS; col++) {
        const tile = state.board[row][col];
        if (!tile) continue;
        const image = this.add.image((col + 0.5) * CELL_SIZE, (row + 0.5) * CELL_SIZE, `tile-${tile.kind}`)
          .setDisplaySize(CELL_SIZE - 2, CELL_SIZE - 2);
        this.items.set(tile.id, image);
      }
    }
    this.refreshLayout();
    this.assertSnapshot(state);
  }

  setInputEnabled(enabled: boolean): void {
    if (this.enabled === enabled) return;
    this.enabled = enabled;
    if (!enabled) this.showHint(null);
  }

  /** 提示只画两格，不交换或启动动画；先清除半次选择。 */
  showHint(move: LegalMove | null): void {
    this.clearSelection();
    this.hintOutline?.clear();
    if (!move) return;
    this.hintOutline.lineStyle(2.5, 0x337cad, 1);
    for (const cell of [move.from, move.to]) {
      this.hintOutline.strokeRoundedRect(cell.col * CELL_SIZE + 1.5, cell.row * CELL_SIZE + 1.5, CELL_SIZE - 3, CELL_SIZE - 3, 5);
    }
  }

  setPaused(paused: boolean): void {
    if (paused) this.tweens?.pauseAll();
    else this.tweens?.resumeAll();
  }

  refreshLayout(): void {
    // refresh先按缓存尺寸缩放；页面展开或说明栏改变尺寸时先读取父容器。
    this.scale.getParentBounds();
    this.scale.refresh();
  }

  /** 等待本步全部Tween完成，再检查物品身份及格子坐标与after一致。 */
  async playStep(step: GameStep, signal: AbortSignal,
    timing: PlaybackTiming = timingFor({ demo: false, speed: 1 })): Promise<void> {
    this.checkAbort(signal);
    this.showHint(null);
    const animate = (changes: Phaser.Types.Tweens.TweenBuilderConfig[], duration: number): Promise<void> =>
      this.animate(changes, duration * timing.scale, signal);
    switch (step.kind) {
      case 'SWAP':
      case 'SWAP_BACK':
        await this.moveItems(step.moves, 160, signal, timing.scale);
        break;
      case 'MATCH':
        this.matchedSegments = step.matches.segments;
        this.matchOutline.clear().lineStyle(2, 0xc36c23, 1);
        for (const cell of step.matches.cells) {
          this.matchOutline.strokeRoundedRect(cell.col * CELL_SIZE + 1, cell.row * CELL_SIZE + 1, CELL_SIZE - 2, CELL_SIZE - 2, 5);
        }
        await animate([], step.matches.cells.length ? timing.matchMs : timing.noMatchMs);
        break;
      case 'CLEAR': {
        const changes = step.removed.map(({ tile }) => {
          const image = this.requireItem(tile.id);
          return { targets: image, alpha: 0, scaleX: image.scaleX * 0.5, scaleY: image.scaleY * 0.5 };
        });
        const batch = this.effects.createClear(step, this.matchedSegments);
        try {
          await animate([...changes, ...batch.changes], step.round === 1 ? 170 : 230);
        } finally { batch.dispose(); }
        this.checkAbort(signal);
        for (const { tile } of step.removed) { this.requireItem(tile.id).destroy(); this.items.delete(tile.id); }
        this.matchOutline.clear();
        if (timing.clearHoldMs) await animate([], timing.clearHoldMs);
        break;
      }
      case 'GRAVITY':
        await this.moveItems(step.moves, step.moves.length ? 220 : 60, signal, timing.scale);
        await this.landItems(step.moves.map(move => move.id), signal, timing.scale);
        break;
      case 'REFILL': {
        const counts = new Map<number, number>();
        for (const { position } of step.spawned) counts.set(position.col, (counts.get(position.col) ?? 0) + 1);
        const changes = step.spawned.map(({ tile, position }) => {
          if (this.items.has(tile.id)) throw new Error(`补充编号重复：${tile.id}`);
          const image = this.add.image((position.col + 0.5) * CELL_SIZE,
            (position.row - counts.get(position.col)! + 0.5) * CELL_SIZE, `tile-${tile.kind}`)
            .setDisplaySize(CELL_SIZE - 2, CELL_SIZE - 2);
          this.items.set(tile.id, image);
          return { targets: image, y: (position.row + 0.5) * CELL_SIZE };
        });
        await animate(changes, 240);
        await this.landItems(step.spawned.map(item => item.tile.id), signal, timing.scale);
        break;
      }
      case 'LEVEL_CHANGE':
        this.showState(step.after);
        await animate([], 140);
        break;
      case 'SHUFFLE':
        await animate([], timing.shuffleNoticeMs);
        await animate([...this.items.values()].map(image => ({ targets: image, alpha: 0 })), 140);
        this.checkAbort(signal);
        this.showState(step.after);
        for (const image of this.items.values()) image.setAlpha(0);
        await animate([...this.items.values()].map(image => ({ targets: image, alpha: 1 })), 140);
        break;
      case 'STABLE':
      case 'WIN':
        this.matchOutline.clear();
        await animate([], 50);
        break;
    }
    this.checkAbort(signal);
    this.assertSnapshot(step.after);
    this.state = step.after;
  }

  private moveItems(moves: ReadonlyArray<TileMove>, duration: number, signal: AbortSignal, scale: number): Promise<void> {
    return this.animate(moves.map(move => ({ targets: this.requireItem(move.id),
      x: (move.to.col + 0.5) * CELL_SIZE, y: (move.to.row + 0.5) * CELL_SIZE })), duration * scale, signal);
  }

  /** 只有真实移动/补充的物品有落地反馈，yoyo恢复原始比例与终点。 */
  private async landItems(ids: ReadonlyArray<number>, signal: AbortSignal, scale: number): Promise<void> {
    this.checkAbort(signal);
    if (!VISUAL_EFFECTS.enabled || !ids.length) return;
    await this.animate(ids.map(id => {
      const image = this.requireItem(id);
      return { targets: image, scaleX: image.scaleX * 1.06, scaleY: image.scaleY * 0.92, yoyo: true };
    }), 55 * scale, signal);
  }

  /** 使用Tween完成信号，包括无位移步骤的短暂等待；取消时移除全部相关Tween。 */
  private animate(changes: Phaser.Types.Tweens.TweenBuilderConfig[], duration: number, signal: AbortSignal): Promise<void> {
    this.checkAbort(signal);
    const configs = changes.length ? changes : [{ targets: { progress: 0 }, progress: 1 }];
    return new Promise((resolve, reject) => {
      const tweens: Phaser.Tweens.Tween[] = [];
      let remaining = configs.length;
      const abort = (): void => {
        for (const tween of tweens) { tween.stop(); this.tweens.remove(tween); }
        signal.removeEventListener('abort', abort);
        reject(new DOMException('播放已取消', 'AbortError'));
      };
      signal.addEventListener('abort', abort, { once: true });
      for (const config of configs) {
        tweens.push(this.tweens.add({ ...config, duration, ease: 'Sine.easeInOut', onComplete: () => {
          remaining--;
          if (!remaining) { signal.removeEventListener('abort', abort); resolve(); }
        } }));
      }
    });
  }

  private requireItem(id: number): Phaser.GameObjects.Image {
    const image = this.items.get(id);
    if (!image) throw new Error(`缺少物品编号：${id}`);
    return image;
  }

  private assertSnapshot(state: GameState): void {
    let count = 0;
    for (let row = 0; row < BOARD_ROWS; row++) for (let col = 0; col < BOARD_COLS; col++) {
      const tile = state.board[row][col];
      if (!tile) continue;
      count++;
      const image = this.requireItem(tile.id);
      if (image.texture.key !== `tile-${tile.kind}` || Math.abs(image.x - (col + 0.5) * CELL_SIZE) > 0.01 ||
          Math.abs(image.y - (row + 0.5) * CELL_SIZE) > 0.01 || image.alpha !== 1 ||
          Math.abs(image.displayWidth - (CELL_SIZE - 2)) > 0.01 || Math.abs(image.displayHeight - (CELL_SIZE - 2)) > 0.01) {
        throw new Error(`动画与快照不一致：物品${tile.id}，位置${row},${col}`);
      }
    }
    if (this.items.size !== count) throw new Error('动画物品数量与快照不一致。');
  }

  private checkAbort(signal: AbortSignal): void {
    if (signal.aborted) throw new DOMException('播放已取消', 'AbortError');
  }

  private clearSelection(): void {
    this.selected = null;
    this.selectionOutline?.clear();
  }

  private handlePointer(pointer: Phaser.Input.Pointer): void {
    if (!this.enabled || !this.state) return;
    this.hintOutline.clear();
    const position = positionAt(pointer.x, pointer.y);
    if (!position) return;
    const result = selectPosition(this.selected, position);
    this.selected = result.selected;
    this.selectionOutline.clear();
    const selectedCells = result.pair ? [result.pair.from, result.pair.to] : result.selected ? [result.selected] : [];
    this.selectionOutline.lineStyle(2, result.pair ? 0xb87720 : 0x21583c, 1);
    for (const cell of selectedCells) {
      this.selectionOutline.strokeRoundedRect(cell.col * CELL_SIZE + 1.5, cell.row * CELL_SIZE + 1.5, CELL_SIZE - 3, CELL_SIZE - 3, 5);
    }
    this.callbacks.onSelection(result);
  }

  /** 将背景、物品与数字合成九张可复用纹理，避免600个独立文字对象。 */
  private prepareTextures(): void {
    if (this.textures.exists(GENERATED_ATLAS.key)) {
      const atlas = this.textures.get(GENERATED_ATLAS.key);
      GENERATED_ATLAS.frames.forEach((frame, index) => {
        atlas.add(`fruit-${index + 1}`, 0, frame.x, frame.y, frame.width, frame.height);
      });
    }

    for (const item of ITEM_ASSETS) {
      const canvas = document.createElement('canvas');
      canvas.width = canvas.height = 96;
      const context = canvas.getContext('2d');
      if (!context) throw new Error('无法创建物品纹理。');
      context.fillStyle = '#fffdf4';
      context.beginPath();
      context.roundRect(0, 0, 96, 96, 14);
      context.fill();
      context.imageSmoothingQuality = 'high';
      if (this.textures.exists(item.textureKey) && (!item.frame || this.textures.get(item.textureKey).has(item.frame))) {
        const frame = this.textures.get(item.textureKey).get(item.frame);
        const scale = Math.min(76 / frame.cutWidth, 76 / frame.cutHeight);
        const width = frame.cutWidth * scale;
        const height = frame.cutHeight * scale;
        context.drawImage(frame.source.image as CanvasImageSource, frame.cutX, frame.cutY, frame.cutWidth, frame.cutHeight,
          (96 - width) / 2, (84 - height) / 2, width, height);
      } else {
        // 图片缺失时保留数字色块，核心棋盘仍可显示。
        context.fillStyle = item.fallbackColor;
        context.beginPath();
        context.arc(46, 42, 25, 0, Math.PI * 2);
        context.fill();
      }
      context.fillStyle = '#fffdf4';
      context.beginPath();
      context.arc(79, 80, 14, 0, Math.PI * 2);
      context.fill();
      context.fillStyle = '#284b37';
      context.font = '600 30px Segoe UI, sans-serif';
      context.textAlign = 'center';
      context.textBaseline = 'middle';
      context.fillText(String(item.kind), 79, 80);
      this.textures.addCanvas(`tile-${item.kind}`, canvas);
    }
  }
}
