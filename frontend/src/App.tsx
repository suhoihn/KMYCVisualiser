import { useCallback, useEffect, useMemo, useRef, useState } from 'react'
import {
  AlertTriangle, BookOpen, Braces, ChevronLeft, ChevronRight, CircleCheck,
  Code2, FileCode2, LoaderCircle, Pause, Play, Plus, RotateCcw, ScanLine, Workflow,
} from 'lucide-react'
import { submitPlayground } from './api'
import { AstGraph } from './components/AstGraph'
import { AstTree } from './components/AstTree'
import { KmyEditor } from './components/KmyEditor'
import { LanguageGuide } from './components/LanguageGuide'
import { ParserConstruction } from './components/ParserConstruction'
import { importsFromTokens, mainFile, missingModuleImports, validFileName } from './moduleWorkspace'
import type { AnalysisResult, KmyToken, ParserEvent, ProcessResult } from './types'
import { buildWalkthrough, stageStart, type Stage } from './walkthrough'

const examples = {
  'Expression': `let width: int = 8;
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
  'Control flow': `var total = 0;
for (var i = 0; i < 5; i += 1) {
    if (i % 2 == 0) {
        total += i;
    }
}
print(total);`,
  'Classes': `class Counter {
    var value: int;

    init(start: int) {
        this.value = start;
    }

    fun increment(): int {
        this.value += 1;
        return this.value;
    }
};

let counter = new Counter(4);`,
  'Modules': `import "math.kmy" as math;
let result = math::triple(7);
print(result);`,
}

const moduleExample = `fun triple(value: int): int {
    return value * 3;
}`

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
  if (event.kind === 'enter') return `Enter ${event.rule}()${event.rule === 'parse_expression' ? ` with minimum binding power ${event.minimumBindingPower}` : ''}.`
  if (event.kind === 'exit') return `Return from ${event.rule}().`
  if (event.kind === 'errorExit') return `Unwind ${event.rule}() after a syntax error.`
  if (event.kind === 'createNode') return `Construct ${event.node?.kind} #${event.node?.id}: ${event.node?.label}. Connect its already-built children.`
  if (event.kind === 'returnExpression') return `Stop this expression: lookahead binding power ${event.bindingPower} is below minimum ${event.minimumBindingPower}. Return to the caller.`
  if (!token) return 'Parser event'
  if (event.kind === 'acceptOperator') {
    return `The Pratt parser accepts “${token.lexeme}”: binding power ${event.bindingPower} meets the current minimum ${event.minimumBindingPower}.`
  }
  return `The parser consumes ${token.type}${token.lexeme ? ` “${token.lexeme}”` : ''}.`
}

function App() {
  const [files, setFiles] = useState<Record<string, string>>({ [mainFile]: examples.Expression })
  const [activeFile, setActiveFile] = useState(mainFile)
  const [results, setResults] = useState<Record<string, AnalysisResult>>({})
  const [addingFile, setAddingFile] = useState(false)
  const [newFileName, setNewFileName] = useState('')
  const [fileNameError, setFileNameError] = useState<string>()
  const [stage, setStage] = useState<Stage>('lexer')
  const [astView, setAstView] = useState<'graph' | 'outline'>('graph')
  const [selectedLine, setSelectedLine] = useState(1)
  const [selectedTokenId, setSelectedTokenId] = useState<string>()
  const [cursor, setCursor] = useState(-1)
  const [playing, setPlaying] = useState(false)
  const [playbackSpeed, setPlaybackSpeed] = useState(650)
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState<string>()
  const [serverPhase, setServerPhase] = useState('ready')
  const [jobStatus, setJobStatus] = useState<string>()
  const [compilation, setCompilation] = useState<ProcessResult>()
  const [execution, setExecution] = useState<ProcessResult>()
  const [guideOpen, setGuideOpen] = useState(false)
  const filesRef = useRef(files)
  const requestRef = useRef<AbortController | undefined>(undefined)
  const source = files[activeFile] ?? ''
  const result = results[activeFile] ?? null
  const timeline = useMemo(() => result ? buildWalkthrough(result, compilation, execution) : [], [result, compilation, execution])
  const currentStep = timeline[cursor]
  const lexerVisibleCount = stage === 'lexer'
    ? currentStep?.stage === 'lexer' ? currentStep.tokenIndex + 1 : 0
    : result?.tokens.length ?? 0
  const parserEventIndex = currentStep?.stage === 'parser' ? currentStep.eventIndex
    : stage !== 'lexer' && stage !== 'parser' ? (result?.parser.events.length ?? 0) - 1 : -1
  const currentEvent = parserEventIndex >= 0 ? result?.parser.events[parserEventIndex] : undefined
  const currentAstStep = currentStep?.stage === 'ast' ? currentStep : undefined
  const consumedTokenIds = useMemo(() => new Set((result?.parser.events.slice(0, parserEventIndex + 1) ?? [])
    .filter((event) => event.kind === 'consume').map((event) => event.tokenId)), [result, parserEventIndex])
  const activeImports = result ? importsFromTokens(result.tokens, activeFile) : []
  const missingImports = useMemo(() => missingModuleImports(results, files), [results, files])
  const invalidFile = Object.entries(results).find(([, analysis]) => !analysis.ok)?.[0]
  const nativeFailed = Boolean(compilation && (compilation.exitCode !== 0 || compilation.limit)
    || execution && (execution.exitCode !== 0 || execution.limit))

  const submitAnalysis = useCallback(async () => {
    requestRef.current?.abort()
    const controller = new AbortController()
    requestRef.current = controller
    setLoading(true)
    setError(undefined)
    setResults({})
    setSelectedTokenId(undefined)
    setCursor(-1)
    setStage('lexer')
    setPlaying(false)
    setCompilation(undefined)
    setExecution(undefined)
    setJobStatus(undefined)
    setServerPhase('queued')
    try {
      let startedReplay = false
      await submitPlayground(filesRef.current, mainFile, controller.signal, (event) => {
        if (controller.signal.aborted) return
        if (event.type === 'phase') setServerPhase(event.phase)
        if (event.type === 'analysis') {
          setResults((previous) => ({ ...previous, [event.file]: event.analysis }))
          if (!startedReplay) { setPlaying(true); startedReplay = true }
        }
        if (event.type === 'compilation') setCompilation(event.compilation)
        if (event.type === 'execution') setExecution(event.execution)
        if (event.type === 'result') { setJobStatus(event.status); setServerPhase('finished') }
      })
    } catch (reason) {
      if ((reason as Error).name !== 'AbortError') { setError((reason as Error).message); setPlaying(false); setServerPhase('failed') }
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
    filesRef.current = { ...filesRef.current, [activeFile]: nextSource }
    setFiles(filesRef.current)
    setLoading(false)
    setResults({})
    setCompilation(undefined)
    setExecution(undefined)
    setJobStatus(undefined)
    setError(undefined)
    setSelectedTokenId(undefined)
    setCursor(-1)
    setStage('lexer')
    setPlaying(false)
  }, [activeFile])

  const activeTokenId = selectedTokenId ?? (stage === 'parser' ? currentEvent?.tokenId
    : stage === 'lexer' && currentStep?.stage === 'lexer' ? result?.tokens[currentStep.tokenIndex]?.id : undefined)
  const activeToken = result?.tokens.find((token) => token.id === activeTokenId)

  useEffect(() => {
    if (activeToken) setSelectedLine(activeToken.span.start.line)
  }, [activeToken])

  useEffect(() => {
    if (!playing || !timeline.length) return
    if (cursor >= timeline.length - 1) {
      if (loading) return
      setPlaying(false)
      return
    }
    const timer = window.setTimeout(() => {
      const next = cursor + 1
      setCursor(next)
      setStage(timeline[next].stage)
      setSelectedTokenId(undefined)
    }, playbackSpeed)
    return () => window.clearTimeout(timer)
  }, [playing, cursor, timeline, playbackSpeed, loading])

  const goToStep = (requested: number) => {
    const next = Math.max(-1, Math.min(requested, timeline.length - 1))
    setPlaying(false)
    setCursor(next)
    setStage(next >= 0 ? timeline[next].stage : 'lexer')
    setSelectedTokenId(undefined)
  }

  const selectStage = (target: Stage) => {
    setPlaying(false)
    setSelectedTokenId(undefined)
    setStage(target)
    const first = stageStart(timeline, target)
    if (first >= 0) setCursor(target === 'ast' || target === 'compile' || target === 'execution' ? first : first - 1)
  }

  const narration = !result ? 'Submit code to record the lexer, parser, and AST walkthrough.'
    : currentStep?.stage !== stage
      ? stage === 'compile' || stage === 'execution' ? 'The native result is shown as soon as the server returns it.'
        : stage === 'parser' ? 'Ready to parse. Press Play or Next to enter the parser.'
        : stage === 'ast' ? 'Parsing is complete. Press Play or Next to reveal the returned AST.'
          : 'Ready. Press Play or Next to reveal the first token.'
      : currentStep.stage === 'compile' ? `Native compilation ${currentStep.process.exitCode === 0 && !currentStep.process.limit ? 'succeeded' : 'failed'} in ${currentStep.process.durationMs} ms.`
        : currentStep.stage === 'execution' ? `Program exited with code ${currentStep.process.exitCode}${currentStep.process.limit ? ` (${currentStep.process.limit})` : ''}.`
        : currentStep.stage === 'lexer' ? tokenExplanation(result.tokens[currentStep.tokenIndex])
        : currentStep.stage === 'parser' ? eventExplanation(currentStep.event, result.tokens.find((token) => token.id === currentStep.event.tokenId))
          : `Revealing ${currentStep.role}: ${currentStep.node.kind} — ${currentStep.node.label}.`

  const lineTokens = useMemo(
    () => result?.tokens.slice(0, lexerVisibleCount).filter((token) => token.span.start.line === selectedLine && token.category !== 'eof') ?? [],
    [result, selectedLine, lexerVisibleCount],
  )
  const lineEvents = useMemo(() => {
    if (!result) return []
    const ids = new Set(lineTokens.map((token) => token.id))
    return result.parser.events.slice(0, parserEventIndex + 1).filter((event) => ids.has(event.tokenId))
  }, [lineTokens, result, parserEventIndex])
  const lineText = source.split('\n')[selectedLine - 1] ?? ''

  const loadExample = (name: keyof typeof examples) => {
    requestRef.current?.abort()
    requestRef.current = undefined
    filesRef.current = name === 'Modules'
      ? { [mainFile]: examples.Modules, 'math.kmy': moduleExample }
      : { [mainFile]: examples[name] }
    setFiles(filesRef.current)
    setResults({})
    setCompilation(undefined)
    setExecution(undefined)
    setJobStatus(undefined)
    setLoading(false)
    setError(undefined)
    setActiveFile(mainFile)
    setSelectedTokenId(undefined)
    setCursor(-1)
    setStage('lexer')
    setPlaying(false)
    setSelectedLine(1)
  }

  const selectFile = (path: string) => {
    setActiveFile(path)
    setSelectedLine(1)
    setSelectedTokenId(undefined)
    setCursor(-1)
    setStage('lexer')
    setPlaying(false)
  }

  const addFile = () => {
    const path = newFileName.trim()
    if (!validFileName(path)) {
      setFileNameError('Use a relative .kmy path, for example lib/math.kmy.')
      return
    }
    if (Object.hasOwn(filesRef.current, path)) {
      setFileNameError('A file with that name already exists.')
      return
    }
    requestRef.current?.abort()
    requestRef.current = undefined
    filesRef.current = { ...filesRef.current, [path]: '' }
    setFiles(filesRef.current)
    setResults({})
    setCompilation(undefined)
    setExecution(undefined)
    setJobStatus(undefined)
    setLoading(false)
    setError(undefined)
    setAddingFile(false)
    setNewFileName('')
    setFileNameError(undefined)
    selectFile(path)
  }

  return (
    <main className="app-shell">
      <header className="topbar">
        <div className="brand">
          <div className="brand-mark"><Code2 size={19} /></div>
          <div><h1>KMY <span>Front-end Visualiser</span></h1><p>See source become structure</p></div>
        </div>
        <div className="header-actions">
          <div className={`status ${error || nativeFailed ? 'status-error' : loading ? 'status-loading' : missingImports.length || invalidFile ? 'status-warning' : result?.ok ? 'status-ok' : 'status-idle'}`}>
            {error || nativeFailed ? <AlertTriangle size={14} /> : loading ? <LoaderCircle className="spin" size={14} /> : missingImports.length || invalidFile ? <AlertTriangle size={14} /> : result?.ok ? <CircleCheck size={14} /> : <ScanLine size={14} />}
            {error ? 'Job error' : loading ? `Server: ${serverPhase}` : missingImports.length ? 'Module file missing' : invalidFile ? `Syntax issue in ${invalidFile}` : jobStatus ? jobStatus.replaceAll('_', ' ') : result?.ok ? 'Syntax valid' : 'Ready to visualise'}
          </div>
          <button className="guide-button" onClick={() => setGuideOpen(true)}><BookOpen size={16} /> Language guide</button>
        </div>
      </header>

      <nav className="pipeline" aria-label="Compiler stages">
        {([
          ['lexer', ScanLine, '01', 'Lexer', 'Characters → tokens'],
          ['parser', Workflow, '02', 'Parser', 'Tokens → decisions'],
          ['ast', Braces, '03', 'AST', 'Decisions → tree'],
          ['compile', Code2, '04', 'Compile', 'Semantics → native x86'],
          ['execution', Play, '05', 'Output', 'Run Linux executable'],
        ] as const).map(([key, Icon, number, title, subtitle], index) => (
          <div className="pipeline-unit" key={key}>
            <button className={`stage-button ${stage === key ? 'active' : ''}`} onClick={() => selectStage(key)} aria-current={stage === key ? 'step' : undefined}>
              <span className="stage-number">{number}</span><Icon size={17} />
              <span><strong>{title}</strong><small>{subtitle}</small></span>
            </button>
            {index < 4 && <ChevronRight className="pipeline-arrow" size={17} />}
          </div>
        ))}
      </nav>

      {(loading || result) && <div className="job-progress" role="status">
        <span><i className={loading ? 'working' : ''} />Server: {loading ? serverPhase : error ? 'failed' : jobStatus?.replaceAll('_', ' ') ?? 'recording available'}</span>
        <span>Replay: {stage} · {playing ? 'playing' : 'paused'}</span>
        <small>Playback uses the compiler recording; it does not run the program again.</small>
      </div>}

      <section className="walkthrough-controls" aria-label="Walkthrough controls">
        <div className="walkthrough-heading"><span className="eyebrow">Replay the compilation</span><p aria-live="polite">{narration}</p></div>
        <div className="walkthrough-transport">
          <button onClick={() => goToStep(-1)} disabled={!result} aria-label="Rewind to start" title="Rewind to start"><RotateCcw size={16} /></button>
          <button onClick={() => goToStep(cursor - 1)} disabled={!result || cursor < 0} aria-label="Previous step" title="Previous step"><ChevronLeft size={17} /></button>
          <button className="play" onClick={() => {
            if (cursor >= timeline.length - 1) { setCursor(-1); setStage('lexer') }
            setPlaying((value) => !value)
          }} disabled={!timeline.length} aria-label={playing ? 'Pause walkthrough' : 'Play walkthrough'} title={playing ? 'Pause' : 'Play'}>
            {playing ? <Pause size={17} /> : <Play size={17} />}
          </button>
          <button onClick={() => goToStep(cursor + 1)} disabled={!result || cursor >= timeline.length - 1} aria-label="Next step" title="Next step"><ChevronRight size={17} /></button>
          <input type="range" aria-label="Walkthrough position" min={0} max={timeline.length} value={cursor + 1}
            onChange={(event) => goToStep(Number(event.target.value) - 1)} disabled={!timeline.length} />
          <span className="walkthrough-count">{cursor + 1} / {timeline.length}</span>
          <label className="walkthrough-speed">Speed <select aria-label="Playback speed" value={playbackSpeed} onChange={(event) => setPlaybackSpeed(Number(event.target.value))}>
            <option value={1000}>Slow</option><option value={650}>Normal</option><option value={300}>Fast</option>
          </select></label>
        </div>
      </section>

      <section className="workbench">
        <div className="editor-column panel">
          <div className="panel-header">
            <div><span className="eyebrow">Source</span><h2>{activeFile}</h2></div>
            <div className="editor-actions">
              <select aria-label="Load example" onChange={(event) => loadExample(event.target.value as keyof typeof examples)} defaultValue="Expression">
                {Object.keys(examples).map((name) => <option key={name}>{name}</option>)}
              </select>
              <button className="analyse-button" onClick={submitAnalysis} disabled={loading} title="Compile and run source (Ctrl/Cmd+Enter)">
                {loading ? <LoaderCircle className="spin" size={15} /> : <Workflow size={15} />}
                {loading ? 'Working' : 'Submit'}
              </button>
            </div>
          </div>
          <div className="file-strip" aria-label="Source files">
            {Object.keys(files).map((path) => (
              <button key={path} className={`file-tab ${activeFile === path ? 'active' : ''}`} onClick={() => selectFile(path)} title={path}>
                <FileCode2 size={13} /><span>{path}</span>{results[path] && <i className={results[path].ok ? 'file-ok' : 'file-error'} />}
              </button>
            ))}
            {addingFile ? (
              <div className="file-add-form">
                <input aria-label="New module file path" value={newFileName} placeholder="lib/math.kmy" autoFocus
                  onChange={(event) => { setNewFileName(event.target.value); setFileNameError(undefined) }}
                  onKeyDown={(event) => { if (event.key === 'Enter') addFile(); if (event.key === 'Escape') setAddingFile(false) }} />
                <button onClick={addFile}>Add</button>
                <button onClick={() => { setAddingFile(false); setFileNameError(undefined) }} aria-label="Cancel adding file">×</button>
              </div>
            ) : <button className="file-add-button" onClick={() => setAddingFile(true)}><Plus size={13} /> Add file</button>}
          </div>
          {fileNameError && <div className="file-error-message" role="alert">{fileNameError}</div>}
          <div className="editor-wrap">
            <KmyEditor
              key={activeFile}
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

        <div className="inspector-column panel" key={stage}>
          {stage === 'lexer' && (
            <>
              <div className="panel-header"><div><span className="eyebrow">Stage 01</span><h2>Token stream</h2></div><span className="metric">{lexerVisibleCount} / {result?.tokens.length ?? 0} tokens</span></div>
              <div className="token-stream">
                {!result && <div className="empty-state">Edit freely, then press Compile &amp; run or Ctrl/Cmd+Enter to record and execute your project.</div>}
                {result && lexerVisibleCount === 0 && <div className="empty-state">Press Play or Next to scan the first token.</div>}
                {result?.tokens.slice(0, lexerVisibleCount).map((token, index) => (
                  <button key={token.id} className={`token token-${token.category} ${activeTokenId === token.id ? 'selected token-arriving' : ''}`}
                    onClick={() => { goToStep(index); setSelectedTokenId(token.id); setSelectedLine(token.span.start.line) }}>
                    <span>{displayLexeme(token)}</span><small>{token.type}</small>
                  </button>
                ))}
              </div>
              {activeToken && <div className="token-detail"><span className={`token-swatch token-${activeToken.category}`} /><div><strong>{activeToken.type}</strong><p>{tokenExplanation(activeToken)}</p></div><code>{activeToken.span.start.line}:{activeToken.span.start.column}–{activeToken.span.end.line}:{activeToken.span.end.column}</code></div>}
            </>
          )}

          {stage === 'parser' && (
            <>
              <div className="panel-header"><div><span className="eyebrow">Stage 02 · {result?.parser.strategy ?? 'Pratt'}</span><h2>Parser timeline</h2></div><span className="metric">{parserEventIndex + 1} / {result?.parser.events.length ?? 0} events</span></div>
              {result && <div className="parser-token-rail" aria-label="Parser token consumption">
                {result.tokens.filter((token) => token.category !== 'eof').map((token) => (
                  <span key={token.id} className={`${consumedTokenIds.has(token.id) ? 'consumed' : ''} ${currentEvent?.tokenId === token.id ? 'current' : ''}`}
                    title={`${token.type}: ${displayLexeme(token)}`}>{displayLexeme(token)}</span>
                ))}
              </div>}
              {currentEvent && activeToken ? (
                <div className="parser-focus" key={parserEventIndex}>
                  <div className="event-type">{currentEvent.kind} · {currentEvent.rule ?? 'parser'}</div>
                  <div className="parser-token"><code>{displayLexeme(activeToken)}</code><span>{activeToken.type}</span></div>
                  <p>{eventExplanation(currentEvent, activeToken)}</p>
                  {currentEvent.kind === 'acceptOperator' && <div className="power-scale"><span>minimum <b>{currentEvent.minimumBindingPower}</b></span><div><i style={{ width: `${Math.min((currentEvent.bindingPower ?? 0) / 1.3, 100)}%` }} /></div><span>operator <b>{currentEvent.bindingPower}</b></span></div>}
                </div>
              ) : <div className="empty-state">{result ? 'Press Play or Next to enter the parser.' : 'Press Submit or Ctrl/Cmd+Enter to run the parser.'}</div>}
              <div className="event-list">
                {result?.parser.events.slice(Math.max(0, parserEventIndex - 3), parserEventIndex + 4).map((event) => {
                  const token = result.tokens.find((item) => item.id === event.tokenId)
                  return <button key={event.step} className={event.step === parserEventIndex ? 'current' : ''}
                    onClick={() => goToStep(stageStart(timeline, 'parser') + event.step)}><span>{String(event.step + 1).padStart(3, '0')}</span><b>{event.kind}</b><code>{token ? displayLexeme(token) : ''}</code></button>
                })}
              </div>
              {result && <ParserConstruction events={result.parser.events} count={parserEventIndex + 1} view={astView} onView={setAstView} />}
            </>
          )}

          {stage === 'ast' && (
            <>
              <div className="panel-header"><div><span className="eyebrow">Stage 03 · {activeFile}</span><h2>Abstract syntax tree</h2></div>
                <div className="ast-view-toggle" aria-label="AST view">
                  <button className={astView === 'graph' ? 'active' : ''} onClick={() => setAstView('graph')} aria-pressed={astView === 'graph'}>Graph</button>
                  <button className={astView === 'outline' ? 'active' : ''} onClick={() => setAstView('outline')} aria-pressed={astView === 'outline'}>Outline</button>
                </div>
              </div>
              {activeImports.length > 0 && <div className="module-imports">
                <span className="eyebrow">Module imports · separate from the compiler AST</span>
                <div>{activeImports.map((item, index) => {
                  const exists = item.targetPath && Object.hasOwn(files, item.targetPath)
                  return <button key={`${item.alias}-${index}`} className={exists ? 'module-import' : 'module-import missing'}
                    onClick={() => {
                      if (!item.targetPath) return
                      if (exists) selectFile(item.targetPath)
                      else { setNewFileName(item.targetPath); setFileNameError(undefined); setAddingFile(true) }
                    }} disabled={!item.targetPath}>
                    <FileCode2 size={15} /><span><b>{item.alias}</b><small>{item.requestedPath}</small></span><em>{exists ? 'Open file →' : item.targetPath ? 'Add this file →' : 'Invalid path'}</em>
                  </button>
                })}</div>
              </div>}
              {result?.ast && <div className="ast-reveal-note"><span>Completed parser AST</span>
                <p>Use the Parser stage to watch nodes being created and connected in recorded order.</p>
                <small>This tree is the parser result, before semantic passes and native lowering.</small>
              </div>}
              {result?.ast && currentAstStep ? astView === 'graph'
                ? <AstGraph key={activeFile} node={result.ast} />
                : <div className="ast-scroll"><AstTree key={activeFile} node={result.ast} /></div>
                : <div className="empty-state">{result?.ast ? 'The AST is ready. Advance the walkthrough to reveal it.' : 'The AST appears after parsing succeeds.'}</div>}
            </>
          )}
          {(stage === 'compile' || stage === 'execution') && (() => {
            const process = stage === 'compile' ? compilation : execution
            return <><div className="panel-header"><div><span className="eyebrow">{stage === 'compile' ? 'Stage 04 · native x86-64' : 'Stage 05 · Linux process'}</span>
              <h2>{stage === 'compile' ? 'Compilation results' : 'Program output'}</h2></div></div>
              {process ? <div className="process-result"><div className={`process-status ${process.exitCode === 0 && !process.limit ? 'ok' : 'failed'}`}>
                Exit {process.exitCode} · {process.durationMs} ms{process.limit && ` · ${process.limit.replaceAll('_', ' ')}`}</div>
                <h3>Standard output</h3><pre>{process.stdout || '(no output)'}</pre>
                {process.stderr && <><h3>Errors / diagnostics</h3><pre className="process-errors">{process.stderr}</pre></>}
              </div> : <div className="empty-state">{loading ? `Server is in ${serverPhase}. Results will appear here.` : stage === 'execution' && jobStatus === 'compiled' ? 'This deployment compiles code but does not run submitted programs.' : stage === 'execution' && compilation?.exitCode !== 0 ? 'Execution waits for successful compilation.' : 'Submit your project to record native compilation.'}</div>}
            </>
          })()}
        </div>
      </section>

      {(compilation || execution) && <section className="native-output panel" aria-label="Native program results">
        <div className="panel-header"><div><span className="eyebrow">Native x86-64 · {mainFile}</span><h2>{execution ? 'Program output' : 'Compilation results'}</h2></div>
          <button className="text-button" onClick={() => selectStage(execution ? 'execution' : 'compile')}>Inspect result →</button></div>
        <pre className={compilation?.exitCode !== 0 || execution?.limit || execution && execution.exitCode !== 0 ? 'process-errors' : ''}>
          {execution ? execution.stdout || execution.stderr || (execution.limit ? `Stopped: ${execution.limit}` : '(no output)') : compilation?.stderr || 'Compilation complete.'}
        </pre>
      </section>}

      <section className="line-story panel">
        <div className="line-heading"><span className="line-badge">{selectedLine}</span><div><span className="eyebrow">Line-by-line</span><h2>{lineText.trim() || 'Blank line'}</h2></div></div>
        <div className="story-grid">
          <article><span className="story-number">01</span><h3>Lexer sees</h3>{lineTokens.length ? lineTokens.map((token) => <button key={token.id} onClick={() => goToStep(result?.tokens.findIndex((item) => item.id === token.id) ?? -1)}><code>{displayLexeme(token)}</code><span>{token.type}</span></button>) : <p>No tokens on this line.</p>}</article>
          <article><span className="story-number">02</span><h3>Parser does</h3>{lineEvents.length ? <><p>{lineEvents.length} parser events reached this line.</p><p>{eventExplanation(lineEvents[0], result?.tokens.find((token) => token.id === lineEvents[0].tokenId))}</p><button className="text-button" onClick={() => goToStep(stageStart(timeline, 'parser') + lineEvents[0].step)}>Open at event {lineEvents[0].step + 1} →</button></> : <p>No parser events reached this line yet.</p>}</article>
          <article><span className="story-number">03</span><h3>AST becomes</h3><p>{!result ? 'Submit the source to build its AST.' : activeImports.some((item) => item.line === selectedLine) ? 'Imports are module metadata, kept outside the compiler’s Program AST. Open the AST stage to follow the imported file.' : result.ok ? 'This line contributes to the parsed Program tree. Open the AST and expand labeled child roles to see its structure.' : 'AST construction stops until the syntax diagnostic is fixed.'}</p><button className="text-button" onClick={() => selectStage('ast')}>Open AST →</button></article>
        </div>
      </section>

      <footer><span>KMY is a work in progress</span><span>Lexer → Pratt parser + AST → native Linux x86-64 → output</span></footer>
      {guideOpen && <LanguageGuide onClose={() => setGuideOpen(false)} />}
    </main>
  )
}

export default App
