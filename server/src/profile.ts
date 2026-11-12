/** Draft arrival choices. The server owns the selected species and validates every field. */
import { curseById, DEFAULT_PLAYER_CURSE } from "@grudgebound/shared";
import { AuthError } from "./auth";

export const STARTERS = [
  { id: "cinder-grin", personality: "Impatient, bright, and always first into trouble." },
  { id: "weepwick", personality: "Quiet company with a stubborn little flame." },
  { id: "hushmoth", personality: "Listens to everything. Startles almost nothing." },
] as const;
export interface ArrivalProfile {
  version: 1; complete: boolean; legacyStarter: boolean; name: string;
  look: "masculine" | "feminine"; pronouns: "he" | "she" | "they";
  starter: string; tokenId: string; mint: "virtual" | "pending" | "done"; proofUrl: string;
}
export const newProfile = (legacyStarter = false): ArrivalProfile => ({ version: 1, complete: false,
  legacyStarter, name: "", look: "masculine", pronouns: "they", starter: DEFAULT_PLAYER_CURSE,
  tokenId: "starter", mint: "virtual", proofUrl: "" });
export function validProfile(p: ArrivalProfile): boolean {
  return !!p && p.version === 1 && typeof p.complete === "boolean" && typeof p.legacyStarter === "boolean"
    && typeof p.name === "string" && p.name.length <= 24 && ["masculine", "feminine"].includes(p.look)
    && ["he", "she", "they"].includes(p.pronouns) && STARTERS.some(s => s.id === p.starter)
    && typeof p.tokenId === "string" && p.tokenId.length > 0 && ["virtual", "pending", "done"].includes(p.mint)
    && typeof p.proofUrl === "string";
}
export function profileChoice(body: Record<string, unknown>, prior: ArrivalProfile): Pick<ArrivalProfile, "name" | "look" | "pronouns" | "starter"> {
  if (typeof body.name !== "string") throw new AuthError(400, "Choose a name between 2 and 24 letters.");
  const name = body.name.normalize("NFC").trim().replace(/ +/g, " ");
  if (name.length < 2 || name.length > 24 || !/^[\p{L}][\p{L}\p{M} '\-]*$/u.test(name))
    throw new AuthError(400, "Use 2-24 letters, spaces, apostrophes or hyphens for your name.");
  if (body.look !== "masculine" && body.look !== "feminine") throw new AuthError(400, "Choose an exorcist look.");
  if (body.pronouns !== "he" && body.pronouns !== "she" && body.pronouns !== "they") throw new AuthError(400, "Choose he, she or they.");
  if (prior.legacyStarter && body.starter !== undefined && body.starter !== prior.starter)
    throw new AuthError(409, "Your existing partner stays with you.");
  const starter = prior.legacyStarter ? prior.starter : body.starter;
  if (!STARTERS.some(s => s.id === starter)) throw new AuthError(400, "Choose one of Kessa's three partners.");
  return { name, look: body.look, pronouns: body.pronouns, starter: starter as string };
}
export function profileView(p: ArrivalProfile) {
  return { ...p, choices: STARTERS.map(s => { const c = curseById(s.id); return { ...s, name: c.name, emotion: c.emotion, stats: c.stats }; }) };
}
