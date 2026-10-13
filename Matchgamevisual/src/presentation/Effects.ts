import type Phaser from 'phaser';
import type { GameStep, MatchSegment } from '../core/types';
import { BURST_ASSET, VISUAL_EFFECTS } from './assets';
import { planClearEffects } from './effectPlan';
import { CELL_SIZE } from './grid';

const RING_TEXTURE = 'clear-ring';
export interface EffectBatch {
  readonly changes: Phaser.Types.Tweens.TweenBuilderConfig[];
  dispose(): void;
}

/** 只管理装饰对象；所有Tween交给棋盘的步骤等待、暂停和取消流程。 */
export class Effects {
  private readonly active = new Set<Phaser.GameObjects.Image>();

  constructor(private readonly scene: Phaser.Scene) {
    if (!scene.textures.exists(RING_TEXTURE)) {
      const canvas = document.createElement('canvas');
      canvas.width = canvas.height = 128;
      const context = canvas.getContext('2d');
      if (!context) throw new Error('无法创建消除反馈纹理。');
      context.strokeStyle = '#edb353';
      context.lineWidth = 6;
      context.beginPath();
      context.arc(64, 64, 48, 0, Math.PI * 2);
      context.stroke();
      scene.textures.addCanvas(RING_TEXTURE, canvas);
    }
  }

  createClear(clear: Extract<GameStep, { kind: 'CLEAR' }>, segments: ReadonlyArray<MatchSegment>,
    enabled = VISUAL_EFFECTS.enabled): EffectBatch {
    const plan = planClearEffects(clear, segments, enabled);
    const texture = plan.style === 'burst' && this.scene.textures.exists(BURST_ASSET.key) ? BURST_ASSET.key : RING_TEXTURE;
    const objects = plan.centers.map(position => {
      const image = this.scene.add.image((position.col + 0.5) * CELL_SIZE, (position.row + 0.5) * CELL_SIZE, texture)
        .setDepth(12).setDisplaySize(plan.diameter, plan.diameter).setAlpha(plan.style === 'ring' ? 0.65 : 0.85);
      this.active.add(image);
      return image;
    });
    return {
      changes: objects.map(image => ({ targets: image, alpha: 0, scaleX: image.scaleX * 1.35, scaleY: image.scaleY * 1.35 })),
      // 仅释放本批对象，旧Promise的finally不会清理新会话的效果。
      dispose: () => { for (const image of objects) this.release(image); },
    };
  }

  clear(): void { for (const image of [...this.active]) this.release(image); }
  private release(image: Phaser.GameObjects.Image): void {
    if (this.active.delete(image)) image.destroy();
  }
}
