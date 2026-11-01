import GameCanvas from "../components/GameCanvas";

export default function Home() {
  return (
    <main className="page">
      <header className="title">
        <h1>Grudgebound</h1>
        <span>The Rite</span>
      </header>
      <GameCanvas />
      <p className="hint">Arrow keys / WASD to walk. Tall grass hides curses. Space to continue.</p>
    </main>
  );
}
