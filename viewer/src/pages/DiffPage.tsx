import { CartesianGrid, Line, LineChart, ReferenceLine, Scatter, ScatterChart, Tooltip, XAxis, YAxis } from 'recharts'
import { Link, useParams } from 'react-router-dom'
import { useJson } from '../api'
import Async from '../components/Async'
import type { Diff } from '../types'

// tiny markers so the two overlaid trajectories stay visible
const dot = (p: { cx?: number; cy?: number; fill?: unknown }) => (
  <circle cx={p.cx} cy={p.cy} r={1.5} fill={p.fill as string} />
)

function DiffBody({ d, batch }: { d: Diff; batch: string }) {
  const sim = d.sim_xy.filter((_, i) => i % 3 === 0).map(([x, y]) => ({ x, y }))
  const road = d.onroad_xy.filter((_, i) => i % 3 === 0).map(([x, y]) => ({ x, y }))
  const err = d.error_curve.map((e, tick) => ({ tick, e }))
  const hit = d.first_diverging_interface !== null
  return (
    <>
      <p className="sub">
        <span className="badge plat">{d.platform}</span> scenario {d.scenario_id}
        {d.injected_fault ? ` · injected ${d.injected_fault}` : ''} ·{' '}
        <Link to={`/run/${batch}/${d.run_id}`}>run page</Link>
      </p>
      <div className={`panel card ${hit ? 'hit' : 'clean'}`} data-testid="interface-card">
        <h2>{hit ? `First diverging interface: ${d.first_diverging_interface}` : 'No divergence detected'}</h2>
        <p>
          <b>{d.first_diverging_label ?? 'all interfaces match'}</b>
          {d.first_diverging_tick !== null && <> at tick {d.first_diverging_tick}</>}
        </p>
        <p>{d.verdict}</p>
        <table>
          <thead>
            <tr>
              <th>Interface</th>
              <th>Status</th>
              <th className="num">First tick</th>
              <th className="num">Max |Δ|</th>
            </tr>
          </thead>
          <tbody>
            {d.interfaces.map((i) => (
              <tr key={i.component}>
                <td>{i.label}</td>
                <td>
                  <span className={`badge ${i.status === 'match' ? 'ok' : 'bad'}`}>{i.status}</span>
                </td>
                <td className="num">{i.first_tick ?? '-'}</td>
                <td className="num">{i.max_abs_diff.toExponential(2)}</td>
              </tr>
            ))}
          </tbody>
        </table>
        <p className="sub">Not observable on-road: {d.not_observable_on_road.join(', ')}</p>
      </div>
      <div className="grid">
        <div className="panel">
          <h2 style={{ marginTop: 0 }}>Trajectory overlay</h2>
          <ScatterChart width={520} height={320}>
            <CartesianGrid stroke="#2a3441" strokeDasharray="3 3" />
            <XAxis type="number" dataKey="x" stroke="#8b98a8" domain={['auto', 'auto']} />
            <YAxis type="number" dataKey="y" stroke="#8b98a8" domain={['auto', 'auto']} />
            <Tooltip />
            <Scatter name="on-road" data={road} fill="#3fb950" shape={dot} line={{ stroke: '#3fb950', strokeWidth: 5 }} isAnimationActive={false} />
            <Scatter name="sim" data={sim} fill="#f85149" shape={dot} line={{ stroke: '#f85149', strokeWidth: 2 }} isAnimationActive={false} />
          </ScatterChart>
        </div>
        <div className="panel">
          <h2 style={{ marginTop: 0 }}>Position error (m)</h2>
          <LineChart width={520} height={320} data={err}>
            <CartesianGrid stroke="#2a3441" strokeDasharray="3 3" />
            <XAxis dataKey="tick" stroke="#8b98a8" />
            <YAxis stroke="#8b98a8" />
            <Tooltip />
            <ReferenceLine y={d.threshold_m} stroke="#d29922" strokeDasharray="4 4" label="threshold" />
            {d.divergence_tick !== null && <ReferenceLine x={d.divergence_tick} stroke="#f85149" label="diverges" />}
            <Line dataKey="e" dot={false} stroke="#4aa3ff" isAnimationActive={false} />
          </LineChart>
        </div>
      </div>
    </>
  )
}

export default function DiffPage() {
  const { batch = '', run = '' } = useParams()
  const r = useJson<Diff>(`${batch}/${run}.diff.json`)
  return (
    <>
      <h1>Diff {run}</h1>
      <Async r={r}>{(d) => <DiffBody d={d} batch={batch} />}</Async>
    </>
  )
}
