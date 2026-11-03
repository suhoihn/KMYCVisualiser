package dev.kmy.visualiser.analysis

import com.fasterxml.jackson.databind.JsonNode
import com.fasterxml.jackson.databind.ObjectMapper
import org.springframework.http.HttpStatus
import org.springframework.context.annotation.Profile
import org.springframework.stereotype.Service
import org.springframework.web.server.ResponseStatusException
import java.nio.charset.StandardCharsets
import java.nio.file.Files
import java.nio.file.Path
import java.util.concurrent.CompletableFuture
import java.util.concurrent.TimeUnit

@Service
@Profile("legacy-analysis")
class CompilerAnalysisService(
    private val properties: CompilerProperties,
    private val objectMapper: ObjectMapper,
) {
    fun analyse(source: String): JsonNode {
        if (source.length > properties.maxSourceLength) {
            throw ResponseStatusException(
                HttpStatus.PAYLOAD_TOO_LARGE,
                "Source is longer than ${properties.maxSourceLength} characters",
            )
        }

        val compiler = Path.of(properties.path).toAbsolutePath().normalize()
        if (!Files.isRegularFile(compiler)) {
            throw ResponseStatusException(
                HttpStatus.SERVICE_UNAVAILABLE,
                "KMY compiler was not found at $compiler",
            )
        }

        val sourceFile = Files.createTempFile("kmy-visualiser-", ".kmy")
        try {
            Files.writeString(sourceFile, source, StandardCharsets.UTF_8)
            val process = ProcessBuilder(
                compiler.toString(),
                sourceFile.toString(),
                "--frontend-json",
            ).start()

            // Drain both streams while the compiler runs so a large AST cannot fill an OS pipe
            // and prevent the timeout from being reached.
            val stdoutFuture = CompletableFuture.supplyAsync {
                process.inputStream.bufferedReader(StandardCharsets.UTF_8).use { it.readText() }
            }
            val stderrFuture = CompletableFuture.supplyAsync {
                process.errorStream.bufferedReader(StandardCharsets.UTF_8).use { it.readText() }
            }
            val completed = process.waitFor(properties.timeout.toMillis(), TimeUnit.MILLISECONDS)
            if (!completed) {
                process.destroyForcibly()
                throw ResponseStatusException(HttpStatus.GATEWAY_TIMEOUT, "KMY analysis timed out")
            }

            val stdout = stdoutFuture.get(1, TimeUnit.SECONDS)
            val stderr = stderrFuture.get(1, TimeUnit.SECONDS)

            if (stdout.isBlank()) {
                throw ResponseStatusException(
                    HttpStatus.BAD_GATEWAY,
                    "KMY compiler returned no JSON${stderr.takeIf { it.isNotBlank() }?.let { ": $it" } ?: ""}",
                )
            }

            return try {
                objectMapper.readTree(stdout)
            } catch (error: Exception) {
                throw ResponseStatusException(
                    HttpStatus.BAD_GATEWAY,
                    "KMY compiler returned invalid JSON: ${error.message}",
                    error,
                )
            }
        } finally {
            Files.deleteIfExists(sourceFile)
        }
    }
}
