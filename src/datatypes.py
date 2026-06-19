from dataclasses import dataclass

@dataclass(frozen=True)
class MarketParams:
    S0: float
    r: float
    sigma: float

@dataclass(frozen=True)
class EuropeanOptionParams:
    K: float
    T: float

@dataclass(frozen=True)
class AsianOptionParams:
    K: float
    T: float

@dataclass(frozen=True)
class BarrierOptionParams:
    K: float
    T: float
    B: float

@dataclass(frozen=True)
class SimulationConfig:
    n_sims: int
    n_steps: int | None=None
    seed: int | None=None
    chunk_size: int = 50_000
    n_workers: int = 4
    antithetic: bool = True

@dataclass(frozen=True)
class PricingResult:
    price: float
    standard_error: float | None
    confidence_interval: tuple[float, float] | None
    n_sims: int 
    method: str
    seed: int | None