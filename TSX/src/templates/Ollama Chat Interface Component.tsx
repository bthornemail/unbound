import React, { useState, useEffect, useRef } from 'react';
import { Send, Loader } from 'lucide-react';
import { Card, CardHeader, CardTitle, CardContent } from '../components/ui/card';
import { Button } from '../components/ui/button';
import { ScrollArea } from '../components/ui/scroll-area';
import { Alert, AlertDescription } from '../components/ui/alert';

const OllamaChat = () => {
  const [messages, setMessages] = useState([]);
  const [input, setInput] = useState('');
  const [isConnected, setIsConnected] = useState(false);
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState(null);
  const ws = useRef(null);
  const scrollRef = useRef(null);

  useEffect(() => {
    connectWebSocket();
    return () => {
      if (ws.current) {
        ws.current.close();
      }
    };
  }, []);

  const connectWebSocket = () => {
    ws.current = new WebSocket('ws://localhost:9876');

    ws.current.onopen = () => {
      setIsConnected(true);
      setError(null);
      addSystemMessage('Connected to Emacs AI Agent');
    };

    ws.current.onclose = () => {
      setIsConnected(false);
      setError('Connection lost. Trying to reconnect...');
      setTimeout(connectWebSocket, 3000);
    };

    ws.current.onerror = () => {
      setError('Failed to connect to the AI Agent');
    };

    ws.current.onmessage = (event) => {
      const response = JSON.parse(event.data);
      addMessage('assistant', response.response || response.error || 'No response');
      setIsLoading(false);
    };
  };

  const addSystemMessage = (content) => {
    setMessages(prev => [...prev, { role: 'system', content }]);
  };

  const addMessage = (role, content) => {
    setMessages(prev => [...prev, { role, content }]);
  };

  const handleSubmit = (e) => {
    e.preventDefault();
    if (!input.trim() || !isConnected || isLoading) return;

    const message = input.trim();
    setInput('');
    addMessage('user', message);
    setIsLoading(true);

    ws.current.send(JSON.stringify({
      command: 'complete',
      args: message
    }));
  };

  useEffect(() => {
    if (scrollRef.current) {
      scrollRef.current.scrollIntoView({ behavior: 'smooth' });
    }
  }, [messages]);

  const MessageBubble = ({ message }) => (
    <div className={`flex ${message.role === 'user' ? 'justify-end' : 'justify-start'} mb-4`}>
      <div
        className={`px-4 py-2 rounded-lg max-w-[80%] ${
          message.role === 'user'
            ? 'bg-blue-500 text-white'
            : message.role === 'system'
            ? 'bg-gray-200 text-gray-800'
            : 'bg-gray-100 text-gray-800'
        }`}
      >
        <p className="whitespace-pre-wrap">{message.content}</p>
      </div>
    </div>
  );

  return (
    <Card className="w-full max-w-2xl mx-auto h-[600px] flex flex-col">
      <CardHeader>
        <CardTitle className="flex items-center justify-between">
          <span>Emacs AI Agent Chat</span>
          <div className="flex items-center gap-2">
            <div
              className={`w-3 h-3 rounded-full ${
                isConnected ? 'bg-green-500' : 'bg-red-500'
              }`}
            />
            <span className="text-sm">
              {isConnected ? 'Connected' : 'Disconnected'}
            </span>
          </div>
        </CardTitle>
      </CardHeader>
      <CardContent className="flex-1 flex flex-col gap-4">
        {error && (
          <Alert variant="destructive">
            <AlertDescription>{error}</AlertDescription>
          </Alert>
        )}
        <ScrollArea className="flex-1 pr-4">
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
            placeholder="Type your message..."
            className="flex-1 p-2 rounded-md border border-gray-300 focus:outline-none focus:ring-2 focus:ring-blue-500"
            disabled={!isConnected || isLoading}
          />
          <Button 
            type="submit" 
            disabled={!isConnected || isLoading || !input.trim()}
            className="px-4"
          >
            {isLoading ? (
              <Loader className="w-5 h-5 animate-spin" />
            ) : (
              <Send className="w-5 h-5" />
            )}
          </Button>
        </form>
      </CardContent>
    </Card>
  );
};

export default OllamaChat;