import { defineConfig } from '@playwright/test'

const cloud = process.env.VIEWER_URL
export default defineConfig({
  testDir: './e2e',
  timeout: 60_000,
  reporter: [['list'], ['html', { open: 'never', outputFolder: process.env.PW_REPORT ?? 'playwright-report' }]],
  use: { baseURL: cloud ?? 'http://127.0.0.1:5173', viewport: { width: 1280, height: 900 } },
  webServer: cloud
    ? undefined
    : { command: 'npm run dev', url: 'http://127.0.0.1:5173', reuseExistingServer: true, timeout: 60_000 },
})
