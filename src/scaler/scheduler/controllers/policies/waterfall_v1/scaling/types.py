import dataclasses


@dataclasses.dataclass(frozen=True)
class WaterfallRule:
    """A single rule in the waterfall config, parsed from 'priority,adapter_id_prefix,max_workers'."""

    priority: int
    adapter_id_prefix: bytes
    max_workers: int
