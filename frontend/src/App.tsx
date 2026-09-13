import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import {
  AlertTriangle, BookOpen, Braces, ChevronLeft, ChevronRight, CircleCheck,
  Code2, LoaderCircle, Pause, Play, RotateCcw, ScanLine, Workflow,
} from 'lucide-react'
import { analyseSource } from './api'
import { AstTree } from './components/AstTree'
import { KmyEditor } from './components/KmyEditor'
import { LanguageGuide } from './components/LanguageGuide'
import type { AnalysisResult, KmyToken, ParserEvent } from './types'

const examples = {
  'Expression': `let const width: int = 8;
let height = 3;
let area = width * height + 2;
print(area);`,
  'Functions': `fun fibonacci(n: int): int {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

print(fibonacci(6));`,
  'Control flow': `let total = 0;
for (let i = 0; i < 5; i += 1) {
    if (i % 2 == 0) {
        total += i;
    }
}
print(total);`,
  'Classes': `class Counter {
    let value: int;

    init(start: int) {
        this.value = start;
    }

    fun increment(): int {
        this.value += 1;
        return this.value;
    }
};

let counter = new Counter(4);`,
}

type Stage = 'lexer' | 'parser' | 'ast'

function displayLexeme(token: KmyToken) {
  if (token.type === 'EndOfFile') return 'EOF'
  if (token.type === 'StringLiteral') return `“${token.lexeme}”`
  return token.lexeme
}

function tokenExplanation(token: KmyToken) {
  if (token.category === 'keyword') return `The lexer classified “${token.lexeme}” as the ${token.type} keyword.`
  if (token.category === 'identifier') return `“${token.lexeme}” is an identifier; the parser will decide its role from context.`
  if (token.category === 'literal') return `“${token.lexeme}” becomes a ${token.type} literal token.`
  if (token.category === 'operator') return `“${token.lexeme}” is the ${token.type} operator.`
  if (token.category === 'punctuation') return `“${token.lexeme}” marks structure as ${token.type}.`
  if (token.category === 'unknown') return `The lexer does not recognise “${token.lexeme}”.`
  return 'The end-of-file sentinel tells the parser that the program is complete.'
}

function eventExplanation(event: ParserEvent, token?: KmyToken) {
  if (!token) return 'Parser event'
  if (event.kind === 'acceptOperator') {
    return `The Pratt parser accepts “${token.lexeme}”: binding power ${event.bindingPower} meets the current minimum ${event.minimumBindingPower}.`
  }
  return `The parser consumes ${token.type}${token.lexeme ? ` “${token.lexeme}”` : ''}.`
}

function App() {
  const [source, setSource] = useState(examples.Expression)
  const [result, setResult] = useState<AnalysisResult | null>(null)
  const [stage, setStage] = useState<Stage>('lexer')
  const [selectedLine, setSelectedLine] = useState(1)
  const [selectedTokenId, setSelectedTokenId] = useState<string>()
  const [parserStep, setParserStep] = useState(0)
  const [playing, setPlaying] = useState(false)
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState<string>()
  const [guideOpen, setGuideOpen] = useState(false)
  const sourceRef = useRef(source)
  const requestRef = useRef<AbortController | undefined>(undefined)

  const submitAnalysis = useCallback(async () => {
    requestRef.current?.abort()
    const controller = new AbortController()
    requestRef.current = controller
    setLoading(true)
    setError(undefined)
    setResult(null)
    setSelectedTokenId(undefined)
    setParserStep(0)
    setPlaying(false)
    try {
      const analysis = await analyseSource(sourceRef.current, controller.signal)
      setResult(analysis)
    } catch (reason) {
      if ((reason as Error).name !== 'AbortError') setError((reason as Error).message)
    } finally {
      if (requestRef.current === controller) {
        requestRef.current = undefined
        setLoading(false)
      }
    }
  }, [])

  useEffect(() => () => requestRef.current?.abort(), [])

  const handleSourceChange = useCallback((nextSource: string) => {
    requestRef.current?.abort()
    requestRef.current = undefined
    sourceRef.current = nextSource
    setSource(nextSource)
    setLoading(false)
    setResult(null)
    setError(undefined)
    setSelectedTokenId(undefined)
    setParserStep(0)
    setPlaying(false)
  }, [])

  const currentEvent = result?.parser.events[parserStep]
  const activeTokenId = stage === 'parser' ? currentEvent?.tokenId : selectedTokenId
  const activeToken = result?.tokens.find((token) => token.id === activeTokenId)

  useEffect(() => {
    if (activeToken) setSelectedLine(activeToken.span.start.line)
  }, [activeToken])

  useEffect(() => {
    if (!playing || stage !== 'parser' || !result?.parser.events.length) return
    const timer = window.setInterval(() => {
      setParserStep((current) => {
        if (current >= result.parser.events.length - 1) {
          setPlaying(false)
          return current
        }
        return current + 1
      })
    }, 650)
    return () => window.clearInterval(timer)
  }, [playing, result, stage])

  const lineTokens = useMemo(
    () => result?.tokens.filter((token) => token.span.start.line === selectedLine && token.category !== 'eof') ?? [],
    [result, selectedLine],
  )
  const lineEvents = useMemo(() => {
    if (!result) return []
    const ids = new Set(lineTokens.map((token) => token.id))
    return result.parser.events.filter((event) => ids.has(event.tokenId))
  }, [lineTokens, result])
  const lineText = source.split('\n')[selectedLine - 1] ?? ''

  const loadExample = (name: keyof typeof examples) => {
    handleSourceChange(examples[name])
    setSelectedLine(1)
  }

  return (
    <main className="app-shell">
      <header className="topbar">
        <div className="brand">
          <div className="brand-mark"><Code2 size={19} /></div>
          <div><h1>KMY <span>Front-end Visualiser</span></h1><p>See source become structure</p></div>
        </div>
        <div className="header-actions">
          <div className={`status ${error ? 'status-error' : loading ? 'status-loading' : result?.ok ? 'status-ok' : result ? 'status-warning' : 'status-idle'}`}>
            {error ? <AlertTriangle size={14} /> : loading ? <LoaderCircle className="spin" size={14} /> : result?.ok ? <CircleCheck size={14} /> : result ? <AlertTriangle size={14} /> : <ScanLine size={14} />}
            {error ? 'Backend offline' : loading ? 'Analysing' : result?.ok ? 'Syntax valid' : result ? 'Needs attention' : 'Ready to visualise'}
          </div>
          <button className="guide-button" onClick={() => setGuideOpen(true)}><BookOpen size={16} /> Language guide</button>
        </div>
      </header>

      <nav className="pipeline" aria-label="Compiler stages">
        {([
          ['lexer', ScanLine, '01', 'Lexer', 'Characters → tokens'],
          ['parser', Workflow, '02', 'Parser', 'Tokens → decisions'],
          ['ast', Braces, '03', 'AST', 'Decisions → tree'],
        ] as const).map(([key, Icon, number, title, subtitle], index) => (
          <div className="pipeline-unit" key={key}>
            <button className={`stage-button ${stage === key ? 'active' : ''}`} onClick={() => setStage(key)}>
              <span className="stage-number">{number}</span><Icon size={17} />
              <span><strong>{title}</strong><small>{subtitle}</small></span>
            </button>
            {index < 2 && <ChevronRight className="pipeline-arrow" size={17} />}
          </div>
        ))}
      </nav>

      <section className="workbench">
        <div className="editor-column panel">
          <div className="panel-header">
            <div><span className="eyebrow">Source</span><h2>playground.kmy</h2></div>
            <div className="editor-actions">
              <select aria-label="Load example" onChange={(event) => loadExample(event.target.value as keyof typeof examples)} defaultValue="Expression">
                {Object.keys(examples).map((name) => <option key={name}>{name}</option>)}
              </select>
              <button className="analyse-button" onClick={submitAnalysis} disabled={loading} title="Visualise source (Ctrl/Cmd+Enter)">
                {loading ? <LoaderCircle className="spin" size={15} /> : <Workflow size={15} />}
                {loading ? 'Analysing' : 'Visualise'}
              </button>
            </div>
          </div>
          <div className="editor-wrap">
            <KmyEditor
              value={source}
              activeSpan={activeToken?.span}
              diagnostics={result?.diagnostics}
              onChange={handleSourceChange}
              onLineChange={setSelectedLine}
              onSubmit={submitAnalysis}
            />
          </div>
          {result?.diagnostics.map((diagnostic, index) => (
            <button className="diagnostic" key={`${diagnostic.message}-${index}`} onClick={() => setSelectedLine(diagnostic.span.start.line)}>
              <AlertTriangle size={15} /><span><strong>Line {diagnostic.span.start.line}</strong>{diagnostic.message}</span>
            </button>
          ))}
          {error && <div className="backend-error"><AlertTriangle size={16} />{error}<button onClick={submitAnalysis}>Retry</button></div>}
        </div>

        <div className="inspector-column panel">
          {stage === 'lexer' && (
            <>
              <div className="panel-header"><div><span className="eyebrow">Stage 01</span><h2>Token stream</h2></div><span className="metric">{result?.tokens.length ?? 0} tokens</span></div>
              <div className="token-stream">
                {!result && <div className="empty-state">Edit freely, then press Visualise or Ctrl/Cmd+Enter to run the compiler front end.</div>}
                {result?.tokens.map((token) => (
                  <button key={token.id} className={`token token-${token.category} ${activeTokenId === token.id ? 'selected' : ''}`} onClick={() => { setSelectedTokenId(token.id); setSelectedLine(token.span.start.line) }}>
                    <span>{displayLexeme(token)}</span><small>{token.type}</small>
                  </button>
                ))}
              </div>
              {activeToken && <div className="token-detail"><span className={`token-swatch token-${activeToken.category}`} /><div><strong>{activeToken.type}</strong><p>{tokenExplanation(activeToken)}</p></div><code>{activeToken.span.start.line}:{activeToken.span.start.column}–{activeToken.span.end.line}:{activeToken.span.end.column}</code></div>}
            </>
          )}

          {stage === 'parser' && (
            <>
              <div className="panel-header"><div><span className="eyebrow">Stage 02 · {result?.parser.strategy ?? 'Pratt'}</span><h2>Parser timeline</h2></div><span className="metric">{result?.parser.events.length ?? 0} events</span></div>
              <div className="transport">
                <button onClick={() => setParserStep(0)} aria-label="Restart"><RotateCcw size={16} /></button>
                <button onClick={() => setParserStep((value) => Math.max(0, value - 1))} aria-label="Previous"><ChevronLeft size={17} /></button>
                <button className="play" onClick={() => setPlaying((value) => !value)} aria-label={playing ? 'Pause' : 'Play'}>{playing ? <Pause size={17} /> : <Play size={17} />}</button>
                <button onClick={() => setParserStep((value) => Math.min((result?.parser.events.length ?? 1) - 1, value + 1))} aria-label="Next"><ChevronRight size={17} /></button>
                <input type="range" min={0} max={Math.max((result?.parser.events.length ?? 1) - 1, 0)} value={parserStep} onChange={(event) => { setParserStep(Number(event.target.value)); setPlaying(false) }} />
                <span>{result?.parser.events.length ? parserStep + 1 : 0} / {result?.parser.events.length ?? 0}</span>
              </div>
              {currentEvent && activeToken ? (
                <div className="parser-focus">
                  <div className="event-type">{currentEvent.kind === 'acceptOperator' ? 'Binding power decision' : 'Consume token'}</div>
                  <div className="parser-token"><code>{displayLexeme(activeToken)}</code><span>{activeToken.type}</span></div>
                  <p>{eventExplanation(currentEvent, activeToken)}</p>
                  {currentEvent.kind === 'acceptOperator' && <div className="power-scale"><span>minimum <b>{currentEvent.minimumBindingPower}</b></span><div><i style={{ width: `${Math.min((currentEvent.bindingPower ?? 0) / 1.3, 100)}%` }} /></div><span>operator <b>{currentEvent.bindingPower}</b></span></div>}
                </div>
              ) : <div className="empty-state">Press Visualise or Ctrl/Cmd+Enter to run the parser.</div>}
              <div className="event-list">
                {result?.parser.events.slice(Math.max(0, parserStep - 3), parserStep + 4).map((event) => {
                  const token = result.tokens.find((item) => item.id === event.tokenId)
                  return <button key={event.step} className={event.step === parserStep ? 'current' : ''} onClick={() => setParserStep(event.step)}><span>{String(event.step + 1).padStart(3, '0')}</span><b>{event.kind}</b><code>{token ? displayLexeme(token) : ''}</code></button>
                })}
              </div>
            </>
          )}

          {stage === 'ast' && (
            <>
              <div className="panel-header"><div><span className="eyebrow">Stage 03</span><h2>Abstract syntax tree</h2></div><span className="metric">Interactive tree</span></div>
              <div className="ast-scroll">{result?.ast ? <AstTree node={result.ast} /> : <div className="empty-state">The AST appears after parsing succeeds.</div>}</div>
            </>
          )}
        </div>
      </section>

      <section className="line-story panel">
        <div className="line-heading"><span className="line-badge">{selectedLine}</span><div><span className="eyebrow">Line-by-line</span><h2>{lineText.trim() || 'Blank line'}</h2></div></div>
        <div className="story-grid">
          <article><span className="story-number">01</span><h3>Lexer sees</h3>{lineTokens.length ? lineTokens.map((token) => <button key={token.id} onClick={() => { setStage('lexer'); setSelectedTokenId(token.id) }}><code>{displayLexeme(token)}</code><span>{token.type}</span></button>) : <p>No tokens on this line.</p>}</article>
          <article><span className="story-number">02</span><h3>Parser does</h3>{lineEvents.length ? <><p>{lineEvents.length} parser events touch this line.</p><p>{eventExplanation(lineEvents[0], result?.tokens.find((token) => token.id === lineEvents[0].tokenId))}</p><button className="text-button" onClick={() => { setStage('parser'); setParserStep(lineEvents[0].step) }}>Open at event {lineEvents[0].step + 1} →</button></> : <p>No parser events on this line.</p>}</article>
          <article><span className="story-number">03</span><h3>AST becomes</h3><p>{!result ? 'Submit the source to build its AST.' : result.ok ? 'This line contributes to the parsed Program tree. Open the AST and expand labeled child roles to see its structure.' : 'AST construction stops until the syntax diagnostic is fixed.'}</p><button className="text-button" onClick={() => setStage('ast')}>Open AST →</button></article>
        </div>
      </section>

      <footer><span>KMY is a work in progress</span><span>Visualising lexer → Pratt parser → AST only</span></footer>
      {guideOpen && <LanguageGuide onClose={() => setGuideOpen(false)} />}
    </main>
  )
}

export default App
