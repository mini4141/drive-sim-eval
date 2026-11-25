import { expect, test } from '@playwright/test'

// Batch list -> batch -> run replay -> diff. Uses the seeded `demo` batch.
test('batch page to run page to diff page', async ({ page }) => {
  await page.goto('/')
  await expect(page.getByRole('heading', { name: 'Batches' })).toBeVisible()
  await page.getByRole('link', { name: 'demo', exact: true }).click()

  await expect(page.getByTestId('aggregate-table')).toBeVisible()
  await page.getByRole('link', { name: 's0002_car_diff', exact: true }).click()

  await expect(page.getByTestId('replay-canvas')).toBeVisible()
  await page.getByRole('link', { name: 'Diff vs on-road' }).click()

  const card = page.getByTestId('interface-card')
  await expect(card).toBeVisible()
  await expect(card).toContainText('VehicleModel')
})

test('repro page shows all-green rows', async ({ page }) => {
  await page.goto('/repro')
  await expect(page.getByTestId('repro-table')).toBeVisible()
  await expect(page.getByText('reproducible: all digests identical')).toBeVisible()
})
