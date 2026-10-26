import { Component, Suspense, useEffect, type ErrorInfo, type ReactNode } from 'react';
import { Link, useNavigate, useParams } from 'react-router';
import { examples, findExample } from './examples';

class ExampleErrorBoundary extends Component<{ children: ReactNode }, { error: Error | null }> {
  state = { error: null as Error | null };

  static getDerivedStateFromError(error: Error) {
    return { error };
  }

  componentDidCatch(error: Error, info: ErrorInfo) {
    console.error(error, info.componentStack);
  }

  render() {
    if (this.state.error) {
      return (
        <div className="m-6 rounded-lg border border-red-300 bg-red-50 p-6 text-red-800">
          <h2 className="mb-2 font-semibold">This example failed to render</h2>
          <pre className="whitespace-pre-wrap text-sm">{this.state.error.message}</pre>
        </div>
      );
    }
    return this.props.children;
  }
}

export default function ExampleView() {
  const { slug } = useParams();
  const navigate = useNavigate();
  const index = findExample(slug);

  const prev = examples[(index - 1 + examples.length) % examples.length];
  const next = examples[(index + 1) % examples.length];

  useEffect(() => {
    if (index < 0) return;
    const onKey = (e: KeyboardEvent) => {
      const target = e.target as HTMLElement | null;
      if (target?.closest('input, textarea, select, [contenteditable="true"]')) return;
      if (e.key === 'ArrowLeft') navigate(`/examples/${prev.slug}`);
      else if (e.key === 'ArrowRight') navigate(`/examples/${next.slug}`);
      else if (e.key === 'Escape') navigate('/');
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [index, prev, next, navigate]);

  if (index < 0) {
    return (
      <div className="flex min-h-screen flex-col items-center justify-center gap-4 bg-gray-100">
        <p className="text-lg text-gray-700">No example called “{slug}”.</p>
        <Link to="/" className="rounded-lg bg-gray-800 px-4 py-2 text-white hover:bg-gray-700">
          Back to gallery
        </Link>
      </div>
    );
  }

  const example = examples[index];
  const navButton = 'rounded-lg px-3 py-1.5 text-sm hover:bg-gray-700';

  return (
    <div className="min-h-screen">
      <nav className="sticky top-0 z-50 flex items-center gap-3 bg-gray-900 px-4 py-2 text-white shadow">
        <Link to="/" className={navButton}>
          ← Gallery
        </Link>
        <h1 className="min-w-0 flex-1 truncate text-center font-semibold">
          {example.title}
          <span className="ml-2 text-sm font-normal text-gray-400">
            {index + 1} / {examples.length}
          </span>
        </h1>
        <Link to={`/examples/${prev.slug}`} className={navButton} title={prev.title}>
          ← Prev
        </Link>
        <Link to={`/examples/${next.slug}`} className={navButton} title={next.title}>
          Next →
        </Link>
      </nav>

      <ExampleErrorBoundary key={example.slug}>
        <Suspense fallback={<p className="p-8 text-center text-gray-500">Loading {example.title}…</p>}>
          <example.Component />
        </Suspense>
      </ExampleErrorBoundary>
    </div>
  );
}
