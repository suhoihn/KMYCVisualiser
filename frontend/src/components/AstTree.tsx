import { useState } from 'react'
import { ChevronDown, ChevronRight } from 'lucide-react'
import type { AstNode } from '../types'

interface Props {
  node: AstNode
  role?: string
  depth?: number
  path?: string
  visiblePaths?: ReadonlySet<string>
  focusPath?: string
}

export function AstTree({ node, role, depth = 0, path = node.id, visiblePaths, focusPath }: Props) {
  const [expanded, setExpanded] = useState(depth < 3)
  const visibleChildren = node.children.map((child, index) => ({ child, index }))
    .filter(({ index }) => !visiblePaths || visiblePaths.has(`${path}/${index}`))
  const hasChildren = visibleChildren.length > 0
  const isOpen = expanded || Boolean(focusPath?.startsWith(`${path}/`))

  return (
    <div className="ast-branch">
      <button className={`ast-node ${focusPath === path ? 'ast-outline-focus' : ''}`} onClick={() => hasChildren && setExpanded((value) => !value)}>
        <span className="ast-toggle">
          {hasChildren ? (isOpen ? <ChevronDown size={14} /> : <ChevronRight size={14} />) : <span className="ast-dot" />}
        </span>
        {role && <span className="ast-role">{role}</span>}
        <span className={`ast-kind kind-${node.kind.toLowerCase()}`}>{node.kind}</span>
        <span className="ast-label">{node.label}</span>
      </button>
      {hasChildren && isOpen && (
        <div className="ast-children">
          {visibleChildren.map(({ child, index }) => (
              <AstTree key={`${child.node.id}-${index}`} node={child.node} role={child.role} depth={depth + 1}
                path={`${path}/${index}`} visiblePaths={visiblePaths} focusPath={focusPath} />
          ))}
        </div>
      )}
    </div>
  )
}
