import type { AnalysisResult } from './types'

export async function analyseSource(source: string, signal: AbortSignal): Promise<AnalysisResult> {
  const response = await fetch('/api/analysis', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ source }),
    signal,
  })

  if (!response.ok) {
    const body = await response.json().catch(() => null) as { detail?: string; message?: string } | null
    throw new Error(body?.detail ?? body?.message ?? `Analysis failed (${response.status})`)
  }

  return response.json() as Promise<AnalysisResult>
}

