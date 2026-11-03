import { X } from 'lucide-react'

interface Props { onClose: () => void }

const sections = [
  ['Variables', 'var score: int = 10;\nlet limit = 100;', 'let creates an immutable binding. Use var when the value must be reassigned.'],
  ['Functions', 'fun add(a: int, b: int): int {\n    return a + b;\n}', 'Functions use fun, typed parameters, an optional return type, and a block body.'],
  ['Control flow', 'if (score > 5) {\n    print(score);\n} else {\n    print(0);\n}', 'if and while conditions require parentheses. Their bodies use braces.'],
  ['Loops', 'for (var i = 0; i < 5; i += 1) {\n    print(i);\n}', 'The parser lowers for into an initializer plus a while loop. Parser replay shows the generated nodes being built.'],
  ['Arrays', 'var values = [1, 2, 3];\nvalues[0] = 9;', 'Arrays support literals, indexing, and index assignment.'],
  ['Anonymous records — parser only', 'let point = {x: 2, y: 4};', 'You can inspect the AST for this syntax, but the Linux x86-64 backend cannot compile anonymous record literals in v0.1. Use a named class for grouped fields in native code.'],
  ['Classes', 'class Counter {\n    var value: int;\n    init(start: int) { this.value = start; }\n};\nlet counter = new Counter(4);', 'Use a named class for fields, methods, and constructors.'],
  ['Enums', 'enum Color { RED, GREEN, BLUE };\nlet color = Color::RED;', 'Use :: for scoped enum access. Aggregate and enum declarations end in a semicolon.'],
  ['Modules', 'import "lib/math.kmy" as math;\nprint(math::triple(7));', 'Imports go at the top of a file. Add the imported file in the source-file bar. Each file has its own parser recording; native compilation resolves the submitted project together.'],
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
        <p className="guide-intro">KMY v0.1 targets Linux x86-64. A successful parse means you can inspect the AST, but does not guarantee the native backend supports that syntax.</p>
        <div className="guide-sections">
          <section>
            <h3>Playground limits</h3>
            <p><strong>Source:</strong> up to 16 .kmy files and 64 KiB of source total. Imports must refer to files in your submitted project.</p>
            <p><strong>Shared queue:</strong> one active job, up to four waiting, and a 20-second queue timeout. Busy requests may be rejected.</p>
            <p><strong>Compiler and replay:</strong> up to 10 seconds each for native compilation and linking, and up to 8 MiB for the response stream. After Submit, open Compile / ASM to see up to 256 KiB of generated assembly.</p>
            <p><strong>Execution:</strong> the hosted deployment compiles but does not run your program. In trusted run mode, defaults are 4 MiB of live KMY allocations, a 4 MiB main-thread stack, 2 seconds of runtime, and 64 KiB of captured output.</p>
          </section>
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
