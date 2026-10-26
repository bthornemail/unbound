import { useEffect, useRef, useState, type FormEvent } from 'react';
import { Send, Loader, Settings, Square, Trash2, PlugZap } from 'lucide-react';
import { Card, CardHeader, CardTitle, CardContent } from '../components/ui/card';
import { Button } from '../components/ui/button';
import { ScrollArea } from '../components/ui/scroll-area';
import { Alert, AlertDescription } from '../components/ui/alert';
import {
  PROVIDERS,
  listModels,
  streamChat,
  type ChatMessage,
  type ChatSession,
  type Provider,
  type ProviderConfig,
} from '../lib/llm';

interface UiMessage {
  role: 'user' | 'assistant' | 'system';
  content: string;
}

interface Settings {
  provider: Provider;
  configs: Record<Provider, ProviderConfig>;
  system: string;
}

const STORAGE_KEY = 'llm-chat-settings';

const defaultSettings = (): Settings => ({
  provider: 'ollama',
  configs: Object.fromEntries(
    Object.entries(PROVIDERS).map(([id, p]) => [id, { ...p.defaults }]),
  ) as Record<Provider, ProviderConfig>,
  system: '',
});

const loadSettings = (): Settings => {
  const fallback = defaultSettings();
  try {
    const saved = JSON.parse(localStorage.getItem(STORAGE_KEY) ?? 'null');
    if (!saved) return fallback;
    return { ...fallback, ...saved, configs: { ...fallback.configs, ...saved.configs } };
  } catch {
    return fallback;
  }
};

const inputClass =
  'w-full p-2 rounded-md border border-gray-300 text-sm focus:outline-none focus:ring-2 focus:ring-blue-500';

const LlmChat = () => {
  const [settings, setSettings] = useState<Settings>(loadSettings);
  const [showSettings, setShowSettings] = useState(false);
  const [models, setModels] = useState<string[]>([]);
  const [status, setStatus] = useState<'unknown' | 'ok' | 'error'>('unknown');
  const [messages, setMessages] = useState<UiMessage[]>([]);
  const [input, setInput] = useState('');
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const abortRef = useRef<AbortController | null>(null);
  const sessionRef = useRef<ChatSession>({});
  const scrollRef = useRef<HTMLDivElement>(null);

  const provider = settings.provider;
  const config = settings.configs[provider];
  const info = PROVIDERS[provider];

  useEffect(() => {
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(settings));
    } catch {
      // storage unavailable (private window); settings just won't persist
    }
  }, [settings]);

  useEffect(() => {
    scrollRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [messages]);

  useEffect(() => () => abortRef.current?.abort(), []);

  const updateConfig = (patch: Partial<ProviderConfig>) =>
    setSettings((s) => ({ ...s, configs: { ...s.configs, [s.provider]: { ...s.configs[s.provider], ...patch } } }));

  const switchProvider = (next: Provider) => {
    setSettings((s) => ({ ...s, provider: next }));
    setModels([]);
    setStatus('unknown');
    sessionRef.current = {};
  };

  const testConnection = async () => {
    setError(null);
    try {
      const found = await listModels(provider, config);
      setModels(found);
      setStatus('ok');
      setMessages((prev) => [
        ...prev,
        { role: 'system', content: `Connected to ${info.label}${found.length ? ` (${found.length} models)` : ''}` },
      ]);
    } catch (err) {
      setStatus('error');
      setError(err instanceof Error ? err.message : String(err));
    }
  };

  const clearChat = () => {
    abortRef.current?.abort();
    setMessages([]);
    setError(null);
    sessionRef.current = {};
  };

  const handleSubmit = async (e: FormEvent) => {
    e.preventDefault();
    const text = input.trim();
    if (!text || isLoading) return;

    const history: ChatMessage[] = [
      ...messages.filter((m): m is ChatMessage => m.role !== 'system' && m.content !== ''),
      { role: 'user', content: text },
    ];
    setInput('');
    setError(null);
    setMessages((prev) => [...prev, { role: 'user', content: text }, { role: 'assistant', content: '' }]);
    setIsLoading(true);

    const controller = new AbortController();
    abortRef.current = controller;
    try {
      for await (const chunk of streamChat({
        provider,
        config,
        system: settings.system,
        messages: history,
        signal: controller.signal,
        session: sessionRef.current,
      })) {
        setMessages((prev) => {
          const next = [...prev];
          const last = next[next.length - 1];
          next[next.length - 1] = { ...last, content: last.content + chunk };
          return next;
        });
      }
      setStatus('ok');
    } catch (err) {
      if (!controller.signal.aborted) {
        setStatus('error');
        setError(err instanceof Error ? err.message : String(err));
      }
      setMessages((prev) => (prev[prev.length - 1]?.content === '' ? prev.slice(0, -1) : prev));
    } finally {
      setIsLoading(false);
      abortRef.current = null;
    }
  };

  const MessageBubble = ({ message }: { message: UiMessage }) => (
    <div className={`flex ${message.role === 'user' ? 'justify-end' : 'justify-start'} mb-4`}>
      <div
        className={`px-4 py-2 rounded-lg max-w-[80%] ${
          message.role === 'user'
            ? 'bg-blue-500 text-white'
            : message.role === 'system'
              ? 'bg-gray-200 text-gray-800 text-sm'
              : 'bg-gray-100 text-gray-800'
        }`}
      >
        <p className="whitespace-pre-wrap">{message.content || '…'}</p>
      </div>
    </div>
  );

  return (
    <Card className="w-full max-w-2xl mx-auto h-[700px] flex flex-col">
      <CardHeader>
        <CardTitle className="flex items-center justify-between gap-2">
          <span>LLM Chat</span>
          <div className="flex items-center gap-2">
            <div
              className={`w-3 h-3 rounded-full ${
                status === 'ok' ? 'bg-green-500' : status === 'error' ? 'bg-red-500' : 'bg-gray-300'
              }`}
            />
            <span className="text-sm font-normal">
              {info.label}
              {config.model ? ` · ${config.model}` : ''}
            </span>
            <Button variant="ghost" size="icon" onClick={clearChat} title="Clear chat">
              <Trash2 className="w-4 h-4" />
            </Button>
            <Button variant="ghost" size="icon" onClick={() => setShowSettings((v) => !v)} title="Connection settings">
              <Settings className="w-4 h-4" />
            </Button>
          </div>
        </CardTitle>
      </CardHeader>

      <CardContent className="flex-1 flex flex-col gap-4 min-h-0">
        {showSettings && (
          <div className="rounded-lg border border-gray-200 bg-gray-50 p-3 space-y-2 text-sm">
            <div className="grid grid-cols-1 sm:grid-cols-2 gap-2">
              <label className="space-y-1">
                <span className="text-gray-600">Provider</span>
                <select
                  className={inputClass}
                  value={provider}
                  onChange={(e) => switchProvider(e.target.value as Provider)}
                >
                  {(Object.keys(PROVIDERS) as Provider[]).map((id) => (
                    <option key={id} value={id}>
                      {PROVIDERS[id].label}
                    </option>
                  ))}
                </select>
              </label>
              <label className="space-y-1">
                <span className="text-gray-600">Model</span>
                <input
                  className={inputClass}
                  list="llm-chat-models"
                  value={config.model}
                  onChange={(e) => updateConfig({ model: e.target.value })}
                  placeholder={provider === 'opencode' ? 'provider/model (blank = default)' : 'model id'}
                  disabled={provider === 'websocket'}
                />
                <datalist id="llm-chat-models">
                  {models.map((m) => (
                    <option key={m} value={m} />
                  ))}
                </datalist>
              </label>
            </div>
            <label className="block space-y-1">
              <span className="text-gray-600">Base URL</span>
              <input className={inputClass} value={config.baseUrl} onChange={(e) => updateConfig({ baseUrl: e.target.value })} />
            </label>
            {info.needsKey && (
              <label className="block space-y-1">
                <span className="text-gray-600">API key</span>
                <input
                  className={inputClass}
                  type="password"
                  autoComplete="off"
                  value={config.apiKey}
                  onChange={(e) => updateConfig({ apiKey: e.target.value })}
                />
              </label>
            )}
            {provider !== 'websocket' && (
              <label className="block space-y-1">
                <span className="text-gray-600">System prompt</span>
                <textarea
                  className={inputClass}
                  rows={2}
                  value={settings.system}
                  onChange={(e) => setSettings((s) => ({ ...s, system: e.target.value }))}
                />
              </label>
            )}
            <div className="flex items-center justify-between gap-2">
              <p className="text-xs text-gray-500">{info.hint}</p>
              <Button variant="outline" size="sm" onClick={testConnection}>
                <PlugZap className="w-4 h-4 mr-1" /> Test
              </Button>
            </div>
          </div>
        )}

        {error && (
          <Alert variant="destructive">
            <AlertDescription>{error}</AlertDescription>
          </Alert>
        )}

        <ScrollArea className="flex-1 min-h-0 pr-4">
          <div className="space-y-4">
            {messages.map((message, index) => (
              <MessageBubble key={index} message={message} />
            ))}
            <div ref={scrollRef} />
          </div>
        </ScrollArea>

        <form onSubmit={handleSubmit} className="flex gap-2">
          <input
            type="text"
            value={input}
            onChange={(e) => setInput(e.target.value)}
            placeholder={`Message ${info.label}…`}
            className="flex-1 p-2 rounded-md border border-gray-300 focus:outline-none focus:ring-2 focus:ring-blue-500"
            disabled={isLoading}
          />
          {isLoading ? (
            <Button type="button" onClick={() => abortRef.current?.abort()} className="px-4" title="Stop">
              <Loader className="w-5 h-5 animate-spin" />
              <Square className="w-4 h-4" />
            </Button>
          ) : (
            <Button type="submit" disabled={!input.trim()} className="px-4">
              <Send className="w-5 h-5" />
            </Button>
          )}
        </form>
      </CardContent>
    </Card>
  );
};

export default LlmChat;
