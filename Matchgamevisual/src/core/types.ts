export const BOARD_ROWS = 20;
export const BOARD_COLS = 30;
export const ITEM_TYPES = 9;
export const MAX_CHAIN_ROUNDS = 100;

export type MatchLength = 3 | 4;
export type ItemKind = 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9;
export type RandomSource = () => number;

/** 所有算法坐标从 0 开始，与像素坐标无关。 */
export interface Position {
  readonly row: number;
  readonly col: number;
}

export interface LegalMove {
  readonly from: Position;
  readonly to: Position;
}

/** 同一物品在交换、下落过程中保留编号。 */
export interface Tile {
  readonly id: number;
  readonly kind: ItemKind;
}

export type Board = ReadonlyArray<ReadonlyArray<Tile | null>>;

export interface GameState {
  readonly board: Board;
  readonly nickname: string;
  readonly matchLength: MatchLength;
  readonly score: number;
  readonly level: 1 | 2 | 3;
  readonly status: 'playing' | 'won';
  readonly nextTileId: number;
}

export interface MatchSegment {
  readonly direction: 'horizontal' | 'vertical';
  readonly kind: ItemKind;
  readonly cells: ReadonlyArray<Position>;
}

export interface MatchResult {
  readonly segments: ReadonlyArray<MatchSegment>;
  readonly cells: ReadonlyArray<Position>;
}

export interface TileMove {
  readonly id: number;
  readonly from: Position;
  readonly to: Position;
}

export interface LocatedTile {
  readonly tile: Tile;
  readonly position: Position;
}

export type StepDetails =
  | { readonly kind: 'SWAP' | 'SWAP_BACK'; readonly moves: ReadonlyArray<TileMove> }
  | { readonly kind: 'MATCH'; readonly round: number; readonly matches: MatchResult }
  | { readonly kind: 'CLEAR'; readonly round: number; readonly removed: ReadonlyArray<LocatedTile>;
      readonly count: number; readonly scorePerTile: 10 | 20 | 30; readonly scoreDelta: number }
  | { readonly kind: 'GRAVITY'; readonly moves: ReadonlyArray<TileMove> }
  | { readonly kind: 'REFILL'; readonly spawned: ReadonlyArray<LocatedTile> }
  | { readonly kind: 'SHUFFLE'; readonly reason: 'no-legal-move' }
  | { readonly kind: 'STABLE' }
  | { readonly kind: 'LEVEL_CHANGE'; readonly fromLevel: 1 | 2 | 3; readonly toLevel: 1 | 2 | 3 }
  | { readonly kind: 'WIN' };

/** after 是本步骤完成后的状态，不包含动画时长或 Phaser 对象。 */
export type GameStep = StepDetails & {
  readonly index: number;
  readonly after: GameState;
};

export interface MoveResolution {
  readonly outcome: 'rejected' | 'invalid' | 'valid';
  readonly reason: 'game-over' | 'invalid-position' | 'not-adjacent' | 'empty-cell' | null;
  readonly steps: ReadonlyArray<GameStep>;
  readonly finalState: GameState;
}
