export type PlaybackSpeed = 1 | 0.5 | 0.25;
export interface PlaybackOptions { readonly demo: boolean; readonly speed: PlaybackSpeed }
export interface PlaybackTiming {
  readonly scale: number;
  readonly matchMs: number;
  readonly noMatchMs: number;
  readonly clearHoldMs: number;
  readonly shuffleNoticeMs: number;
}

/** 时长属于表现层，不能进入规则或得分计算。 */
export function timingFor(options: PlaybackOptions): PlaybackTiming {
  if (typeof options.demo !== 'boolean' || ![1, 0.5, 0.25].includes(options.speed)) {
    throw new RangeError('演示速度只能是1、0.5或0.25。');
  }
  return Object.freeze(options.demo
    ? { scale: 1 / options.speed, matchMs: 650, noMatchMs: 300, clearHoldMs: 450, shuffleNoticeMs: 1000 }
    : { scale: 1, matchMs: 160, noMatchMs: 70, clearHoldMs: 0, shuffleNoticeMs: 500 });
}
