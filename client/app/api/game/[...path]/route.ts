/** Same-origin gateway: the browser session stays in an HttpOnly cookie. */
export const runtime = "nodejs";
export const dynamic = "force-dynamic";

async function forward(request: Request, context: { params: Promise<{ path: string[] }> }) {
  const { path } = await context.params;
  if (!path.length || path.some((p) => !/^[a-zA-Z0-9_-]+$/.test(p))) return Response.json({ error: "Not found" }, { status: 404 });
  const incoming = new URL(request.url);
  if (request.method === "POST") {
    if (request.headers.get("origin") !== incoming.origin) return Response.json({ error: "Invalid request origin" }, { status: 403 });
    if (!request.headers.get("content-type")?.startsWith("application/json")) return Response.json({ error: "Expected JSON" }, { status: 415 });
  }
  const base = process.env.GAME_SERVER_URL || "http://localhost:8787";
  const url = new URL(`${base.replace(/\/$/, "")}/${path.join("/")}`);
  url.search = incoming.search;
  const headers = new Headers({ "Content-Type": "application/json", Origin: incoming.origin });
  const cookie = request.headers.get("cookie");
  if (cookie) headers.set("Cookie", cookie);
  // Never accept a client-supplied server URL, Authorization header, or proxy address.
  try {
    const body = request.method === "POST" ? await request.text() : undefined;
    if (body && Buffer.byteLength(body) > 16 * 1024) return Response.json({ error: "Request too large" }, { status: 413 });
    const upstream = await fetch(url, { method: request.method, headers, body, cache: "no-store", redirect: "error", signal: AbortSignal.timeout(60_000) });
    const responseHeaders = new Headers({ "Content-Type": "application/json", "Cache-Control": "no-store" });
    const setCookie = upstream.headers.get("set-cookie");
    if (setCookie) responseHeaders.set("Set-Cookie", setCookie);
    return new Response(await upstream.text(), { status: upstream.status, headers: responseHeaders });
  } catch {
    return Response.json({ error: "Hollowmere is unreachable. Please try again." }, { status: 502 });
  }
}
export const GET = forward;
export const POST = forward;
