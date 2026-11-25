import { createReadStream, existsSync, statSync } from 'node:fs'
import { join, resolve } from 'node:path'
import react from '@vitejs/plugin-react'
import type { Plugin } from 'vite'
import { defineConfig } from 'vitest/config'

// Serve ../results at /results (dev + preview) so the viewer reads the same static JSON that
// S3/CloudFront serves in the cloud.
function serveResults(): Plugin {
  const dir = resolve(process.env.RESULTS_DIR ?? '../results')
  const handler = (req: { url?: string }, res: any, next: () => void) => {
    const url = (req.url ?? '').split('?')[0]
    if (!url.startsWith('/results/')) return next()
    const file = join(dir, decodeURIComponent(url.slice('/results/'.length)))
    if (!file.startsWith(dir) || !existsSync(file) || !statSync(file).isFile()) {
      res.statusCode = 404
      return res.end('not found')
    }
    res.setHeader('Content-Type', 'application/json')
    createReadStream(file).pipe(res)
  }
  return {
    name: 'serve-results',
    configureServer: (s) => void s.middlewares.use(handler),
    configurePreviewServer: (s) => void s.middlewares.use(handler),
  }
}

export default defineConfig({
  plugins: [react(), serveResults()],
  test: {
    environment: 'jsdom',
    setupFiles: ['./src/setupTests.ts'],
    include: ['src/**/*.test.tsx'],
  },
})
