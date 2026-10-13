import { describe, expect, it } from 'vitest';
import type Phaser from 'phaser';
import type { GameStep, MatchSegment } from '../src/core/types';
import { createGame } from '../src/core/game';
import { Effects } from '../src/presentation/Effects';

/** 只替换无法在Node运行的Phaser绘制接口；实际效果批次管理由Effects执行。 */
class TestImage {
  scaleX = 1;
  scaleY = 1;
  destroyed = 0;
  setDepth(): this { return this; }
  setDisplaySize(size: number): this { this.scaleX = this.scaleY = size / 128; return this; }
  setAlpha(): this { return this; }
  destroy(): void { this.destroyed++; }
}

function setup(): { effects: Effects; images: TestImage[] } {
  const images: TestImage[] = [];
  const scene = { textures: { exists: () => true }, add: { image: () => {
    const image = new TestImage(); images.push(image); return image;
  } } } as unknown as Phaser.Scene;
  return { effects: new Effects(scene), images };
}

const positions = [{ row: 0, col: 0 }, { row: 0, col: 1 }, { row: 0, col: 2 }];
const clear: Extract<GameStep, { kind: 'CLEAR' }> = {
  kind: 'CLEAR', index: 2, after: createGame('批次测试', 3, () => 0), round: 2, count: 3, scorePerTile: 20, scoreDelta: 60,
  removed: positions.map((position, index) => ({ position, tile: { id: index + 1, kind: 1 } })),
};
const segments: MatchSegment[] = [{ direction: 'horizontal', kind: 1, cells: positions }];

describe('装饰对象的批次生命周期', () => {
  it('批次释放可重复调用，每个对象只销毁一次', () => {
    const { effects, images } = setup();
    const batch = effects.createClear(clear, segments);
    expect(batch.changes).toHaveLength(1);
    batch.dispose();
    batch.dispose();
    effects.clear();
    expect(images.map(image => image.destroyed)).toEqual([1]);
  });

  it('旧回调释放不会销毁新局产生的效果对象', () => {
    const { effects, images } = setup();
    const old = effects.createClear(clear, segments);
    effects.clear();
    const current = effects.createClear(clear, segments);
    old.dispose();
    expect(images.map(image => image.destroyed)).toEqual([1, 0]);
    current.dispose();
    expect(images.map(image => image.destroyed)).toEqual([1, 1]);
  });

  it('禁用时不创建装饰对象，仍可正常清理', () => {
    const { effects, images } = setup();
    const batch = effects.createClear(clear, segments, false);
    expect(batch.changes).toEqual([]);
    expect(images).toEqual([]);
    batch.dispose();
    effects.clear();
  });
});
