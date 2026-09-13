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
  kind: 'consume' | 'acceptOperator'
  tokenId: string
  minimumBindingPower?: number
  bindingPower?: number
}

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

