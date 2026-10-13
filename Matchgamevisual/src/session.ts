/** 应用层累计时钟，随机算法与浏览器帧率不参与计时。 */
export class SessionClock {
  private accumulated = 0;
  private runningSince: number | null = null;
  constructor(private readonly now: () => number = () => performance.now()) {}
  start(): void {
    this.accumulated = 0;
    this.runningSince = this.now();
  }
  pause(): void {
    if (this.runningSince === null) return;
    this.accumulated += Math.max(0, this.now() - this.runningSince);
    this.runningSince = null;
  }
  resume(): void {
    if (this.runningSince === null) this.runningSince = this.now();
  }
  get elapsedMs(): number {
    return this.accumulated + (this.runningSince === null ? 0 : Math.max(0, this.now() - this.runningSince));
  }
}
