import { useMemo } from 'react'
import { AstGraph } from './AstGraph'
import { AstTree } from './AstTree'
import { parserSnapshot } from '../walkthrough'
import type { ParserEvent } from '../types'

interface Props { events: ParserEvent[]; count: number; view: 'graph' | 'outline'; onView: (view: 'graph' | 'outline') => void }

export function ParserConstruction({ events, count, view, onView }: Props) {
  const snapshot = useMemo(() => parserSnapshot(events, count), [events, count])
  return <div className="parser-construction">
    <div className="parser-call-stack" aria-label="Parser call stack">
      <span className="eyebrow">Current parser calls · outer → inner</span>
      <div>{snapshot.frames.length ? snapshot.frames.map((frame) => <span key={frame.callId}>
        {frame.rule}<small>#{frame.callId}{frame.rule === 'parse_expression' ? ` · minBP ${frame.minimumBindingPower}` : ''}</small>
      </span>) : <p>{count ? 'Parser returned.' : 'Step into parsing to see its call stack.'}</p>}</div>
    </div>
    <div className="construction-heading"><span><b>AST under construction</b><small>{snapshot.nodeCount} nodes · recorded creation order</small></span>
      <div className="ast-view-toggle"><button aria-pressed={view === 'graph'} className={view === 'graph' ? 'active' : ''} onClick={() => onView('graph')}>Graph</button>
        <button aria-pressed={view === 'outline'} className={view === 'outline' ? 'active' : ''} onClick={() => onView('outline')}>Outline</button></div>
    </div>
    {snapshot.nodeCount ? view === 'graph'
      ? <AstGraph node={snapshot.forest} focusPath={snapshot.focusPath} />
      : <div className="ast-scroll"><AstTree node={snapshot.forest} focusPath={snapshot.focusPath} /></div>
      : <div className="construction-empty">Tokens will become nodes here as the parser constructs them.</div>}
  </div>
}
