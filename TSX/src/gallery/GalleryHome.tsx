import { useState } from 'react';
import { Link } from 'react-router';
import { examples, groups } from './examples';

export default function GalleryHome() {
  const [query, setQuery] = useState('');
  const q = query.trim().toLowerCase();
  const visible = examples.filter((e) => !q || e.title.toLowerCase().includes(q) || e.group.toLowerCase().includes(q));

  return (
    <div className="min-h-screen bg-gray-100 text-gray-900">
      <div className="mx-auto max-w-6xl px-4 py-10">
        <header className="mb-8 flex flex-col gap-4 sm:flex-row sm:items-end sm:justify-between">
          <div>
            <h1 className="text-3xl font-bold">Example Gallery</h1>
            <p className="text-gray-600">{examples.length} examples</p>
          </div>
          <input
            type="search"
            value={query}
            onChange={(e) => setQuery(e.target.value)}
            placeholder="Filter examples…"
            className="w-full rounded-lg border border-gray-300 bg-white px-3 py-2 shadow-sm outline-none focus:border-gray-500 sm:w-72"
          />
        </header>

        {groups.map((group) => {
          const items = visible.filter((e) => e.group === group);
          if (items.length === 0) return null;
          return (
            <section key={group} className="mb-10">
              <h2 className="mb-3 text-sm font-semibold uppercase tracking-wide text-gray-500">{group}</h2>
              <div className="grid grid-cols-1 gap-4 sm:grid-cols-2 lg:grid-cols-3">
                {items.map((e) => (
                  <Link
                    key={e.slug}
                    to={`/examples/${e.slug}`}
                    className="group rounded-xl border border-gray-200 bg-white p-5 shadow-sm transition hover:-translate-y-0.5 hover:border-gray-400 hover:shadow-md"
                  >
                    <div className="mb-2 flex items-center justify-between text-xs text-gray-500">
                      <span className="rounded-full bg-gray-100 px-2 py-0.5">{e.group}</span>
                      <span>#{examples.indexOf(e) + 1}</span>
                    </div>
                    <h3 className="font-semibold group-hover:underline">{e.title}</h3>
                  </Link>
                ))}
              </div>
            </section>
          );
        })}

        {visible.length === 0 && <p className="text-gray-500">No examples match “{query}”.</p>}
      </div>
    </div>
  );
}
