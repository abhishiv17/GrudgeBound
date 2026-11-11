// Stands in for Phaser when export-battle-art.ts bundles the web client's art code for Node:
// the painters only need the class they extend to exist.
export const GameObjects = { Container: class {} };
export default { GameObjects };
