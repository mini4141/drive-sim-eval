// Scripted screenshots (Playwright, headless). Usage: node capture.mjs <baseUrl> <outDir> <phase1|phase2> [batch]
import { chromium } from '@playwright/test'
import { mkdirSync } from 'node:fs'

const [base, out, phase, cloudBatch = 'cloud-001'] = process.argv.slice(2)
mkdirSync(out, { recursive: true })

const shots =
  phase === 'phase1'
    ? [
        ['01-batches', '/'],
        ['02-batch-overview', '/batch/local-001'],
        ['03-run-replay-car', '/run/demo/s0002_car_closed_loop', 0.55],
        ['04-run-replay-robot', '/run/demo/s0002_robot_closed_loop', 0.55],
        ['05-diff-divergence', '/diff/demo/s0002_car_diff'],
        ['06-profile', '/profile/demo/s0002_car_closed_loop'],
        ['07-repro-green', '/repro'],
        ['08-diff-injected-dropout', '/diff/demo/s0002_van_diff_sensor_dropout40'],
      ]
    : [
        ['01-cloud-batch-overview', `/batch/${cloudBatch}`],
        ['02-cloud-run-replay', `/run/${cloudBatch}/s0002`, 0.55],
        ['03-cloud-diff', '/diff/demo/s0002_car_diff'],
      ]

const browser = await chromium.launch()
const page = await browser.newPage({ viewport: { width: 1280, height: 900 } })
for (const [name, path, scrub] of shots) {
  await page.goto(base + path, { waitUntil: 'networkidle' })
  if (scrub !== undefined) {
    const canvas = page.getByTestId('replay-canvas')
    await canvas.waitFor()
    const slider = page.getByLabel('scrub')
    const max = Number(await slider.getAttribute('max'))
    await slider.fill(String(Math.round(max * scrub)))
  }
  await page.waitForTimeout(600)
  await page.screenshot({ path: `${out}/${name}.png`, fullPage: true })
  console.log(`captured ${name}`)
}
await browser.close()
