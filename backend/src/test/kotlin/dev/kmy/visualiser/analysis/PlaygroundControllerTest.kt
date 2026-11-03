package dev.kmy.visualiser.analysis

import com.fasterxml.jackson.module.kotlin.jacksonObjectMapper
import com.sun.net.httpserver.HttpServer
import org.junit.jupiter.api.Test
import org.junit.jupiter.api.assertThrows
import org.springframework.http.HttpStatus
import org.springframework.mock.web.MockHttpServletRequest
import org.springframework.web.server.ResponseStatusException
import java.io.ByteArrayOutputStream
import java.net.InetSocketAddress
import kotlin.test.assertEquals

class PlaygroundControllerTest {
    @Test
    fun `forwards analysis and native results without parsing the language`() {
        val stream = "{\"type\":\"phase\",\"phase\":\"compile\"}\n{\"type\":\"result\",\"status\":\"success\"}\n"
        val runner = HttpServer.create(InetSocketAddress("127.0.0.1", 0), 0)
        runner.createContext("/jobs") { exchange ->
            val payload = jacksonObjectMapper().readTree(exchange.requestBody)
            assertEquals("print(1 + 2);", payload.path("files").path("main.kmy").asText())
            exchange.sendResponseHeaders(200, 0)
            exchange.responseBody.use { it.write(stream.toByteArray()) }
        }
        runner.start()
        try {
            val controller = PlaygroundController(jacksonObjectMapper(), "http://127.0.0.1:${runner.address.port}")
            val request = MockHttpServletRequest().apply {
                setContent("{\"files\":{\"main.kmy\":\"print(1 + 2);\"},\"entryFile\":\"main.kmy\"}".toByteArray())
            }
            val response = controller.submit(request)
            val output = ByteArrayOutputStream()
            response.body!!.writeTo(output)
            assertEquals(stream, output.toString(Charsets.UTF_8))
        } finally { runner.stop(0) }
    }

    @Test
    fun `rejects oversized HTTP bodies before decoding JSON`() {
        val controller = PlaygroundController(jacksonObjectMapper(), "http://127.0.0.1:1")
        val request = MockHttpServletRequest().apply { setContent(ByteArray(131073)) }
        val error = assertThrows<ResponseStatusException> { controller.submit(request) }
        assertEquals(HttpStatus.PAYLOAD_TOO_LARGE, error.statusCode)
    }
}
