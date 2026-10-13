import type { ItemKind } from '../core/types';

/** 所有可替换图片集中在此配置，路径适配Vite的部署基础目录。 */
const assetRoot = `${import.meta.env.BASE_URL}assets/`;
export const BACKGROUND_URL = `${assetRoot}backgrounds/orchard.png`;
export const BURST_ASSET = {
  key: 'clear-burst',
  url: `${assetRoot}effects/burst.png`,
};
/** 只控制装饰；关闭后仍播放完整的算法步骤与物品位移。 */
export const VISUAL_EFFECTS = { enabled: true };
export const GENERATED_ATLAS = {
  key: 'generated-fruits',
  url: `${assetRoot}items/fruits.png`,
  // 生成图集的物品略偏离等分网格，使用显式切片避免截断轮廓。
  frames: [
    { x: 20, y: 30, width: 280, height: 310 },
    { x: 322, y: 60, width: 305, height: 274 },
    { x: 654, y: 39, width: 284, height: 293 },
    { x: 976, y: 28, width: 264, height: 322 },
    { x: 15, y: 343, width: 300, height: 341 },
    { x: 327, y: 382, width: 300, height: 292 },
    { x: 645, y: 358, width: 277, height: 326 },
    { x: 940, y: 386, width: 300, height: 293 },
    { x: 17, y: 697, width: 299, height: 300 },
  ],
};

export interface ItemAsset {
  readonly kind: ItemKind;
  readonly textureKey: string;
  readonly frame?: string;
  readonly url?: string;
  readonly fallbackColor: string;
}

// 后续可给单个条目改为独立textureKey与url，并去掉frame，接入用户图片。
const colors = ['#dc5b53', '#ee9b3c', '#efca50', '#78a956', '#9168b1', '#5c8fbd', '#d95d76', '#e9a58d', '#90b65f'];
export const ITEM_ASSETS: ReadonlyArray<ItemAsset> = colors.map((fallbackColor, index) => ({
  kind: (index + 1) as ItemKind,
  textureKey: GENERATED_ATLAS.key,
  frame: `fruit-${index + 1}`,
  fallbackColor,
}));
