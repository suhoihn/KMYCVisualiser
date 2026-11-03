package dev.kmy.visualiser.analysis

import jakarta.servlet.http.HttpServletRequest
import org.springframework.http.HttpStatus
import org.springframework.http.ResponseEntity
import org.springframework.web.bind.annotation.CrossOrigin
import org.springframework.web.bind.annotation.GetMapping
import org.springframework.web.bind.annotation.PostMapping
import org.springframework.web.bind.annotation.RestController
import org.springframework.web.server.ResponseStatusException
import org.springframework.web.servlet.mvc.method.annotation.StreamingResponseBody

@RestController
@CrossOrigin(originPatterns = ["http://localhost:*", "http://127.0.0.1:*"])
class PlaygroundController(private val relay: PlaygroundRelayService) {
    @GetMapping("/api/health")
    fun health(): Map<String, String> = mapOf("status" to "ok")

    @PostMapping("/api/playground", produces = ["application/x-ndjson"])
    fun submit(request: HttpServletRequest): ResponseEntity<StreamingResponseBody> {
        val payload = request.inputStream.readNBytes(131073)
        if (payload.size > 131072) throw ResponseStatusException(HttpStatus.PAYLOAD_TOO_LARGE, "Project request exceeds 128 KiB")
        val clientIp = request.getHeader("X-Real-IP") ?: request.remoteAddr
        return ResponseEntity.ok().header("X-Accel-Buffering", "no").body(relay.relay(payload, clientIp))
    }
}
