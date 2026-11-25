import type { ReactNode } from 'react'
import type { Remote } from '../api'

export default function Async<T>({ r, children }: { r: Remote<T>; children: (data: T) => ReactNode }) {
  if (r.loading) return <p className="sub">Loading…</p>
  if (r.error || !r.data) return <p className="err">Could not load data: {r.error}</p>
  return <>{children(r.data)}</>
}
