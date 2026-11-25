import { useEffect, useMemo, useRef, useState } from 'react'
import type { Run } from '../types'

const W = 1100
const H = 560

export default function ReplayCanvas({ run }: { run: Run }) {
  const ref = useRef<HTMLCanvasElement>(null)
  const [tick, setTick] = useState(0)
  const [playing, setPlaying] = useState(false)
  const last = run.trajectory.length - 1

  const view = useMemo(() => {
    const pts: [number, number][] = [...run.route, ...run.trajectory.map((r) => [r[0], r[1]] as [number, number])]
    const xs = pts.map((p) => p[0])
    const ys = pts.map((p) => p[1])
    const [x0, x1, y0, y1] = [Math.min(...xs), Math.max(...xs), Math.min(...ys), Math.max(...ys)]
    const scale = 0.92 * Math.min(W / Math.max(x1 - x0, 1), H / Math.max(y1 - y0, 1))
    const cx = (x0 + x1) / 2
    const cy = (y0 + y1) / 2
    return {
      scale,
      px: (x: number) => W / 2 + (x - cx) * scale,
      py: (y: number) => H / 2 - (y - cy) * scale,
    }
  }, [run])

  useEffect(() => {
    if (!playing) return
    const id = setInterval(() => {
      setTick((t) => Math.min(t + 2, last))
    }, 50)
    return () => clearInterval(id)
  }, [playing, last])

  useEffect(() => {
    if (tick >= last) setPlaying(false)
  }, [tick, last])

  useEffect(() => {
    const c = ref.current
    const g = c?.getContext('2d')
    if (!c || !g) return
    const { px, py, scale } = view
    g.clearRect(0, 0, W, H)
    const nodes = new Map(run.map.nodes.map((n) => [n.id, n]))
    g.strokeStyle = '#2f3b4a'
    g.lineWidth = 1
    g.beginPath()
    for (const e of run.map.edges) {
      const a = nodes.get(e.from)
      const b = nodes.get(e.to)
      if (a && b) {
        g.moveTo(px(a.x), py(a.y))
        g.lineTo(px(b.x), py(b.y))
      }
    }
    g.stroke()
    g.strokeStyle = '#4aa3ff'
    g.lineWidth = 3
    g.beginPath()
    run.route.forEach(([x, y], i) => (i ? g.lineTo(px(x), py(y)) : g.moveTo(px(x), py(y))))
    g.stroke()
    g.strokeStyle = 'rgba(63,185,80,0.6)'
    g.lineWidth = 2
    g.beginPath()
    for (let i = 0; i <= tick; i++) {
      const r = run.trajectory[i]
      if (i) g.lineTo(px(r[0]), py(r[1]))
      else g.moveTo(px(r[0]), py(r[1]))
    }
    g.stroke()
    g.fillStyle = '#d29922'
    for (const a of run.actors) {
      const p = a.xy[Math.min(tick, a.xy.length - 1)]
      g.fillRect(px(p[0]) - 5, py(p[1]) - 5, 10, 10)
    }
    const [x, y, yaw] = run.trajectory[tick]
    const len = Math.max(4.5 * scale, 14)
    g.save()
    g.translate(px(x), py(y))
    g.rotate(-yaw)
    g.fillStyle = '#3fb950'
    g.beginPath()
    g.moveTo(len / 2, 0)
    g.lineTo(-len / 2, len / 3)
    g.lineTo(-len / 2, -len / 3)
    g.closePath()
    g.fill()
    g.restore()
  }, [run, tick, view])

  const row = run.trajectory[tick]
  return (
    <div>
      <canvas ref={ref} width={W} height={H} data-testid="replay-canvas" />
      <div className="controls">
        <button
          onClick={() => {
            if (tick >= last) setTick(0)
            setPlaying(!playing)
          }}
        >
          {playing ? 'Pause' : 'Play'}
        </button>
        <input
          type="range"
          min={0}
          max={last}
          value={tick}
          aria-label="scrub"
          onChange={(e) => setTick(Number(e.target.value))}
        />
        <span className="mono">
          t={(tick * run.dt).toFixed(1)}s v={row[3].toFixed(1)}m/s steer={row[4].toFixed(2)} acc={row[5].toFixed(1)}
        </span>
      </div>
    </div>
  )
}
