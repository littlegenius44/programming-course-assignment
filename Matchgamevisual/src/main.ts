import Phaser from 'phaser';
import { BOARD_COLS, BOARD_ROWS } from './core/types';
import { GameController } from './app';
import { BACKGROUND_URL } from './presentation/assets';
import { BoardScene } from './presentation/BoardScene';
import { CELL_SIZE } from './presentation/grid';
import { PageUI } from './ui/page';
import './ui/style.css';

document.body.style.setProperty('--page-background', `url("${BACKGROUND_URL}")`);
let controller: GameController;
const page = new PageUI({
  start: (nickname, matchLength) => controller.start(nickname, matchLength),
  pause: () => controller.pause(), resume: () => controller.resume(),
  restart: () => controller.restart(), quit: () => controller.quit(),
  back: () => controller.backToStart(),
  hint: () => { void controller.requestHint(); },
  playback: options => { controller.setPlayback(options); scene.refreshLayout(); },
});
const scene = new BoardScene({
  onReady: () => page.setReady(),
  onSelection: result => {
    if (result.pair) void controller.move(result.pair.from, result.pair.to);
    else controller.noteSelection(result.selected);
  },
});
controller = new GameController({ renderer: scene, render: model => page.render(model), onError: error => console.error(error) });

const game = new Phaser.Game({
  type: Phaser.WEBGL,
  parent: 'game',
  width: BOARD_COLS * CELL_SIZE,
  height: BOARD_ROWS * CELL_SIZE,
  backgroundColor: '#eceddd',
  scene: [scene],
  scale: {
    mode: Phaser.Scale.FIT,
    autoCenter: Phaser.Scale.CENTER_BOTH,
  },
});

const visibilityChanged = (): void => controller.setHidden(document.hidden);
document.addEventListener('visibilitychange', visibilityChanged);
visibilityChanged();
const timer = window.setInterval(() => controller.tick(), 250);

// 热更新前释放实例和监听，避免重复画布及旧回调残留。
if (import.meta.hot) {
  import.meta.hot.dispose(() => {
    window.clearInterval(timer);
    document.removeEventListener('visibilitychange', visibilityChanged);
    page.dispose();
    controller.dispose();
    game.destroy(true);
  });
}
