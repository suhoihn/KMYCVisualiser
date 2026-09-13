package dev.kmy.visualiser.analysis

import com.fasterxml.jackson.module.kotlin.jacksonObjectMapper
import org.junit.jupiter.api.Test
import org.junit.jupiter.api.assertThrows
import org.springframework.http.HttpStatus
import org.springframework.web.server.ResponseStatusException
import kotlin.test.assertEquals

class CompilerAnalysisServiceTest {
    @Test
    fun `rejects source over configured limit`() {
        val service = CompilerAnalysisService(
            CompilerProperties(path = "missing", maxSourceLength = 4),
            jacksonObjectMapper(),
        )

        val error = assertThrows<ResponseStatusException> {
            service.analyse("12345")
        }

        assertEquals(HttpStatus.PAYLOAD_TOO_LARGE, error.statusCode)
    }
}
