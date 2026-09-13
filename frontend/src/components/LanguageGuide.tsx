import { X } from 'lucide-react'

interface Props { onClose: () => void }

const sections = [
  ['Variables', 'let score: int = 10;\nlet const limit = 100;', 'Variables are mutable by default. Put const after let to make one immutable.'],
  ['Functions', 'fun add(a: int, b: int): int {\n    return a + b;\n}', 'Functions use fun, typed parameters, an optional return type, and a block body.'],
  ['Control flow', 'if (score > 5) {\n    print(score);\n} else {\n    print(0);\n}', 'if and while conditions require parentheses. Their bodies use braces.'],
  ['Loops', 'for (let i = 0; i < 5; i += 1) {\n    print(i);\n}', 'The parser lowers for into an initializer plus a while loop. The AST view exposes that transformation.'],
  ['Arrays', 'let values: int[] = [1, 2, 3];\nvalues[0] = 9;', 'Array literals, array types, indexing, and index assignment are supported by the front end.'],
  ['Records', 'let point = {x: 2, y: 4};\nprint(point.x);', 'A brace expression containing name/value pairs creates an anonymous record literal.'],
  ['Classes', 'class Counter {\n    let value: int;\n    init(start: int) { this.value = start; }\n};', 'Classes and records can contain fields, methods, constructors, and nested enums.'],
  ['Enums', 'enum Color { RED, GREEN, BLUE };\nlet color = Color::RED;', 'Use :: for scoped enum access. Aggregate and enum declarations end in a semicolon.'],
  ['Types', 'int  double  bool  string  void  any\ni64  u64  f64  byte\nint[]  int[5]  (int, int)->int', 'KMY supports named, array, function, record-shaped, scoped, and nullable types in the parser.'],
]

export function LanguageGuide({ onClose }: Props) {
  return (
    <div className="guide-backdrop" onMouseDown={onClose}>
      <aside className="guide" onMouseDown={(event) => event.stopPropagation()}>
        <div className="guide-header">
          <div><span className="eyebrow">Language notes</span><h2>Learn KMY</h2></div>
          <button className="icon-button" onClick={onClose} aria-label="Close guide"><X size={18} /></button>
        </div>
        <p className="guide-intro">KMY is evolving. This guide describes syntax accepted by the current lexer and Pratt parser; later compiler backends may support a smaller subset.</p>
        <div className="guide-sections">
          {sections.map(([title, code, description]) => (
            <section key={title}>
              <h3>{title}</h3>
              <p>{description}</p>
              <pre><code>{code}</code></pre>
            </section>
          ))}
        </div>
      </aside>
    </div>
  )
}
