import dataclasses


@dataclasses.dataclass(frozen=True)
class WaterfallRule:
    """A single rule in the waterfall config, parsed from 'priority,manager_id,max_workers'."""

    priority: int
    worker_manager_id: bytes
    max_workers: int
