import type { AnalysisResult, KmyToken } from './types'

export interface ModuleImport {
  alias: string
  requestedPath: string
  targetPath: string | null
  line: number
}

export const mainFile = 'main.kmy'

export function validFileName(path: string) {
  return /^(?:[A-Za-z0-9_-]+\/)*[A-Za-z0-9_-]+\.kmy$/.test(path)
}

export function resolveModulePath(importer: string, requested: string): string | null {
  if (requested.startsWith('/') || requested.includes('\\')) return null
  const parts = importer.split('/').slice(0, -1)
  for (const part of requested.split('/')) {
    if (part === '..') {
      if (!parts.length) return null
      parts.pop()
    } else if (part !== '.' && part) {
      parts.push(part)
    }
  }
  const resolved = parts.join('/')
  return validFileName(resolved) ? resolved : null
}

export function importsFromTokens(tokens: KmyToken[], importer: string): ModuleImport[] {
  const imports: ModuleImport[] = []
  for (let index = 0; index < tokens.length - 3; index += 1) {
    if (tokens[index].type !== 'KeywordImport') continue
    const path = tokens[index + 1]
    const as = tokens[index + 2]
    const alias = tokens[index + 3]
    if (path.type !== 'StringLiteral' || as.type !== 'KeywordAs' || alias.type !== 'Identifier') continue
    imports.push({
      alias: alias.lexeme,
      requestedPath: path.lexeme,
      targetPath: resolveModulePath(importer, path.lexeme),
      line: tokens[index].span.start.line,
    })
  }
  return imports
}

export function missingModuleImports(results: Record<string, AnalysisResult>, files: Record<string, string>) {
  return Object.entries(results).flatMap(([path, result]) =>
    importsFromTokens(result.tokens, path)
      .filter((item) => !item.targetPath || !Object.hasOwn(files, item.targetPath))
      .map((item) => ({ ...item, importer: path })))
}
