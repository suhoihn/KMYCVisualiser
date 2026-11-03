// Run against the real local Spring + Linux runner: node --experimental-strip-types scripts/check-replay.mjs
import assert from 'node:assert/strict'
import { submitPlayground } from '../src/api.ts'
import { buildWalkthrough, parserSnapshot, stageStart } from '../src/walkthrough.ts'

const nativeFetch = globalThis.fetch
globalThis.fetch = (url, options) => nativeFetch(new URL(url, process.env.KMY_TEST_API ?? 'http://127.0.0.1:8080'), options)
const events = []
await submitPlayground({ 'main.kmy': 'print(1 + 2 * 3);' }, 'main.kmy', new AbortController().signal, event => events.push(event))
const result = events.find(event => event.type === 'analysis').analysis
const compile = events.find(event => event.type === 'compilation').compilation
assert.match(compile.assembly.text, /\.intel_syntax noprefix/)
assert.equal(compile.assembly.truncated, false)
const execution = events.find(event => event.type === 'execution').execution
assert.equal(execution.stdout, '7\n')
assert.equal(events.at(-1).status, 'success')
assert.ok(events.findIndex(event => event.type === 'analysis') < events.findIndex(event => event.type === 'compilation'))
const timeline = buildWalkthrough(result, compile, execution)
assert.equal(timeline.at(-1).stage, 'execution')
assert.equal(timeline[stageStart(timeline, 'compile')].process.exitCode, 0)
const parser = result.parser.events
const original = JSON.stringify(parser)
const firstNode = parser.findIndex(event => event.kind === 'createNode')
const before = parserSnapshot(parser, firstNode)
const after = parserSnapshot(parser, firstNode + 1)
assert.equal(before.nodeCount, 0)
assert.equal(after.nodeCount, 1)
assert.equal(after.forest.children[0].node.kind, 'Literal')
assert.ok(after.frames.some(frame => frame.rule === 'parse_expression'))
assert.ok(after.focusPath)
const complete = parserSnapshot(parser, parser.length)
assert.equal(complete.frames.length, 0)
assert.equal(complete.forest.children.length, 1)
assert.equal(complete.forest.children[0].node.kind, 'Program')
assert.deepEqual(complete.forest.children[0].node, result.ast)
// Seek backward after completing playback; no future nodes or calls can leak in.
assert.deepEqual(parserSnapshot(parser, firstNode + 1), after)
assert.equal(parserSnapshot(parser, 0).nodeCount, 0)
assert.equal(JSON.stringify(parser), original)
console.log(`PASS live native stream: ${result.tokens.length} tokens, ${parser.length} parser events, ${complete.nodeCount} nodes, stdout=7`)
console.log('PASS call stack, causal partial AST, final AST identity, rewind, and native stage selection')

const modules = []
await submitPlayground({ 'main.kmy': 'import "lib/math.kmy" as math; print(math::twice(4));',
  'lib/math.kmy': 'fun twice(value: int): int { return value * 2; }' }, 'main.kmy', new AbortController().signal, event => modules.push(event))
assert.equal(modules.find(event => event.type === 'execution').execution.stdout, '8\n')
for (const event of modules.filter(event => event.type === 'analysis')) {
  const snapshot = parserSnapshot(event.analysis.parser.events, event.analysis.parser.events.length)
  assert.deepEqual(snapshot.forest.children[0].node, event.analysis.ast)
}
console.log('PASS per-file module replay and function parameter nodes match final ASTs')
