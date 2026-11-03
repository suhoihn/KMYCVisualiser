package dev.kmy.visualiser.analysis

import com.fasterxml.jackson.databind.JsonNode
import jakarta.validation.Valid
import org.springframework.boot.context.properties.EnableConfigurationProperties
import org.springframework.context.annotation.Profile
import org.springframework.web.bind.annotation.CrossOrigin
import org.springframework.web.bind.annotation.PostMapping
import org.springframework.web.bind.annotation.RequestBody
import org.springframework.web.bind.annotation.RequestMapping
import org.springframework.web.bind.annotation.RestController

@RestController
@Profile("legacy-analysis")
@RequestMapping("/api")
@CrossOrigin(originPatterns = ["http://localhost:*", "http://127.0.0.1:*"])
@EnableConfigurationProperties(CompilerProperties::class)
class AnalysisController(
    private val compilerAnalysisService: CompilerAnalysisService,
) {
    @PostMapping("/analysis")
    fun analyse(@Valid @RequestBody request: AnalysisRequest): JsonNode =
        compilerAnalysisService.analyse(request.source)
}
