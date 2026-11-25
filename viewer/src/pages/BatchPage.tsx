import { useMemo, useState } from 'react'
import { Bar, BarChart, CartesianGrid, Line, LineChart, Tooltip, XAxis, YAxis } from 'recharts'
import { Link, useParams } from 'react-router-dom'
import { useJson } from '../api'
import Async from '../components/Async'
import type { BatchIndex, Scaling } from '../types'

export function histogram(values: number[], bins = 12) {
  const max = Math.max(...values, 1e-9)
  const width = max / bins
  const counts = Array.from({ length: bins }, (_, i) => ({ bin: (i * width).toFixed(2), count: 0 }))
  for (const v of values) counts[Math.min(bins - 1, Math.floor(v / width))].count++
  return counts
}

export function AggregateTable({ index }: { index: BatchIndex }) {
  const a = index.aggregate
  const rows: [string, string][] = [
    ['Runs', String(a.n_runs)],
    ['Mean action L2 error', a.mean_action_l2_error.toFixed(3)],
    ['p95 action L2 error', a.p95_action_l2_error.toFixed(3)],
    ['Mean route completion', `${a.mean_route_completion_pct.toFixed(1)} %`],
    ['Collisions', String(a.total_collisions)],
    ['Pass / fail', `${a.passed} / ${a.failed}`],
    ['Throughput', `${index.throughput_rps.toFixed(1)} runs/s`],
  ]
  return (
    <table data-testid="aggregate-table">
      <tbody>
        {rows.map(([k, v]) => (
          <tr key={k}>
            <td>{k}</td>
            <td className="num">{v}</td>
          </tr>
        ))}
      </tbody>
    </table>
  )
}

function RunsTable({ index, id }: { index: BatchIndex; id: string }) {
  const [failedOnly, setFailedOnly] = useState(false)
  const thr = index.aggregate.thresholds.action_l2_error ?? Infinity
  const runs = index.runs.filter((r) => !failedOnly || r.metrics.action_l2_error > thr)
  return (
    <>
      <label>
        <input type="checkbox" checked={failedOnly} onChange={(e) => setFailedOnly(e.target.checked)} /> failed only
      </label>
      <div className="scroll">
        <table>
          <thead>
            <tr>
              <th>Run</th>
              <th>Platform</th>
              <th className="num">Completion %</th>
              <th className="num">Action L2</th>
              <th className="num">Collisions</th>
              <th>Digest</th>
            </tr>
          </thead>
          <tbody>
            {runs.map((r) => (
              <tr key={r.run_id}>
                <td>
                  <Link to={`/run/${id}/${r.run_id}`}>{r.run_id}</Link>
                </td>
                <td>
                  <span className="badge plat">{r.platform}</span>
                </td>
                <td className="num">{r.metrics.route_completion_pct.toFixed(1)}</td>
                <td className="num">{r.metrics.action_l2_error.toFixed(3)}</td>
                <td className="num">{r.metrics.collisions}</td>
                <td className="mono">{r.digest.slice(0, 12)}…</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </>
  )
}

export default function BatchPage() {
  const { id = '' } = useParams()
  const idx = useJson<BatchIndex>(`${id}/index.json`)
  const scaling = useJson<Scaling>(`${id}/scaling.json`)
  const hist = useMemo(
    () => (idx.data ? histogram(idx.data.runs.map((r) => r.metrics.action_l2_error)) : []),
    [idx.data],
  )
  return (
    <>
      <h1>Batch {id}</h1>
      <Async r={idx}>
        {(index) => (
          <>
            <p className="sub">
              {index.mode} · {index.workers ?? '?'} workers · {index.created_at} {index.backend ? `· ${index.backend}` : ''}
            </p>
            <div className="grid">
              <div className="panel">
                <h2 style={{ marginTop: 0 }}>Aggregate</h2>
                <AggregateTable index={index} />
              </div>
              <div className="panel">
                <h2 style={{ marginTop: 0 }}>Action L2 error histogram</h2>
                <BarChart width={460} height={260} data={hist}>
                  <CartesianGrid stroke="#2a3441" strokeDasharray="3 3" />
                  <XAxis dataKey="bin" stroke="#8b98a8" />
                  <YAxis stroke="#8b98a8" allowDecimals={false} />
                  <Tooltip />
                  <Bar dataKey="count" fill="#4aa3ff" isAnimationActive={false} />
                </BarChart>
              </div>
            </div>
            {scaling.data && (
              <div className="panel" data-testid="scaling">
                <h2 style={{ marginTop: 0 }}>
                  Worker scaling {scaling.data.digests_identical ? '(digests identical across worker counts)' : ''}
                </h2>
                <LineChart width={520} height={240} data={scaling.data.points}>
                  <CartesianGrid stroke="#2a3441" strokeDasharray="3 3" />
                  <XAxis dataKey="workers" stroke="#8b98a8" label={{ value: 'workers', position: 'insideBottom', offset: -2 }} />
                  <YAxis stroke="#8b98a8" />
                  <Tooltip />
                  <Line dataKey="runs_per_sec" name="runs/s" stroke="#3fb950" isAnimationActive={false} />
                </LineChart>
              </div>
            )}
            <h2>Runs</h2>
            <div className="panel">
              <RunsTable index={index} id={id} />
            </div>
          </>
        )}
      </Async>
    </>
  )
}
