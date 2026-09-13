import { useMemo } from 'react'
import CodeMirror from '@uiw/react-codemirror'
import { Decoration, EditorView, keymap } from '@codemirror/view'
import { Prec, type Extension } from '@codemirror/state'
import { kmyEditorTheme, kmyLanguage } from '../kmyLanguage'
import type { Diagnostic, SourceSpan } from '../types'

interface Props {
  value: string
  activeSpan?: SourceSpan
  diagnostics?: Diagnostic[]
  onChange: (value: string) => void
  onLineChange: (line: number) => void
  onSubmit: () => void
}

function markRange(span: SourceSpan, className: string, documentLength: number): Extension | null {
  const start = Math.max(0, Math.min(span.start.offset, documentLength))
  const end = Math.max(start, Math.min(span.end.offset, documentLength))
  if (end <= start) return null
  return EditorView.decorations.of(
    Decoration.set([
      Decoration.mark({ class: className }).range(start, end),
    ]),
  )
}

export function KmyEditor({ value, activeSpan, diagnostics, onChange, onLineChange, onSubmit }: Props) {
  // While the user is editing there are no decorations, so keep this key stable and
  // avoid reconfiguring CodeMirror on every keystroke.
  const decorationBoundary = activeSpan || diagnostics?.length ? value.length : 0
  const extensions = useMemo<Extension[]>(() => {
    const configured: Extension[] = [
      kmyLanguage,
      kmyEditorTheme,
      Prec.highest(keymap.of([{ key: 'Mod-Enter', run: () => { onSubmit(); return true } }])),
    ]
    const activeMark = activeSpan ? markRange(activeSpan, 'cm-active-token', decorationBoundary) : null
    if (activeMark) configured.push(activeMark)
    diagnostics?.forEach((diagnostic) => {
      const mark = markRange(diagnostic.span, 'cm-diagnostic', decorationBoundary)
      if (mark) configured.push(mark)
    })
    return configured
  }, [activeSpan, decorationBoundary, diagnostics, onSubmit])

  return (
    <CodeMirror
      value={value}
      height="100%"
      placeholder="Write KMY code, then press Ctrl/Cmd+Enter…"
      autoFocus
      extensions={extensions}
      onChange={onChange}
      onUpdate={(viewUpdate) => {
        if (viewUpdate.selectionSet) {
          const line = viewUpdate.state.doc.lineAt(viewUpdate.state.selection.main.head).number
          onLineChange(line)
        }
      }}
      basicSetup={{
        foldGutter: true,
        highlightActiveLine: true,
        highlightActiveLineGutter: true,
        bracketMatching: true,
        closeBrackets: true,
      }}
    />
  )
}
