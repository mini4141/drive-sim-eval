import { useEffect, useState } from 'react'

export const BASE: string = (import.meta.env.VITE_RESULTS_BASE_URL as string | undefined) ?? '/results'

export async function getJson<T>(path: string): Promise<T> {
  const res = await fetch(`${BASE}/${path}`)
  if (!res.ok) throw new Error(`${res.status} ${path}`)
  return (await res.json()) as T
}

export type Remote<T> = { data?: T; error?: string; loading: boolean }

export function useJson<T>(path: string): Remote<T> {
  const [state, setState] = useState<Remote<T>>({ loading: true })
  useEffect(() => {
    let live = true
    setState({ loading: true })
    getJson<T>(path)
      .then((data) => live && setState({ data, loading: false }))
      .catch((e: Error) => live && setState({ error: e.message, loading: false }))
    return () => {
      live = false
    }
  }, [path])
  return state
}
