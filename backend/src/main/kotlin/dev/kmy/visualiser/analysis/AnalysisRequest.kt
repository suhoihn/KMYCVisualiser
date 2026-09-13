package dev.kmy.visualiser.analysis

import jakarta.validation.constraints.NotNull

data class AnalysisRequest(
    @field:NotNull
    val source: String,
)

