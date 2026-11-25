import { Link } from 'react-router-dom'
import { useJson } from '../api'
import Async from '../components/Async'
import type { ManifestEntry } from '../types'

export default function BatchesPage() {
  const r = useJson<ManifestEntry[]>('batches.json')
  return (
    <>
      <h1>Batches</h1>
      <p className="sub">Evaluation batches found in the results store.</p>
      <Async r={r}>
        {(batches) => (
          <div className="panel">
            <table>
              <thead>
                <tr>
                  <th>Batch</th>
                  <th>Mode</th>
                  <th className="num">Runs</th>
                  <th className="num">Throughput (runs/s)</th>
                  <th>Created</th>
                </tr>
              </thead>
              <tbody>
                {batches.map((b) => (
                  <tr key={b.batch_id}>
                    <td>
                      <Link to={`/batch/${b.batch_id}`}>{b.batch_id}</Link>
                    </td>
                    <td>{b.mode}</td>
                    <td className="num">{b.n_runs}</td>
                    <td className="num">{b.throughput_rps ? b.throughput_rps.toFixed(1) : '-'}</td>
                    <td className="mono">{b.created_at}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </Async>
    </>
  )
}
