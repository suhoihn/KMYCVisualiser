import { StreamLanguage, type StreamParser } from '@codemirror/language'
import { EditorView } from '@codemirror/view'

const keywords = new Set([
  'print', 'if', 'else', 'while', 'for', 'continue', 'break', 'true', 'false',
  'null', 'let', 'const', 'fun', 'return', 'this', 'class', 'new', 'typealias',
  'record', 'init', 'enum',
])

const types = new Set(['int', 'double', 'bool', 'string', 'void', 'any', 'i64', 'u64', 'f64', 'byte'])

const parser: StreamParser<null> = {
  startState: () => null,
  token(stream) {
    if (stream.eatSpace()) return null
    if (stream.match('//')) {
      stream.skipToEnd()
      return 'comment'
    }
    if (stream.peek() === '"') {
      stream.next()
      while (!stream.eol()) {
        if (stream.next() === '"') break
      }
      return 'string'
    }
    if (stream.match(/^(?:\d+(?:\.\d+)?|\.\d+)/)) return 'number'
    if (stream.match(/^[A-Za-z_][A-Za-z0-9_]*/)) {
      const word = stream.current()
      if (types.has(word)) return 'typeName'
      if (keywords.has(word)) return 'keyword'
      return 'variableName'
    }
    if (stream.match(/^(?:<<=|>>=|&&=|\|\|=|\.\.\.|==|!=|>=|<=|<<|>>|&&|\|\||\?\?|!!|\+=|-=|\*=|\/=|%=|&=|\|=|\^=|->|::)/)) return 'operator'
    if (stream.match(/^[+\-*/%&|~^=><!]/)) return 'operator'
    stream.next()
    return 'punctuation'
  },
}

export const kmyLanguage = StreamLanguage.define(parser)

export const kmyEditorTheme = EditorView.theme({
  '&': { height: '100%', backgroundColor: '#0d131d', color: '#dbe6f5' },
  '.cm-content': { fontFamily: '"JetBrains Mono", "Cascadia Code", monospace', padding: '18px 0' },
  '.cm-scroller': { overflow: 'auto' },
  '.cm-gutters': { backgroundColor: '#0d131d', color: '#506078', border: 'none' },
  '.cm-activeLine': { backgroundColor: '#141e2b' },
  '.cm-activeLineGutter': { backgroundColor: '#141e2b', color: '#9fb3cc' },
  '.cm-cursor': { borderLeftColor: '#73e2c3' },
  '.cm-selectionBackground, ::selection': { backgroundColor: '#244657 !important' },
  '.cm-active-token': { backgroundColor: '#f4bd5060', borderBottom: '1px solid #f4bd50' },
  '.cm-diagnostic': { textDecoration: 'underline wavy #ff6b7a' },
}, { dark: true })

