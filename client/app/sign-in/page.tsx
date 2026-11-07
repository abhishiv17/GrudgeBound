"use client";

import { useEffect, useState, type FormEvent } from "react";
import "./sign-in.css";

export default function SignInPage() {
  const [email, setEmail] = useState("");
  const [code, setCode] = useState("");
  const [challenge, setChallenge] = useState("");
  const [device, setDevice] = useState<string | null>(null);
  const [error, setError] = useState("");
  const [busy, setBusy] = useState(false);
  const [done, setDone] = useState(false);
  useEffect(() => { setDevice(new URLSearchParams(window.location.search).get("device")); }, []);

  async function submit(event: FormEvent) {
    event.preventDefault(); setBusy(true); setError("");
    try {
      const response = await fetch(`/api/game/auth/email/${challenge ? "verify" : "start"}`, {
        method: "POST", headers: { "Content-Type": "application/json" },
        body: JSON.stringify(challenge ? { challengeId: challenge, code } : { email, ...(device ? { device } : {}) }),
      });
      const result = await response.json();
      if (!response.ok) throw new Error(result.error || "Sign-in failed. Please try again.");
      if (!challenge) setChallenge(result.challengeId);
      else setDone(true);
    } catch (e) { setError(e instanceof Error ? e.message : "Sign-in failed."); }
    finally { setBusy(false); }
  }

  return (
    <main className="sign-in">
      <section className="sign-in__paper" aria-labelledby="sign-in-title">
        <p className="sign-in__eyebrow">Grudgebound · The Rite</p>
        <h1 id="sign-in-title">{done ? "Your place is ready." : "Enter Hollowmere"}</h1>
        {done ? <>
          <p>{device ? "Return to the game. Kessa is waiting for you." : "The lanterns are lit. Your journey can begin."}</p>
          {!device && <a className="btn btn--primary" href="/">Enter Lantern Ward</a>}
        </> : <>
          <p>{device ? "Continue only if you just opened sign-in from your game." : "A place in the ledger, kept for your return."}</p>
          <form onSubmit={submit}>
            {!challenge ? <>
              <label htmlFor="email">Your email</label>
              <input id="email" type="email" autoComplete="email" maxLength={254} required value={email} onChange={(e) => setEmail(e.target.value)} disabled={busy} autoFocus />
              <p>We’ll send a six-digit code to let you in.</p>
            </> : <>
              <label htmlFor="code">Code sent to {email}</label>
              <input id="code" type="text" inputMode="numeric" autoComplete="one-time-code" pattern="[0-9]{6}" maxLength={6} required value={code} onChange={(e) => setCode(e.target.value.replace(/\D/g, ""))} disabled={busy} autoFocus />
              <p>The code lasts ten minutes.</p>
            </>}
            <p role="alert" className="sign-in__error">{error}</p>
            <button className="btn btn--primary" type="submit" disabled={busy}>{busy ? "One moment…" : challenge ? device ? "Sign in to my game" : "Sign in" : "Send my code"}</button>
            {challenge && <button className="btn btn--paper" type="button" disabled={busy} onClick={() => { setChallenge(""); setCode(""); setError(""); }}>Request a new code</button>}
          </form>
        </>}
      </section>
    </main>
  );
}
