/**
 * Minimal client for an OpenAI-compatible chat completions API (NEAR AI Cloud).
 * Configure in .env: NEAR_AI_API_KEY, NEAR_AI_BASE_URL, NEAR_AI_MODEL.
 * maxTokens includes the model's hidden reasoning, so keep it well above the reply length.
 * Set NEAR_AI_DEBUG=1 to log why a call fell back to the scripted policy.
 * Check https://docs.near.ai for the current base URL and model names.
 */
export interface ChatMessage {
  role: "system" | "user" | "assistant";
  content: string;
}

export interface LlmConfig {
  apiKey: string;
  baseUrl: string;
  model: string;
  timeoutMs: number;
}

export function llmConfigFromEnv(): LlmConfig | null {
  const apiKey = process.env.NEAR_AI_API_KEY?.trim();
  const baseUrl = process.env.NEAR_AI_BASE_URL?.trim();
  const model = process.env.NEAR_AI_MODEL?.trim();
  if (!apiKey || !baseUrl || !model) return null;
  return { apiKey, baseUrl: baseUrl.replace(/\/+$/, ""), model, timeoutMs: 8000 };
}

export async function chat(cfg: LlmConfig, messages: ChatMessage[], maxTokens = 600): Promise<string> {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), cfg.timeoutMs);
  try {
    const res = await fetch(`${cfg.baseUrl}/chat/completions`, {
      method: "POST",
      headers: { "Content-Type": "application/json", Authorization: `Bearer ${cfg.apiKey}` },
      body: JSON.stringify({ model: cfg.model, messages, max_tokens: maxTokens, temperature: 0.7 }),
      signal: controller.signal,
    });
    if (!res.ok) throw new Error(`LLM HTTP ${res.status}: ${(await res.text()).slice(0, 200)}`);
    const data = (await res.json()) as { choices?: { message?: { content?: string } }[] };
    const text = data.choices?.[0]?.message?.content;
    if (!text) throw new Error("LLM returned no content");
    return text;
  } finally {
    clearTimeout(timer);
  }
}
