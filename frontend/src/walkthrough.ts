import type { AnalysisResult, AstNode, ParserEvent, ProcessResult, RecordedNode } from './types'

export type Stage = 'lexer' | 'parser' | 'ast' | 'compile' | 'execution'

export type WalkthroughStep =
  | { stage: 'lexer'; tokenIndex: number }
  | { stage: 'parser'; eventIndex: number; event: ParserEvent }
  | { stage: 'ast'; path: string; node: AstNode; role: string }
  | { stage: 'compile'; process: ProcessResult }
  | { stage: 'execution'; process: ProcessResult }

export function buildWalkthrough(result: AnalysisResult, compilation?: ProcessResult, execution?: ProcessResult): WalkthroughStep[] {
  const steps: WalkthroughStep[] = result.tokens.map((_, tokenIndex) => ({ stage: 'lexer', tokenIndex }))

  result.parser.events.forEach((event, eventIndex) => {
    steps.push({ stage: 'parser', eventIndex, event })
  })

  if (result.ast) steps.push({ stage: 'ast', path: result.ast.id, node: result.ast, role: 'Completed program' })
  if (compilation) steps.push({ stage: 'compile', process: compilation })
  if (execution) steps.push({ stage: 'execution', process: execution })
  return steps
}

export function parserSnapshot(events: ParserEvent[], count: number) {
  const frames: ParserEvent[] = []
  const nodes = new Map<string, RecordedNode>()
  let focusId: string | undefined
  for (const event of events.slice(0, count)) {
    if (event.kind === 'enter') frames.push(event)
    if (event.kind === 'exit' || event.kind === 'errorExit') {
      const index = frames.findIndex((frame) => frame.callId === event.callId)
      if (index >= 0) frames.splice(index)
    }
    if (event.kind === 'createNode' && event.node) {
      nodes.set(event.node.id, event.node)
      focusId = event.node.id
    }
  }
  const children = new Set<string>()
  nodes.forEach((node) => node.children.forEach((child) => children.add(child.node.id)))
  const cache = new Map<string, AstNode>()
  function materialize(node: RecordedNode): AstNode {
    const cached = cache.get(node.id)
    if (cached) return cached
    const tree: AstNode = { id: node.id, kind: node.kind, label: node.label, children: [] }
    cache.set(node.id, tree)
    tree.children = node.children.flatMap((child) => {
      const target = 'ref' in child.node ? nodes.get(child.node.id) : child.node
      return target ? [{ role: child.role, node: materialize(target) }] : []
    })
    return tree
  }
  const roots = [...nodes.values()].filter((node) => !children.has(node.id))
  const forest: AstNode = { id: 'construction', kind: 'Construction', label: `${nodes.size} AST nodes constructed`,
    children: roots.map((node) => ({ role: 'partial tree', node: materialize(node) })) }
  function findPath(node: AstNode, path: string): string | undefined {
    if (node.id === focusId) return path
    for (const [index, child] of node.children.entries()) {
      const found = findPath(child.node, `${path}/${index}`)
      if (found) return found
    }
  }
  return { frames, forest, nodeCount: nodes.size, focusPath: findPath(forest, forest.id) }
}

export function stageStart(steps: WalkthroughStep[], stage: Stage): number {
  return steps.findIndex((step) => step.stage === stage)
}
