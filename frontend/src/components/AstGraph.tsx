import { useMemo, useState } from 'react'
import { Maximize2, Minus, Plus } from 'lucide-react'
import type { AstNode } from '../types'

interface Props {
  node: AstNode
  visiblePaths?: ReadonlySet<string>
  focusPath?: string
}

interface PlacedNode {
  key: string
  node: AstNode
  x: number
  y: number
  depth: number
}

interface PlacedEdge {
  fromX: number
  fromY: number
  toX: number
  toY: number
  role: string
}

const nodeWidth = 176
const nodeHeight = 62
const horizontalGap = 28
const verticalGap = 58
const padding = 28

function shorten(value: string, limit: number) {
  return value.length > limit ? `${value.slice(0, limit - 1)}…` : value
}

function layoutTree(root: AstNode, overrides: Record<string, boolean>, visiblePaths?: ReadonlySet<string>, focusPath?: string) {
  const nodes: PlacedNode[] = []
  const edges: PlacedEdge[] = []
  const widths = new Map<string, number>()
  let maxDepth = 0

  function place(node: AstNode, key: string, depth: number, x: number): number {
    const expanded = (overrides[key] ?? (depth < 3)) || Boolean(focusPath?.startsWith(`${key}/`))
    const children = expanded ? node.children.map((child, index) => ({ ...child, index })).filter((child) => !visiblePaths || visiblePaths.has(`${key}/${child.index}`)) : []
    const childWidths = children.map((child) => measure(child.node, `${key}/${child.index}`, depth + 1))
    const descendantsWidth = childWidths.reduce((sum, width) => sum + width, 0) + Math.max(0, children.length - 1) * horizontalGap
    const width = Math.max(nodeWidth, descendantsWidth)
    const centerX = x + width / 2
    const y = depth * (nodeHeight + verticalGap)
    maxDepth = Math.max(maxDepth, depth)
    nodes.push({ key, node, x: centerX - nodeWidth / 2, y, depth })

    let childX = x + (width - descendantsWidth) / 2
    children.forEach((child, visibleIndex) => {
      const childWidth = childWidths[visibleIndex]
      const childCenter = childX + childWidth / 2
      edges.push({
        fromX: centerX,
        fromY: y + nodeHeight,
        toX: childCenter,
        toY: y + nodeHeight + verticalGap,
        role: child.role,
      })
      place(child.node, `${key}/${child.index}`, depth + 1, childX)
      childX += childWidth + horizontalGap
    })
    return width
  }

  function measure(node: AstNode, key: string, depth: number): number {
    const cached = widths.get(key)
    if (cached !== undefined) return cached
    if (!((overrides[key] ?? (depth < 3)) || Boolean(focusPath?.startsWith(`${key}/`))) || !node.children.length) {
      widths.set(key, nodeWidth)
      return nodeWidth
    }
    const children = node.children.map((child, index) => ({ ...child, index })).filter((child) => !visiblePaths || visiblePaths.has(`${key}/${child.index}`))
    const childrenWidth = children.reduce((total, child) => total + measure(child.node, `${key}/${child.index}`, depth + 1), 0)
    const width = Math.max(nodeWidth, childrenWidth + Math.max(0, children.length - 1) * horizontalGap)
    widths.set(key, width)
    return width
  }

  const width = place(root, root.id, 0, padding)
  return { nodes, edges, width: width + padding * 2, height: (maxDepth + 1) * nodeHeight + maxDepth * verticalGap + padding * 2 }
}

function nodeTone(kind: string) {
  if (/Program|Block|Module/.test(kind)) return 'structure'
  if (/Literal|Variable|Parameter|Field/.test(kind)) return 'value'
  if (/Binary|Unary|Assignment|Call|Get|Index|ScopeAccess/.test(kind)) return 'expression'
  return 'statement'
}

export function AstGraph({ node, visiblePaths, focusPath }: Props) {
  const [overrides, setOverrides] = useState<Record<string, boolean>>({})
  const [selectedKey, setSelectedKey] = useState(node.id)
  const [zoom, setZoom] = useState(1)
  const layout = useMemo(() => layoutTree(node, overrides, visiblePaths, focusPath), [node, overrides, visiblePaths, focusPath])
  const selected = layout.nodes.find((item) => item.key === focusPath) ?? layout.nodes.find((item) => item.key === selectedKey) ?? layout.nodes[0]
  const visibleChildCount = (item: PlacedNode) => item.node.children.filter((_, index) => !visiblePaths || visiblePaths.has(`${item.key}/${index}`)).length

  const toggleNode = (item: PlacedNode) => {
    setSelectedKey(item.key)
    if (visibleChildCount(item)) {
      setOverrides((current) => ({ ...current, [item.key]: !(current[item.key] ?? (item.depth < 3 || Boolean(focusPath?.startsWith(`${item.key}/`)))) }))
    }
  }

  return (
    <div className="ast-graph-view">
      <div className="ast-graph-toolbar">
        <span>Click a node to inspect and expand its branches</span>
        <div className="ast-zoom-controls" aria-label="Graph zoom">
          <button onClick={() => setZoom((value) => Math.max(.5, +(value - .2).toFixed(1)))} aria-label="Zoom out"><Minus size={14} /></button>
          <span>{Math.round(zoom * 100)}%</span>
          <button onClick={() => setZoom((value) => Math.min(1.6, +(value + .2).toFixed(1)))} aria-label="Zoom in"><Plus size={14} /></button>
          <button onClick={() => setZoom(1)} aria-label="Reset zoom"><Maximize2 size={14} /></button>
        </div>
      </div>
      <div className="ast-graph-scroll">
        <svg width={layout.width * zoom} height={layout.height * zoom} viewBox={`0 0 ${layout.width} ${layout.height}`} role="group" aria-label="Graphical abstract syntax tree">
          <defs>
            <pattern id="ast-grid" width="22" height="22" patternUnits="userSpaceOnUse">
              <circle cx="1" cy="1" r="1" fill="#283649" />
            </pattern>
          </defs>
          <rect width="100%" height="100%" fill="url(#ast-grid)" />
          {layout.edges.map((edge, index) => {
            const midY = (edge.fromY + edge.toY) / 2
            return (
              <g key={`${index}-${edge.role}`}>
                <path className="ast-graph-edge" d={`M ${edge.fromX} ${edge.fromY} C ${edge.fromX} ${midY}, ${edge.toX} ${midY}, ${edge.toX} ${edge.toY}`} />
                <text className="ast-graph-role" x={(edge.fromX + edge.toX) / 2} y={midY - 5} textAnchor="middle">{shorten(edge.role, 20)}</text>
              </g>
            )
          })}
          {layout.nodes.map((item) => {
            const expanded = (overrides[item.key] ?? (item.depth < 3)) || Boolean(focusPath?.startsWith(`${item.key}/`))
            const childCount = visibleChildCount(item)
            return (
              <g
                key={item.key}
                className={`ast-graph-node tone-${nodeTone(item.node.kind)} ${selected?.key === item.key ? 'selected' : ''} ${focusPath === item.key ? 'ast-just-revealed' : ''}`}
                transform={`translate(${item.x} ${item.y})`}
                role="button"
                tabIndex={0}
                aria-label={`${item.node.kind}: ${item.node.label}${childCount ? `, ${expanded ? 'collapse' : 'expand'} ${childCount} revealed children` : ''}`}
                onClick={() => toggleNode(item)}
                onKeyDown={(event) => { if (event.key === 'Enter' || event.key === ' ') { event.preventDefault(); toggleNode(item) } }}
              >
                <title>{`${item.node.kind}: ${item.node.label}`}</title>
                <rect width={nodeWidth} height={nodeHeight} rx="12" />
                <text className="ast-graph-kind" x="14" y="24">{shorten(item.node.kind, 21)}</text>
                <text className="ast-graph-label" x="14" y="46">{shorten(item.node.label, 23)}</text>
                {childCount > 0 && <><circle className="ast-graph-toggle" cx={nodeWidth - 15} cy="17" r="10" /><text className="ast-graph-toggle-label" x={nodeWidth - 15} y="21" textAnchor="middle">{expanded ? '−' : '+'}</text></>}
              </g>
            )
          })}
        </svg>
      </div>
      {selected && <div className="ast-graph-detail"><span>{selected.node.kind}</span><strong>{selected.node.label}</strong><small>{visibleChildCount(selected)} children revealed</small></div>}
    </div>
  )
}
