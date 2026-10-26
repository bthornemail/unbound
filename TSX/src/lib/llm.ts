import Anthropic from '@anthropic-ai/sdk';

export type Provider = 'ollama' | 'openai' | 'anthropic' | 'opencode' | 'websocket';

export interface ProviderConfig {
  baseUrl: string;
  model: string;
  apiKey: string;
}

export interface ChatMessage {
  role: 'user' | 'assistant';
  content: string;
}

export interface ChatRequest {
  provider: Provider;
  config: ProviderConfig;
  system: string;
  messages: ChatMessage[];
  signal?: AbortSignal;
  /** Per-conversation state for stateful backends (OpenCode keeps history server-side). */
  session: ChatSession;
}

export interface ChatSession {
  opencodeSessionId?: string;
}

export const PROVIDERS: Record<Provider, { label: string; needsKey: boolean; hint: string; defaults: ProviderConfig }> = {
  ollama: {
    label: 'Ollama',
    needsKey: false,
    hint: 'Local Ollama server. Run `ollama serve`; localhost origins are allowed by default.',
    defaults: { baseUrl: 'http://localhost:11434', model: 'llama3.2', apiKey: '' },
  },
  openai: {
    label: 'OpenAI-compatible',
    needsKey: true,
    hint: 'Any /v1/chat/completions server: OpenAI, OpenRouter, LM Studio, llama.cpp, vLLM, Ollama’s /v1.',
    defaults: { baseUrl: 'https://api.openai.com/v1', model: '', apiKey: '' },
  },
  anthropic: {
    label: 'Anthropic (Claude)',
    needsKey: true,
    hint: 'Calls the Claude API straight from the browser, so the key is visible to anyone using this page. Use it locally only.',
    defaults: { baseUrl: 'https://api.anthropic.com', model: 'claude-opus-5-5', apiKey: '' },
  },
  opencode: {
    label: 'OpenCode',
    needsKey: false,
    hint: 'Run `opencode serve --port 4096 --cors http://localhost:5173`. Model is provider/model, or blank for OpenCode’s default.',
    defaults: { baseUrl: 'http://localhost:4096', model: '', apiKey: '' },
  },
  websocket: {
    label: 'WebSocket relay (Emacs agent)',
    needsKey: false,
    hint: 'Sends {command: "complete", args} and reads {response | error} back.',
    defaults: { baseUrl: 'ws://localhost:9876', model: '', apiKey: '' },
  },
};

const trimSlash = (url: string) => url.replace(/\/+$/, '');

async function ensureOk(res: Response): Promise<Response> {
  if (!res.ok) {
    const body = await res.text().catch(() => '');
    throw new Error(`${res.status} ${res.statusText}${body ? `: ${body.slice(0, 300)}` : ''}`);
  }
  return res;
}

async function* readLines(res: Response): AsyncGenerator<string> {
  if (!res.body) return;
  const reader = res.body.pipeThrough(new TextDecoderStream()).getReader();
  let buffer = '';
  for (;;) {
    const { value, done } = await reader.read();
    if (done) break;
    buffer += value;
    const lines = buffer.split(/\r?\n/);
    buffer = lines.pop() ?? '';
    yield* lines;
  }
  if (buffer) yield buffer;
}

const anthropicClient = ({ apiKey, baseUrl }: ProviderConfig) =>
  new Anthropic({ apiKey, baseURL: trimSlash(baseUrl) || undefined, dangerouslyAllowBrowser: true });

/** Streams the assistant's reply as text chunks. */
export async function* streamChat(req: ChatRequest): AsyncGenerator<string> {
  const { config, system, messages, signal } = req;
  const base = trimSlash(config.baseUrl);

  switch (req.provider) {
    case 'ollama': {
      const res = await ensureOk(
        await fetch(`${base}/api/chat`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            model: config.model,
            stream: true,
            messages: system ? [{ role: 'system', content: system }, ...messages] : messages,
          }),
          signal,
        }),
      );
      for await (const line of readLines(res)) {
        if (!line.trim()) continue;
        const chunk = JSON.parse(line);
        if (chunk.error) throw new Error(chunk.error);
        if (chunk.message?.content) yield chunk.message.content;
      }
      return;
    }

    case 'openai': {
      const res = await ensureOk(
        await fetch(`${base}/chat/completions`, {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
            ...(config.apiKey ? { Authorization: `Bearer ${config.apiKey}` } : {}),
          },
          body: JSON.stringify({
            model: config.model,
            stream: true,
            messages: system ? [{ role: 'system', content: system }, ...messages] : messages,
          }),
          signal,
        }),
      );
      for await (const line of readLines(res)) {
        if (!line.startsWith('data:')) continue;
        const data = line.slice(5).trim();
        if (data === '[DONE]') return;
        const chunk = JSON.parse(data);
        if (chunk.error) throw new Error(chunk.error.message ?? String(chunk.error));
        const text = chunk.choices?.[0]?.delta?.content;
        if (text) yield text;
      }
      return;
    }

    case 'anthropic': {
      const stream = anthropicClient(config).beta.messages.stream(
        {
          model: config.model,
          max_tokens: 64000,
          ...(system ? { system } : {}),
          messages,
          betas: ['server-side-fallback-2026-07-01'],
          fallbacks: 'default',
        },
        { signal },
      );
      for await (const event of stream) {
        if (event.type === 'content_block_delta' && event.delta.type === 'text_delta') yield event.delta.text;
      }
      const final = await stream.finalMessage();
      if (final.stop_reason === 'refusal') {
        yield `\n\n[Declined${final.stop_details?.category ? ` (${final.stop_details.category})` : ''}]`;
      } else if (final.stop_reason === 'max_tokens') {
        yield '\n\n[Stopped: reached max_tokens]';
      }
      return;
    }

    case 'opencode': {
      const json = { 'Content-Type': 'application/json' };
      if (!req.session.opencodeSessionId) {
        const res = await ensureOk(
          await fetch(`${base}/session`, { method: 'POST', headers: json, body: JSON.stringify({ title: 'Template chat' }), signal }),
        );
        req.session.opencodeSessionId = (await res.json()).id;
      }
      const sessionId = req.session.opencodeSessionId;
      const onAbort = () => void fetch(`${base}/session/${sessionId}/abort`, { method: 'POST' }).catch(() => {});
      signal?.addEventListener('abort', onAbort, { once: true });

      const slash = config.model.indexOf('/');
      const model = slash > 0 ? { providerID: config.model.slice(0, slash), modelID: config.model.slice(slash + 1) } : undefined;
      const latest = messages[messages.length - 1]?.content ?? '';
      try {
        const res = await ensureOk(
          await fetch(`${base}/session/${sessionId}/message`, {
            method: 'POST',
            headers: json,
            body: JSON.stringify({ parts: [{ type: 'text', text: latest }], ...(model ? { model } : {}), ...(system ? { system } : {}) }),
            signal,
          }),
        );
        const { info, parts } = await res.json();
        if (info?.error) throw new Error(info.error.data?.message ?? info.error.name ?? 'OpenCode error');
        yield (parts as { type: string; text?: string; synthetic?: boolean }[])
          .filter((p) => p.type === 'text' && !p.synthetic)
          .map((p) => p.text)
          .join('\n\n');
      } finally {
        signal?.removeEventListener('abort', onAbort);
      }
      return;
    }

    case 'websocket': {
      const latest = messages[messages.length - 1]?.content ?? '';
      yield await new Promise<string>((resolve, reject) => {
        const ws = new WebSocket(base);
        const fail = (message: string) => {
          ws.close();
          reject(new Error(message));
        };
        signal?.addEventListener('abort', () => fail('Aborted'), { once: true });
        ws.onopen = () => ws.send(JSON.stringify({ command: 'complete', args: latest }));
        ws.onerror = () => fail(`Could not connect to ${base}`);
        ws.onmessage = (event) => {
          const reply = JSON.parse(event.data);
          ws.close();
          if (reply.error) reject(new Error(reply.error));
          else resolve(reply.response ?? '');
        };
      });
      return;
    }
  }
}

/** Lists the models a backend offers; doubles as a connection test. */
export async function listModels(provider: Provider, config: ProviderConfig): Promise<string[]> {
  const base = trimSlash(config.baseUrl);
  switch (provider) {
    case 'ollama': {
      const res = await ensureOk(await fetch(`${base}/api/tags`));
      return (await res.json()).models.map((m: { name: string }) => m.name);
    }
    case 'openai': {
      const res = await ensureOk(
        await fetch(`${base}/models`, { headers: config.apiKey ? { Authorization: `Bearer ${config.apiKey}` } : {} }),
      );
      return (await res.json()).data.map((m: { id: string }) => m.id);
    }
    case 'anthropic': {
      const ids: string[] = [];
      for await (const model of anthropicClient(config).models.list()) ids.push(model.id);
      return ids;
    }
    case 'opencode': {
      const res = await ensureOk(await fetch(`${base}/config/providers`));
      const { providers } = (await res.json()) as { providers: { id: string; models: Record<string, unknown> }[] };
      return providers.flatMap((p) => Object.keys(p.models).map((m) => `${p.id}/${m}`));
    }
    case 'websocket':
      return new Promise((resolve, reject) => {
        const ws = new WebSocket(base);
        ws.onopen = () => {
          ws.close();
          resolve([]);
        };
        ws.onerror = () => reject(new Error(`Could not connect to ${base}`));
      });
  }
}
