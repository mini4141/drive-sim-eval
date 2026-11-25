import { Link, Route, Routes } from 'react-router-dom'
import BatchPage from './pages/BatchPage'
import BatchesPage from './pages/BatchesPage'
import DiffPage from './pages/DiffPage'
import ProfilePage from './pages/ProfilePage'
import ReproPage from './pages/ReproPage'
import RunPage from './pages/RunPage'

export default function App() {
  return (
    <>
      <header className="nav">
        <span className="brand">Drive Sim Eval</span>
        <Link to="/">Batches</Link>
        <Link to="/repro">Reproducibility</Link>
      </header>
      <main>
        <Routes>
          <Route path="/" element={<BatchesPage />} />
          <Route path="/batch/:id" element={<BatchPage />} />
          <Route path="/run/:batch/:run" element={<RunPage />} />
          <Route path="/diff/:batch/:run" element={<DiffPage />} />
          <Route path="/profile/:batch/:run" element={<ProfilePage />} />
          <Route path="/repro" element={<ReproPage />} />
        </Routes>
      </main>
    </>
  )
}
