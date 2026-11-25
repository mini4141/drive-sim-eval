export interface Metrics {
  route_completion_pct: number
  collisions: number
  mean_abs_lat_offset: number
  action_l2_error: number
  final_speed: number
  duration_s: number
}

export interface RunSummary {
  run_id: string
  scenario_id: string
  platform: string
  mode: string
  ticks: number
  metrics: Metrics
  digest: string
}

export interface Aggregate {
  n_runs: number
  mean_action_l2_error: number
  p95_action_l2_error: number
  mean_route_completion_pct: number
  total_collisions: number
  passed: number
  failed: number
  thresholds: Record<string, number>
}

export interface BatchIndex {
  batch_id: string
  created_at: string
  runs: RunSummary[]
  aggregate: Aggregate
  throughput_rps: number
  workers?: number
  mode?: string
  wall_s?: number
  backend?: string
}

export interface ManifestEntry {
  batch_id: string
  created_at: string
  n_runs: number
  mode: string
  throughput_rps: number
}

export interface ScalingPoint {
  workers: number
  wall_s: number
  runs_per_sec: number
  speedup: number
  combined_digest: string
}

export interface Scaling {
  batch_id: string
  n_runs: number
  cpu_count: number
  digests_identical: boolean
  points: ScalingPoint[]
}

export type Xy = [number, number]

export interface Run extends RunSummary {
  config_hash: string
  seed: number
  dt: number
  trajectory: number[][]
  series: { lat_offset: number[]; speed: number[]; action_err: number[] }
  map: { nodes: { id: number; x: number; y: number }[]; edges: { from: number; to: number }[] }
  route: Xy[]
  actors: { id: number; xy: Xy[] }[]
  injected_fault?: string | null
}

export interface InterfaceStatus {
  component: string
  label: string
  tolerance: number
  first_tick: number | null
  max_abs_diff: number
  status: 'match' | 'diverges'
}

export interface Diff {
  run_id: string
  scenario_id: string
  platform: string
  injected_fault: string | null
  threshold_m: number
  divergence_tick: number | null
  max_position_error_m: number
  first_diverging_interface: string | null
  first_diverging_label: string | null
  first_diverging_tick: number | null
  interfaces: InterfaceStatus[]
  not_observable_on_road: string[]
  verdict: string
  error_curve: number[]
  sim_xy: Xy[]
  onroad_xy: Xy[]
}

export interface Profile {
  run_id: string
  ticks: number
  components: Record<string, { count: number; total_us: number; mean_us: number; p95_us: number }>
}

export interface Repro {
  scenario_id: string
  mode: string
  platform: string
  repeats: number
  workers: number[]
  reference_digest: string
  rows: { workers: number; repeat: number; digest: string; matches: boolean }[]
  reproducible: boolean
}
