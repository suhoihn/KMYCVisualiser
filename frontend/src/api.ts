import type { AnalysisResult, PlaygroundEvent } from './types'

export async function submitPlayground(files: Record<string, string>, entryFile: string, signal: AbortSignal,
  onEvent: (event: PlaygroundEvent) => void) {
  const response = await fetch('./api/playground', {
    method: 'POST', headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ files, entryFile, action: 'run' }), signal,
  })
  if (!response.ok) {
    const problem = await response.json().catch(() => null)
    throw new Error(problem?.detail ?? problem?.message ?? `Playground request failed (${response.status})`)
  }
  if (!response.body) throw new Error('The browser did not receive a compiler stream')
  const reader = response.body.getReader()
  const decoder = new TextDecoder()
  let pending = ''
  let total = 0
  let completed = false
  const line = (text: string) => {
    if (!text.trim()) return
    const event = JSON.parse(text) as PlaygroundEvent
    if (event.type === 'error') throw new Error(event.message)
    if (event.type === 'result') completed = true
    onEvent(event)
  }
  try {
    while (true) {
      const { value, done } = await reader.read()
      if (done) break
      total += value.byteLength
      if (total > 8 * 1024 * 1024) throw new Error('Recording exceeds 8 MiB')
      pending += decoder.decode(value, { stream: true })
      const lines = pending.split('\n')
      pending = lines.pop() ?? ''
      lines.forEach(line)
    }
    pending += decoder.decode()
    line(pending)
    if (!completed) throw new Error('Compiler stream ended before the final result')
  } finally {
    await reader.cancel().catch(() => {})
    reader.releaseLock()
  }
}

export async function analyseSource(source: string, signal: AbortSignal): Promise<AnalysisResult> {
  const response = await fetch('./api/analysis', {
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

