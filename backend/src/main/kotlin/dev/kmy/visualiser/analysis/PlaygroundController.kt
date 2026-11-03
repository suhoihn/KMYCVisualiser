package dev.kmy.visualiser.analysis

import com.fasterxml.jackson.databind.ObjectMapper
import jakarta.servlet.http.HttpServletRequest
import org.springframework.beans.factory.annotation.Value
import org.springframework.http.HttpStatus
import org.springframework.http.ResponseEntity
import org.springframework.web.bind.annotation.CrossOrigin
import org.springframework.web.bind.annotation.PostMapping
import org.springframework.web.bind.annotation.RestController
import org.springframework.web.server.ResponseStatusException
import org.springframework.web.servlet.mvc.method.annotation.StreamingResponseBody
import java.net.HttpURLConnection
import java.net.URI

@RestController
@CrossOrigin(originPatterns = ["http://localhost:*", "http://127.0.0.1:*"])
class PlaygroundController(
    private val mapper: ObjectMapper,
    @Value("\${kmy.runner-url:http://127.0.0.1:8090}") private val runnerUrl: String,
) {
    @PostMapping("/api/playground", produces = ["application/x-ndjson"])
    fun submit(request: HttpServletRequest): ResponseEntity<StreamingResponseBody> {
        val payload = request.inputStream.readNBytes(131073)
        if (payload.size > 131072) throw ResponseStatusException(HttpStatus.PAYLOAD_TOO_LARGE, "Project request exceeds 128 KiB")
        try {
            val json = mapper.readTree(payload)
            if (json == null || !json.path("files").isObject) {
                throw ResponseStatusException(HttpStatus.BAD_REQUEST, "Submit a files map and an entryFile")
            }
        } catch (error: com.fasterxml.jackson.core.JsonProcessingException) {
            throw ResponseStatusException(HttpStatus.BAD_REQUEST, "Invalid project JSON")
        }
        val upstream = URI.create("${runnerUrl.trimEnd('/')}/jobs").toURL().openConnection() as HttpURLConnection
        try {
            upstream.requestMethod = "POST"
            upstream.connectTimeout = 3000
            upstream.readTimeout = 50000
            upstream.doOutput = true
            upstream.setRequestProperty("Content-Type", "application/json")
            upstream.setRequestProperty("X-Client-IP", request.getHeader("X-Real-IP") ?: request.remoteAddr)
            upstream.setFixedLengthStreamingMode(payload.size)
            upstream.outputStream.use { it.write(payload) }
            if (upstream.responseCode != 200) {
                val status = upstream.responseCode
                upstream.disconnect()
                throw ResponseStatusException(HttpStatus.valueOf(status), when (status) {
                    429 -> "Playground busy or submission limit reached; try again shortly"
                    400 -> "Invalid project paths, source size, or entry file"
                    413 -> "Project request is too large"
                    else -> "Runner could not start the job"
                })
            }
        } catch (error: java.io.IOException) {
            upstream.disconnect()
            throw ResponseStatusException(HttpStatus.SERVICE_UNAVAILABLE, "Native runner is unavailable on $runnerUrl", error)
        }
        val stream = StreamingResponseBody { output ->
            try {
                upstream.inputStream.use { input ->
                    val buffer = ByteArray(8192)
                    var total = 0
                    while (true) {
                        val count = input.read(buffer)
                        if (count < 0) break
                        total += count
                        if (total > 8 * 1024 * 1024) throw java.io.IOException("Recording exceeds 8 MiB")
                        output.write(buffer, 0, count)
                        output.flush()
                    }
                }
            } catch (error: java.io.IOException) {
                val event = mapper.writeValueAsBytes(mapOf("type" to "error", "message" to "Runner connection ended: ${error.message}"))
                try { output.write(event); output.write(10); output.flush() } catch (_: java.io.IOException) { }
            } finally {
                upstream.disconnect()
            }
        }
        return ResponseEntity.ok().header("X-Accel-Buffering", "no").body(stream)
    }
}
