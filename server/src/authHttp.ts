import type { IncomingMessage, ServerResponse } from "node:http";
import { AuthError, AuthService, SESSION_MS } from "./auth";

export function sessionToken(req: IncomingMessage): string | undefined {
  if (req.headers.authorization) {
    const match = /^Bearer ([A-Za-z0-9_-]{43})$/.exec(req.headers.authorization);
    if (!match) throw new AuthError(401, "Invalid sign-in session.");
    return match[1];
  }
  return /(?:^|;\s*)gb_session=([A-Za-z0-9_-]{43})(?:;|$)/.exec(req.headers.cookie ?? "")?.[1];
}

export function cookie(token: string, production: boolean, clear = false): string {
  return `gb_session=${token}; HttpOnly; SameSite=Strict; Path=/; Max-Age=${clear ? 0 : SESSION_MS / 1000}${production ? "; Secure" : ""}`;
}

/** Kept independent of the world so authentication can be exercised over HTTP in tests. */
export async function authRoute(
  auth: AuthService, req: IncomingMessage, res: ServerResponse, path: string,
  read: () => Promise<unknown>, send: (status: number, body: unknown) => void,
  signInUrl: string, production: boolean,
): Promise<void> {
  res.setHeader("Cache-Control", "no-store");
  const ip = req.socket.remoteAddress ?? "unknown";
  if (req.method === "GET" && path === "/auth/session") {
    send(200, { player: auth.authenticate(sessionToken(req)) }); return;
  }
  if (req.method !== "POST") throw new AuthError(404, "Not found.");
  if (!(req.headers["content-type"] ?? "").startsWith("application/json")) throw new AuthError(415, "Expected application/json.");
  const raw = await read();
  if (!raw || typeof raw !== "object" || Array.isArray(raw)) throw new AuthError(400, "Expected a JSON object.");
  const b = raw as Record<string, unknown>;
  switch (path) {
    case "/auth/device/start": send(200, auth.startDevice(ip, signInUrl)); return;
    case "/auth/device/poll": send(200, auth.pollDevice(b.deviceCode)); return;
    case "/auth/email/start": send(200, await auth.startEmail(b.email, ip, b.device)); return;
    case "/auth/email/verify": {
      const result = await auth.verify(b.challengeId, b.code, ip);
      if (result.approved) { send(200, result); return; }
      res.setHeader("Set-Cookie", cookie(result.token, production));
      send(200, { player: result.player, expiresAt: result.expiresAt }); return;
    }
    case "/auth/logout":
      auth.logout(sessionToken(req));
      res.setHeader("Set-Cookie", cookie("", production, true));
      send(200, { ok: true }); return;
    default: throw new AuthError(404, "Not found.");
  }
}
