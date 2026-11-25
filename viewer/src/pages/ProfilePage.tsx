import { Bar, BarChart, CartesianGrid, Tooltip, XAxis, YAxis } from 'recharts'
import { Link, useParams } from 'react-router-dom'
import { BASE, useJson } from '../api'
import Async from '../components/Async'
import type { Profile } from '../types'

export default function ProfilePage() {
  const { batch = '', run = '' } = useParams()
  const r = useJson<Profile>(`${batch}/${run}.profile.json`)
  return (
    <>
      <h1>Profile {run}</h1>
      <Async r={r}>
        {(p) => {
          const rows = Object.entries(p.components)
            .filter(([name]) => name !== 'tick')
            .map(([name, t]) => ({ name, ...t }))
          return (
            <>
              <p className="sub">
                {p.ticks} ticks · <Link to={`/run/${batch}/${run}`}>run page</Link> · Chrome trace:{' '}
                <a href={`${BASE}/${batch}/${run}.trace.json`}>trace.json</a> (opens in{' '}
                <a href="https://ui.perfetto.dev">Perfetto</a>)
              </p>
              <div className="panel">
                <h2 style={{ marginTop: 0 }}>Total time per component (µs)</h2>
                <BarChart width={640} height={300} data={rows}>
                  <CartesianGrid stroke="#2a3441" strokeDasharray="3 3" />
                  <XAxis dataKey="name" stroke="#8b98a8" />
                  <YAxis stroke="#8b98a8" />
                  <Tooltip />
                  <Bar dataKey="total_us" name="total µs" fill="#4aa3ff" isAnimationActive={false} />
                </BarChart>
              </div>
              <div className="panel">
                <table>
                  <thead>
                    <tr>
                      <th>Component</th>
                      <th className="num">Calls</th>
                      <th className="num">Total µs</th>
                      <th className="num">Mean µs</th>
                      <th className="num">p95 µs</th>
                    </tr>
                  </thead>
                  <tbody>
                    {Object.entries(p.components).map(([name, t]) => (
                      <tr key={name}>
                        <td>{name}</td>
                        <td className="num">{t.count}</td>
                        <td className="num">{t.total_us.toFixed(0)}</td>
                        <td className="num">{t.mean_us.toFixed(2)}</td>
                        <td className="num">{t.p95_us.toFixed(2)}</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </>
          )
        }}
      </Async>
    </>
  )
}
