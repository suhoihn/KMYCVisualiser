export interface SourcePosition {
  offset: number
  line: number
  column: number
}

export interface SourceSpan {
  start: SourcePosition
  end: SourcePosition
}

export interface KmyToken {
  id: string
  type: string
  category: 'keyword' | 'literal' | 'identifier' | 'operator' | 'punctuation' | 'eof' | 'unknown'
  lexeme: string
  span: SourceSpan
}

export interface ParserEvent {
  step: number
  kind: 'consume' | 'acceptOperator' | 'returnExpression' | 'enter' | 'exit' | 'errorExit' | 'createNode'
  tokenId: string
  callId?: number
  parentCallId?: number
  depth?: number
  rule?: string
  node?: RecordedNode
  minimumBindingPower?: number
  bindingPower?: number
}

export interface RecordedNode {
  id: string
  kind: string
  label: string
  children: { role: string; node: RecordedNode | { id: string; ref: true } }[]
}

export interface ProcessResult {
  exitCode: number
  stdout: string
  stderr: string
  durationMs: number
  limit: 'timeout' | 'output_limit' | 'heap_limit' | null
  assembly?: { text: string; truncated: boolean }
}

export type PlaygroundEvent =
  | { type: 'phase'; phase: string; file?: string }
  | { type: 'analysis'; file: string; analysis: AnalysisResult }
  | { type: 'compilation'; compilation: ProcessResult }
  | { type: 'execution'; execution: ProcessResult }
  | { type: 'result'; status: string }
  | { type: 'error'; message: string }

export interface AstChild {
  role: string
  node: AstNode
}

export interface AstNode {
  id: string
  kind: string
  label: string
  children: AstChild[]
}

export interface Diagnostic {
  severity: 'error' | 'warning'
  phase: string
  message: string
  span: SourceSpan
}

export interface AnalysisResult {
  schemaVersion: number
  ok: boolean
  tokens: KmyToken[]
  parser: {
    strategy: string
    events: ParserEvent[]
  }
  ast: AstNode | null
  diagnostics: Diagnostic[]
}

