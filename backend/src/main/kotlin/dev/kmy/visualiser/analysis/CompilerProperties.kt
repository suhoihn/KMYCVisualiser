package dev.kmy.visualiser.analysis

import org.springframework.boot.context.properties.ConfigurationProperties
import java.time.Duration

@ConfigurationProperties("kmy.compiler")
data class CompilerProperties(
    val path: String,
    val timeout: Duration = Duration.ofSeconds(5),
    val maxSourceLength: Int = 100_000,
)

