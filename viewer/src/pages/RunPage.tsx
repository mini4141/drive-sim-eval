import { useMemo } from 'react'
import { CartesianGrid, Line, LineChart, Tooltip, XAxis, YAxis } from 'recharts'
import { Link, useParams } from 'react-router-dom'
import { useJson } from '../api'
import Async from '../components/Async'
import ReplayCanvas from '../components/ReplayCanvas'
import type { Run } from '../types'

function Series({ title, data, color }: { title: string; data: { tick: number; value: number }[]; color: string }) {
  return (
    <div className="panel">
      <h2 style={{ marginTop: 0 }}>{title}</h2>
      <LineChart width={360} height={180} data={data}>
        <CartesianGrid stroke="#2a3441" strokeDasharray="3 3" />
        <XAxis dataKey="tick" stroke="#8b98a8" />
        <YAxis stroke="#8b98a8" />
        <Tooltip />
        <Line dataKey="value" dot={false} stroke={color} isAnimationActive={false} />
      </LineChart>
    </div>
  )
}

function RunBody({ run, batch }: { run: Run; batch: string }) {
  const series = useMemo(() => {
    const down = (a: number[]) => a.map((value, tick) => ({ tick, value })).filter((_, i) => i % 4 === 0)
    return {
      lat: down(run.series.lat_offset),
      speed: down(run.series.speed),
      err: down(run.series.action_err),
    }
  }, [run])
  const m = run.metrics
  return (
    <>
      <p className="sub">
        <span className="badge plat">{run.platform}</span> {run.mode} · scenario {run.scenario_id} · {run.ticks} ticks
        {run.injected_fault ? ` · fault ${run.injected_fault}` : ''}
      </p>
      <p>
        <Link to={`/diff/${batch}/${run.run_id}`}>Diff vs on-road</Link> ·{' '}
        <Link to={`/profile/${batch}/${run.run_id}`}>Profile</Link> · digest <span className="mono">{run.digest}</span>
      </p>
      <ReplayCanvas run={run} />
      <div className="grid" style={{ marginTop: 16 }}>
        <div className="panel stat">{m.route_completion_pct.toFixed(1)} %<small>route completion</small></div>
        <div className="panel stat">{m.collisions}<small>collisions</small></div>
        <div className="panel stat">{m.mean_abs_lat_offset.toFixed(2)} m<small>mean |lateral offset|</small></div>
        <div className="panel stat">{m.action_l2_error.toFixed(3)}<small>action L2 error vs on-road</small></div>
      </div>
      <div className="grid">
        <Series title="Lateral offset (m)" data={series.lat} color="#d29922" />
        <Series title="Speed (m/s)" data={series.speed} color="#3fb950" />
        <Series title="Action error" data={series.err} color="#f85149" />
      </div>
    </>
  )
}

export default function RunPage() {
  const { batch = '', run = '' } = useParams()
  const r = useJson<Run>(`${batch}/${run}.json`)
  return (
    <>
      <h1>Run {run}</h1>
      <Async r={r}>{(data) => <RunBody run={data} batch={batch} />}</Async>
    </>
  )
}
