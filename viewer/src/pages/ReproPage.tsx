import { useJson } from '../api'
import Async from '../components/Async'
import type { Repro } from '../types'

export default function ReproPage() {
  const r = useJson<Repro>('repro.json')
  return (
    <>
      <h1>Reproducibility</h1>
      <Async r={r}>
        {(d) => (
          <>
            <p className="sub">
              scenario {d.scenario_id} · {d.mode} · {d.platform} · {d.repeats} repeats × workers {d.workers.join(', ')}
            </p>
            <div className="panel">
              <h2 style={{ marginTop: 0 }}>
                <span className={`badge ${d.reproducible ? 'ok' : 'bad'}`}>
                  {d.reproducible ? 'reproducible: all digests identical' : 'NOT reproducible'}
                </span>
              </h2>
              <p className="mono">reference digest {d.reference_digest}</p>
              <table data-testid="repro-table">
                <thead>
                  <tr>
                    <th>Workers</th>
                    <th>Repeat</th>
                    <th>Digest (SHA-256)</th>
                    <th>Match</th>
                  </tr>
                </thead>
                <tbody>
                  {d.rows.map((row) => (
                    <tr key={`${row.workers}-${row.repeat}`}>
                      <td>{row.workers}</td>
                      <td>{row.repeat}</td>
                      <td className="mono">{row.digest}</td>
                      <td>
                        <span className={`badge ${row.matches ? 'ok' : 'bad'}`}>{row.matches ? '✓' : '✗'}</span>
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </>
        )}
      </Async>
    </>
  )
}
