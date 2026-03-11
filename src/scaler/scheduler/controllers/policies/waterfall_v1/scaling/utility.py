from typing import List

from scaler.scheduler.controllers.policies.waterfall_v1.scaling.types import WaterfallRule


def parse_waterfall_rules(policy_content: str) -> List[WaterfallRule]:
    """Parse waterfall rules from policy_content.

    Expected format (one rule per line, ``#`` comments supported)::

        #priority,adapter_id_prefix,max_workers
        1,adapter_a,10
        2,adapter_b,20

    Raises ``ValueError`` on malformed input.
    """
    rules: List[WaterfallRule] = []
    for line_number, raw_line in enumerate(policy_content.splitlines(), start=1):
        # Strip inline comments
        line = raw_line.split("#", 1)[0].strip()
        if not line:
            continue

        parts = [p.strip() for p in line.split(",")]
        if len(parts) != 3:
            raise ValueError(
                f"waterfall_v1 policy_content line {line_number}: "
                f"expected 'priority,adapter_id_prefix,max_workers', got {raw_line.strip()!r}"
            )

        raw_priority, adapter_id_prefix, raw_max_workers = parts

        if not adapter_id_prefix:
            raise ValueError(f"waterfall_v1 policy_content line {line_number}: adapter_id_prefix cannot be empty")

        rules.append(
            WaterfallRule(
                priority=int(raw_priority),
                adapter_id_prefix=adapter_id_prefix.encode(),
                max_workers=int(raw_max_workers),
            )
        )

    if not rules:
        raise ValueError("waterfall_v1 policy_content: no rules specified")

    return rules
