import { describe, expect, it } from 'vitest';
import { SessionClock } from '../src/session';

describe('整局累计时钟', () => {
  it('正常等待和动画期间持续累计，主动暂停排除用时', () => {
    let now = 0;
    const clock = new SessionClock(() => now);
    clock.start();
    now = 1000;
    expect(clock.elapsedMs).toBe(1000);
    clock.pause();
    now = 6000;
    expect(clock.elapsedMs).toBe(1000);
    clock.resume();
    now = 6500;
    expect(clock.elapsedMs).toBe(1500);
  });

  it('重复暂停与继续不重复累计，重开从零开始', () => {
    let now = 100;
    const clock = new SessionClock(() => now);
    clock.start();
    now = 300;
    clock.pause();
    clock.pause();
    now = 900;
    clock.resume();
    clock.resume();
    now = 1000;
    expect(clock.elapsedMs).toBe(300);
    clock.start();
    expect(clock.elapsedMs).toBe(0);
    now = 1200;
    expect(clock.elapsedMs).toBe(200);
  });

  it('停止后用时冻结，未启动时为零', () => {
    let now = 0;
    const clock = new SessionClock(() => now);
    expect(clock.elapsedMs).toBe(0);
    clock.start();
    now = 2500;
    clock.pause();
    now = 9000;
    expect(clock.elapsedMs).toBe(2500);
  });
});
