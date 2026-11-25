import { render, screen } from '@testing-library/react'
import { MemoryRouter, Route, Routes } from 'react-router-dom'
import { afterEach, expect, test, vi } from 'vitest'
import BatchPage from './BatchPage'
import type { BatchIndex } from '../types'

const metrics = {
  route_completion_pct: 100,
  collisions: 0,
  mean_abs_lat_offset: 0.1,
  action_l2_error: 0.31,
  final_speed: 0,
  duration_s: 20,
}
const fixture: BatchIndex = {
  batch_id: 'fixture',
  created_at: '2026-01-01T00:00:00Z',
  throughput_rps: 48.9,
  workers: 8,
  mode: 'open_loop',
  runs: [{ run_id: 's0001', scenario_id: 's0001', platform: 'car', mode: 'open_loop', ticks: 500, metrics, digest: 'a'.repeat(64) }],
  aggregate: {
    n_runs: 200,
    mean_action_l2_error: 0.312,
    p95_action_l2_error: 0.532,
    mean_route_completion_pct: 100,
    total_collisions: 0,
    passed: 182,
    failed: 18,
    thresholds: { action_l2_error: 0.5 },
  },
}

afterEach(() => vi.unstubAllGlobals())

test('batch page renders the aggregate table from a fixture', async () => {
  vi.stubGlobal(
    'fetch',
    vi.fn(async (url: string) =>
      url.endsWith('index.json')
        ? new Response(JSON.stringify(fixture))
        : new Response('missing', { status: 404 }),
    ),
  )
  render(
    <MemoryRouter initialEntries={['/batch/fixture']}>
      <Routes>
        <Route path="/batch/:id" element={<BatchPage />} />
      </Routes>
    </MemoryRouter>,
  )
  expect(await screen.findByTestId('aggregate-table')).toBeInTheDocument()
  expect(screen.getByText('0.312')).toBeInTheDocument()
  expect(screen.getByText('182 / 18')).toBeInTheDocument()
})
