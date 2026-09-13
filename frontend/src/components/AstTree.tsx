import { useState } from 'react'
import { ChevronDown, ChevronRight } from 'lucide-react'
import type { AstNode } from '../types'

interface Props {
  node: AstNode
  role?: string
  depth?: number
}

export function AstTree({ node, role, depth = 0 }: Props) {
  const [expanded, setExpanded] = useState(depth < 3)
  const hasChildren = node.children.length > 0

  return (
    <div className="ast-branch">
      <button className="ast-node" onClick={() => hasChildren && setExpanded((value) => !value)}>
        <span className="ast-toggle">
          {hasChildren ? (expanded ? <ChevronDown size={14} /> : <ChevronRight size={14} />) : <span className="ast-dot" />}
        </span>
        {role && <span className="ast-role">{role}</span>}
        <span className={`ast-kind kind-${node.kind.toLowerCase()}`}>{node.kind}</span>
        <span className="ast-label">{node.label}</span>
      </button>
      {hasChildren && expanded && (
        <div className="ast-children">
          {node.children.map((child, index) => (
            <AstTree key={`${child.node.id}-${index}`} node={child.node} role={child.role} depth={depth + 1} />
          ))}
        </div>
      )}
    </div>
  )
}

